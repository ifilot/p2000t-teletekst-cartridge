#!/usr/bin/env python3
"""Boot SLOT1+SLOT2, join fictitious Wi-Fi, and render NOS page 100."""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
EMU=ROOT/'emulator'
with tempfile.TemporaryDirectory(prefix='p2000t-emulator-') as directory:
    temp=Path(directory); build=temp/'build'; monitor=temp/'monitor.bin'; intro_screen=temp/'intro-screen.bin'; intro_hidden_screen=temp/'intro-hidden-screen.bin'; source_screen=temp/'source-screen.bin'; screen=temp/'screen.bin'; p2000_screen=temp/'p2000-screen.bin'; legacy_warning_screen=temp/'legacy-warning-screen.bin'; legacy_screen=temp/'legacy-screen.bin'; legacy_clock_screen=temp/'legacy-clock-screen.bin'; legacy_p2000_screen=temp/'legacy-p2000-screen.bin'; incompatible_screen=temp/'incompatible-screen.bin'; frame=temp/'frame.bin'
    subprocess.run(['make','-C',str(ROOT/'src')],check=True)
    subprocess.run(['cmake','-S',str(EMU),'-B',str(build),'-G','Ninja'],check=True)
    subprocess.run(['cmake','--build',str(build)],check=True)
    subprocess.run(['python3',str(EMU/'tests/make_monitor.py'),str(monitor)],check=True)
    subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(EMU/'tests/fixtures/nos-100.json'),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--frames','4','--dump-screen',str(intro_screen)],check=True)
    intro=intro_screen.read_bytes()
    assert intro[0:2] == bytes((0x04,0x1d))
    assert intro[40:42] == bytes((0x04,0x1d))
    assert intro[2*40+7:2*40+33] == b'P2000T  INTERNET TELETEKST'
    assert intro[3*40:3*40+3] == bytes((0x04,0x1d,0x17))
    assert intro[3*40+8:3*40+10] == bytes((0x20,0x70))
    assert intro[5*40+13:5*40+15] == bytes((0x7f,0x7f))
    assert intro[7*40+15:7*40+17] == bytes((0x7f,0x7f))
    p2000_mosaics=intro[3*40:10*40]
    assert any(value not in (0x04,0x1d,0x17,0x20,0x7f)
               for value in p2000_mosaics)
    assert intro[9*40+3] == 0x3c
    assert intro[10*40:10*40+4] == bytes((0x04,0x1d,0x17,0x35))
    assert intro[17*40+8:17*40+31] == b'UW VENSTER OP DE WERELD'
    assert intro[18*40+8:18*40+31] == b'NOS EN P2000T TELETEKST'
    assert intro[19*40+5:19*40+35] == b'ORIGINEEL SAA5050-MOZAIEKBEELD'
    assert intro[20*40+5:20*40+35] == b'KLASSIEK BEELD, ACTUEEL NIEUWS'
    assert intro[22*40+11:22*40+28] == b'DRUK OP EEN TOETS'
    assert intro[23*40:23*40+3] == bytes((0x04,0x1d,0x07))
    assert intro[23*40+3:23*40+29] == b'P2000T Teletekst Cartridge'
    assert intro[23*40+29:23*40+34] == b'     '
    assert intro[23*40+34:24*40] == b'v0.4.0'
    subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(EMU/'tests/fixtures/nos-100.json'),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--frames','30','--dump-screen',str(intro_hidden_screen)],check=True)
    intro_hidden=intro_hidden_screen.read_bytes()
    assert intro_hidden[22*40:23*40] == b' '*40
    subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(EMU/'tests/fixtures/nos-100.json'),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--auto','--frames','119','--dump-screen',str(source_screen)],check=True)
    source=source_screen.read_bytes()
    assert source[5*40+5:5*40+22] == b'1 - NOS TELETEKST'
    assert source[6*40+5:6*40+25] == b'2 - P2000T TELETEKST'
    assert source[12*40+5:12*40+25] == b'P - PAUZE / DOORGAAN'
    assert source[13*40+5:13*40+25] == b'S - SUBPAGINA KIEZEN'
    assert source[14*40+5:14*40+30] == b'W - WIFI-NETWERK WIJZIGEN'
    assert source[15*40+5:15*40+19] == b'H - HULP TONEN'
    assert source[16*40+5:16*40+32] == b'STOP - ANDERE TELETEKSTBRON'
    subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(EMU/'tests/fixtures/nos-100.json'),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--auto','--p2wp-version','2','--frames','20',
        '--dump-screen',str(legacy_warning_screen)],check=True)
    legacy_warning=legacy_warning_screen.read_bytes()
    assert b'COMPATIBILITEITSMODUS ACTIEF' in legacy_warning
    assert b'P2WP/2 VERBINDING GEVONDEN' in legacy_warning
    subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(EMU/'tests/fixtures/nos-100.json'),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--auto','--p2wp-version','1','--frames','20',
        '--dump-screen',str(incompatible_screen)],check=True)
    incompatible=incompatible_screen.read_bytes()
    assert b'PROTOCOL NIET COMPATIBEL' in incompatible
    assert b'GEEN GEDEELDE P2WP-VERSIE' in incompatible
    subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(EMU/'tests/fixtures/nos-100.json'),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--auto',
        '--frames','650','--dump-screen',str(screen),'--dump-frame',str(frame)],check=True)
    display=screen.read_bytes()
    assert len(display)==960
    assert b'NOS Telet' in display, 'production ROM did not render the NOS header'
    assert b'Meer bevoegdheden' in display, 'production ROM did not render NOS page content'
    assert b'PICO W NIET GEVONDEN' not in display
    assert display[0] == 0x03
    assert display[1:3] in (b'zo',b'ma',b'di',b'wo',b'do',b'vr',b'za')
    assert display[3] == ord(' ') and display[6] == ord('.')
    assert display[7:10] in (b'jan',b'feb',b'mrt',b'apr',b'mei',b'jun',
                             b'jul',b'aug',b'sep',b'okt',b'nov',b'dec')
    assert display[10] == ord(' ')
    assert display[13] == ord(':') and display[16] == ord(':')
    assert all(display[index:index+1].isdigit()
               for index in (4,5,11,12,14,15,17,18))
    pixels=frame.read_bytes()
    assert len(pixels)==720*720*4
    colours={pixels[i:i+4] for i in range(0,len(pixels),4)}
    assert len(colours)>=4, 'SAA5050 renderer produced a blank framebuffer'
    subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(EMU/'tests/fixtures/nos-100.json'),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--auto','--auto-source','2',
        '--frames','650','--dump-screen',str(p2000_screen)],check=True)
    p2000_display=p2000_screen.read_bytes()
    assert p2000_display[13] in (ord(':'),ord(' '))
    assert all(p2000_display[index:index+1].isdigit()
               for index in (4,5,11,12,14,15))
    subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(EMU/'tests/fixtures/nos-100.json'),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--auto','--p2wp-version','2','--frames','700',
        '--dump-screen',str(legacy_screen)],check=True)
    legacy=legacy_screen.read_bytes()
    assert b'NOS Telet' in legacy and b'Meer bevoegdheden' in legacy
    assert legacy[0] != 0x03, 'P2WP/2 must preserve the provider header without a date clock'
    subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(EMU/'tests/fixtures/nos-100.json'),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--auto','--p2wp-version','2','--p2wp-status-length','9',
        '--frames','300','--dump-screen',str(legacy_clock_screen)],check=True)
    legacy_clock=legacy_clock_screen.read_bytes()
    assert legacy_clock[0] == 0x03 and legacy_clock[3] == ord(':')
    assert all(legacy_clock[index:index+1].isdigit() for index in (1,2,4,5,7,8))
    subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(EMU/'tests/fixtures/nos-100.json'),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--auto','--auto-source','2','--p2wp-version','2',
        '--frames','700','--dump-screen',str(legacy_p2000_screen)],check=True)
    legacy_p2000=legacy_p2000_screen.read_bytes()
    assert b'NOS Telet' in legacy_p2000 and b'Meer bevoegdheden' in legacy_p2000
print('P2WP/2 compatibility + P2WP/3 NOS/P2000T + incompatible warning: passed')
