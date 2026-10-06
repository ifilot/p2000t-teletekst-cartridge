"""Regression checks for the ROM/RAM linker boundary and protected stack budget."""

from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from make_cartridge_rom import validate_runtime
from prepare_runtime import select_functions


@pytest.fixture
def layout():
    code = b"\x5e" + bytes(255)
    data = b"\0\xff\xff"
    runtime = bytes(range(256)) * 16
    symbols = {
        "RAM_Start": 0x7000, "Runtime_Start": 0x8000, "Stack_Top": 0x9FF0,
        "__BSS_head": 0x7000, "__BSS_END_tail": 0x7FC3,
        "__DATA_head": 0x7FC0, "__DATA_END_tail": 0x7FC3,
        "__ROMABLE_END_tail": 0x1100,
        "__runtime_code_head": 0x8000, "__runtime_code_tail": 0x9000,
        "__runtime_code_size": 4096,
        "_main": 0x1050, "_lz4_decompress": 0x1060,
        "asm_dzx0_standard": 0x1070, "unpack_runtime": 0x1080,
        "_choose_source": 0x1090, "_p2wp_request": 0x10A0,
        "_fetch_page": 0x8000, "_viewer_loop": 0x8500, "_teletekst_start": 0x8F00,
    }
    return dict(
        code=code, data=data, runtime=runtime, packed=b"packed",
        decoded=runtime, symbols=symbols,
        functions=["fetch_page", "viewer_loop", "teletekst_start"],
    )


def test_valid_layout_reports_stack_and_growth_separately(layout):
    report = validate_runtime(**layout)
    assert report["stack_reserve"] == 1024
    assert report["ram_growth_budget"] == 3056
    assert report["mutable_ram_bytes"] == 4035


def test_runtime_cannot_consume_stack_reserve(layout):
    runtime = bytes(0x1BF1)
    layout.update(runtime=runtime, decoded=runtime)
    layout["symbols"].update(__runtime_code_tail=0x9BF1, __runtime_code_size=len(runtime))
    with pytest.raises(ValueError, match="stack reserve"):
        validate_runtime(**layout)


@pytest.mark.parametrize("symbol,value,error", [
    ("Stack_Top", 0xDFF0, "16 KiB"),
    ("__BSS_END_tail", 0x8001, "overlaps"),
    ("__runtime_code_head", 0x8100, "origin"),
    ("__runtime_code_tail", 0x9001, "length"),
    ("__runtime_code_size", 4095, "section size"),
    ("__ROMABLE_END_tail", 0x1101, "loader address"),
    ("__DATA_head", 0x7FC1, "DATA length"),
    ("_fetch_page", 0x1080, "not linked"),
    ("asm_dzx0_standard", 0x8100, "resident"),
])
def test_inconsistent_or_unsafe_link_maps_are_rejected(layout, symbol, value, error):
    layout["symbols"][symbol] = value
    with pytest.raises(ValueError, match=error):
        validate_runtime(**layout)


def test_wrong_or_stale_compressed_runtime_is_rejected(layout):
    layout["decoded"] = layout["runtime"][:-1] + b"\0"
    with pytest.raises(ValueError, match="round-trip"):
        validate_runtime(**layout)


def test_cartridge_overflow_is_rejected(layout):
    layout["packed"] = bytes(16384)
    with pytest.raises(ValueError, match="cartridge limit"):
        validate_runtime(**layout)


def test_missing_manifest_and_map_symbols_are_rejected(layout):
    layout["functions"] = []
    with pytest.raises(ValueError, match="manifest"):
        validate_runtime(**layout)
    layout["symbols"].pop("unpack_runtime")
    with pytest.raises(ValueError, match="Missing layout"):
        validate_runtime(**layout)


def test_generated_function_headers_define_the_core(tmp_path):
    assembly = tmp_path / "viewer.asm"
    source = """SECTION code_compiler
; Function show_help
_show_help:
    ret
; Function expected_status_length
_expected_status_length:
    ret
SECTION rodata_compiler
___str_0:
    defm "stay in ROM"
SECTION code_compiler
; Function fetch_page
_fetch_page:
    call _show_help
    ret
; Function viewer_loop
_viewer_loop:
    call _fetch_page
    ret
; Function teletekst_start
_teletekst_start:
    ret
"""
    assembly.write_text(source)
    selected = select_functions(assembly)
    assert selected == {"expected_status_length", "fetch_page", "viewer_loop", "teletekst_start"}
    output = assembly.read_text()
    assert "SECTION code_compiler\n_show_help:" in output
    assert "SECTION runtime_code\n_fetch_page:" in output
    assert "SECTION rodata_compiler\n___str_0:" in output
    # Placement changes preserve every instruction and literal verbatim.
    assert [line for line in output.splitlines() if not line.startswith("SECTION")] == [
        line for line in source.splitlines() if not line.startswith("SECTION")
    ]


def test_changed_core_boundary_requires_review(tmp_path):
    assembly = tmp_path / "viewer.asm"
    assembly.write_text("; Function other\n_other:\n    ret\n")
    with pytest.raises(ValueError, match="boundaries changed"):
        select_functions(assembly)
