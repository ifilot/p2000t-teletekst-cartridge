# Adapt the selected chip's SDK linker script instead of duplicating its boot,
# flash and binary-info layout. Refuse SDK changes that need a fresh review.
function(p2wp_replace_linker_fragment variable before after)
    string(FIND "${${variable}}" "${before}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Pico SDK linker layout changed; review network_stack.cmake: ${before}")
    endif()
    string(REPLACE "${before}" "${after}" updated "${${variable}}")
    set(${variable} "${updated}" PARENT_SCOPE)
endfunction()

function(p2wp_configure_network_stack target stack_size)
    set(sdk_script "${PICO_LINKER_SCRIPT_PATH}/memmap_default.ld")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${sdk_script}")
    file(READ "${sdk_script}" layout)

    # Core 0 runs TLS. Its stack must fit beyond the SDK's 4 KiB scratch bank,
    # without growing down into core 1's mailbox stack in SCRATCH_X.
    p2wp_replace_linker_fragment(layout
        ".stack_dummy (NOLOAD):\n    {\n        KEEP(*(.stack*))\n    } > SCRATCH_Y"
        ".stack_dummy (ORIGIN(RAM) + LENGTH(RAM) - ${stack_size}) (NOLOAD):\n    {\n        KEEP(*(.stack*))\n    } > RAM")
    p2wp_replace_linker_fragment(layout
        "__StackTop = ORIGIN(SCRATCH_Y) + LENGTH(SCRATCH_Y);"
        "__StackTop = ORIGIN(RAM) + LENGTH(RAM);")

    # Both newlib's _sbrk and other supported libc allocators must stop below
    # the reserved stack. Reserving only the stack symbols is insufficient.
    p2wp_replace_linker_fragment(layout
        "__HeapLimit = ORIGIN(RAM) + LENGTH(RAM);"
        "__HeapLimit = ADDR(.stack_dummy);")
    p2wp_replace_linker_fragment(layout
        "__StackLimit = ORIGIN(RAM) + LENGTH(RAM);"
        "__StackLimit = ADDR(.stack_dummy);")

    string(APPEND layout "
ASSERT(SIZEOF(.stack_dummy) >= 8192, \"TLS network stack must reserve at least 8 KiB\");
ASSERT(__StackBottom == ADDR(.stack_dummy), \"Network stack reservation and startup symbols disagree\");
ASSERT(__StackTop == ADDR(.stack_dummy) + SIZEOF(.stack_dummy), \"Network stack exceeds its reservation\");
ASSERT(__end__ <= __StackBottom, \"Static data overlaps the network stack\");
ASSERT(__HeapLimit <= __StackBottom && __StackLimit <= __StackBottom, \"Heap overlaps the network stack\");
ASSERT(__StackTop <= __StackOneBottom || __StackBottom >= __StackOneTop, \"Network and mailbox stacks overlap\");
ASSERT(SIZEOF(.stack1_dummy) >= 4096, \"Mailbox stack must reserve at least 4 KiB\");
")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/p2wp_memmap.ld")
    file(GENERATE OUTPUT "${output}" CONTENT "${layout}")
    pico_set_linker_script(${target} "${output}")
endfunction()
