#!/usr/bin/env python3
"""Boot SLOT1+SLOT2, join fictitious Wi-Fi, and render NOS page 100."""
from pathlib import Path
import base64
import json
import hashlib
import subprocess
import sys
import tempfile
import time

ROOT=Path(__file__).resolve().parents[2]
EMU=ROOT/'emulator'
sys.path.insert(0,str(ROOT/'server'))
from server import page_response
with tempfile.TemporaryDirectory(prefix='p2000t-emulator-') as directory:
    temp=Path(directory); build=temp/'build'; monitor=temp/'monitor.bin'; intro_screen=temp/'intro-screen.bin'; intro_hidden_screen=temp/'intro-hidden-screen.bin'; intro_countdown_screen=temp/'intro-countdown-screen.bin'; source_screen=temp/'source-screen.bin'; screen=temp/'screen.bin'; custom_dialog_screen=temp/'custom-dialog-screen.bin'; custom_screen=temp/'custom-screen.bin'; restored_custom_screen=temp/'restored-custom-screen.bin'; emulated_flash=temp/'pico-flash.bin'; auto_flash=temp/'auto-flash.bin'; auto_screen=temp/'auto-screen.bin'; archive_screen=temp/'archive-screen.bin'; cancel_screen=temp/'cancel-screen.bin'; custom_concealed_screen=temp/'custom-concealed-screen.bin'; custom_revealed_screen=temp/'custom-revealed-screen.bin'; custom_conceal_fixture=temp/'custom-conceal.json'; zoom_screen=temp/'zoom-screen.bin'; reveal_fixture=temp/'reveal.json'; reveal_screen=temp/'reveal-screen.bin'; help_screen=temp/'help-screen.bin'; p2000_screen=temp/'p2000-screen.bin'; legacy_warning_screen=temp/'legacy-warning-screen.bin'; legacy_screen=temp/'legacy-screen.bin'; legacy_clock_screen=temp/'legacy-clock-screen.bin'; legacy_p2000_screen=temp/'legacy-p2000-screen.bin'; incompatible_screen=temp/'incompatible-screen.bin'; frame=temp/'frame.bin'; loop_fetches=temp/'loop-fetches.bin'; pause_fetches=temp/'pause-fetches.bin'; resume_fetches=temp/'resume-fetches.bin'; keypad_pages=temp/'keypad-pages.txt'; arrow_pages=temp/'arrow-pages.txt'; auto_pages=temp/'auto-pages.txt'
    cycle_screen=temp/'cycle-screen.bin'; archive_sources=temp/'archive-sources.bin'; legacy_archive_sources=temp/'legacy-archive-sources.bin'; auto_skip_pages=temp/'auto-skip-pages.txt'; timeout_screen=temp/'timeout-screen.bin'; legacy_timeout_screen=temp/'legacy-timeout-screen.bin'
    bundled_monitor=EMU/'assets/P2000ROM.bin'
    assert bundled_monitor.stat().st_size == 4096
    assert hashlib.sha256(bundled_monitor.read_bytes()).hexdigest() == \
        '351e0d3dcb9e39e0ed375bb0cb7debf7a6e8afcc93a3aa1f147dfdf68392dac8'
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
    assert b'UW VENSTER OP DE WERELD' in intro[17*40:18*40]
    assert b'NOS EN P2000T TELETEKST' in intro[18*40:19*40]
    assert b'ORIGINEEL SAA5050-MOZAIEKBEELD' in intro[19*40:20*40]
    assert b'KLASSIEK BEELD, ACTUEEL NIEUWS' in intro[20*40:21*40]
    assert intro[22*40+4:22*40+21] == b'DRUK OP EEN TOETS'
    assert intro[22*40+23:22*40+35] == b'AUTO-MODE 60'
    assert intro[23*40:23*40+3] == bytes((0x04,0x1d,0x07))
    assert intro[23*40+3:23*40+29] == b'P2000T Teletekst Cartridge'
    assert intro[23*40+29:23*40+34] == b'     '
    assert intro[23*40+34:24*40] == b'v0.5.0'
    subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(EMU/'tests/fixtures/nos-100.json'),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--frames','30','--dump-screen',str(intro_hidden_screen)],check=True)
    intro_hidden=intro_hidden_screen.read_bytes()
    assert intro_hidden[22*40+4:22*40+21] == b' '*17
    assert intro_hidden[22*40+23:22*40+35] == b'AUTO-MODE 60'
    subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(EMU/'tests/fixtures/nos-100.json'),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--frames','60','--dump-screen',str(intro_countdown_screen)],check=True)
    intro_countdown=intro_countdown_screen.read_bytes()
    assert intro_countdown[22*40+23:22*40+35] == b'AUTO-MODE 59'
    subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(EMU/'tests/fixtures/nos-100.json'),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--auto','--frames','149','--dump-screen',str(source_screen)],check=True)
    source=source_screen.read_bytes()
    assert b'1 - NOS TELETEKST' in source[5*40:6*40]
    assert b'2 - P2000T TELETEKST' in source[6*40:7*40]
    assert b'3 - TELETEKSTARCHIEF.NL' in source[7*40:8*40]
    assert b'0 - EIGEN SERVER' in source[8*40:9*40]
    assert b'A AUTOSTART NA 60S: UIT' in source[11*40:12*40]
    assert b'START/I INDEX' in source and b'R ONTHUL' in source and b'Z ZOOM' in source
    assert b'<-/P VORIGE' in source and b'->/N VOLGENDE' in source
    assert b'A PAUZE/DOORGAAN' in source and b'S SUBPAGINA' in source
    assert b'V AUTO-PAGINA' in source and b'W WIFI' in source and b'H HULP' in source
    assert b'STOP - ANDERE TELETEKSTBRON' in source
    assert b'CARTRIDGE: v0.5.0 / PICO v0.5.0' in source[18*40:19*40]
    assert b'LAATSTE VERSIE ONLINE: v0.5.0' in source[19*40:20*40]
    common=[str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(EMU/'tests/fixtures/nos-100.json'),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--auto']
    subprocess.run(common+['--auto-source-cycles','4','--frames','198',
        '--dump-screen',str(cycle_screen)],check=True)
    cycle=cycle_screen.read_bytes()
    assert b'A AUTOSTART NA 60S: EIGEN' in cycle[11*40:12*40]
    assert b'EIGENEF' not in cycle and b'UITENEF' not in cycle, \
        'shorter auto-start labels left characters from the previous value'
    subprocess.run(common+['--auto-source','3','--frames','650',
        '--dump-screen',str(archive_screen),'--dump-sources',str(archive_sources)],check=True)
    assert b'Meer bevoegdheden' in archive_screen.read_bytes(), \
        'teletekstarchief.nl source did not use its compatible /json endpoint'
    assert archive_sources.read_bytes()[0] == 3, \
        'P2WP/7 archive selection did not use the verified built-in source'
    subprocess.run(common+['--auto-source','3','--p2wp-version','6','--frames','650',
        '--dump-sources',str(legacy_archive_sources)],check=True)
    assert legacy_archive_sources.read_bytes()[0] == 2, \
        'P2WP/6 archive compatibility did not use the custom-source fallback'
    subprocess.run(common+['--auto-keys','KP1,KP0,BACKSPACE,KP0,KP1',
        '--frames','850','--dump-pages',str(keypad_pages)],check=True)
    assert keypad_pages.read_text().splitlines()[:2] == ['100','101'], \
        'numeric keypad entry or Backspace editing did not select page 101'
    subprocess.run(common+['--auto-key','RIGHT','--frames','750',
        '--dump-pages',str(arrow_pages)],check=True)
    assert arrow_pages.read_text().splitlines()[:2] == ['100','101'], \
        'right arrow did not follow nextPage'
    subprocess.run(common+['--auto-key','V','--frames','1450',
        '--dump-pages',str(auto_pages)],check=True)
    assert auto_pages.read_text().splitlines()[:3] == ['100','100','101'], \
        'automatic next-page mode did not advance after the last subpage'
    subprocess.run(common+['--auto-key','V','--fail-page','101','--fail-error','7',
        '--frames','1900','--dump-pages',str(auto_skip_pages)],check=True)
    assert auto_skip_pages.read_text().splitlines()[:4] == ['100','100','101','102'], \
        'automatic next-page mode did not skip invalid page content'
    subprocess.run(common+['--auto-keys','1,0,1','--fail-page','101',
        '--fail-error','12','--frames','450','--dump-screen',str(timeout_screen)],check=True)
    timeout=timeout_screen.read_bytes()
    assert b'FOUTCODE: 0C' in timeout and b'FOUT: SERVER REAGEERT NIET' in timeout, \
        'specific P2WP/7 transport error was not explained on screen'
    assert b'DETAIL: HTTP 000 LWIP 00 NET 00' in timeout
    subprocess.run(common+['--p2wp-version','6','--auto-keys','1,0,1',
        '--fail-page','101','--fail-error','12','--frames','450',
        '--dump-screen',str(legacy_timeout_screen)],check=True)
    legacy_timeout=legacy_timeout_screen.read_bytes()
    assert b'FOUTCODE: 04' in legacy_timeout and \
        b'FOUT: ONBEKENDE NETWERKFOUT' in legacy_timeout
    assert b'DETAIL:' not in legacy_timeout, \
        'P2WP/6 exposed P2WP/7-only error diagnostics'
    subprocess.run(common+['--auto-keys','W,STOP','--frames','650',
        '--dump-screen',str(cancel_screen)],check=True)
    assert b'KIES BRON (0-3)' in cancel_screen.read_bytes(), \
        'Shift-STOP did not cancel Wi-Fi setup and return to source selection'
    auto_flash.write_bytes(b'P2WPURL1'+bytes((0xfe,1,0)))
    subprocess.run(common+['--auto-wait-opening','--flash',str(auto_flash),
        '--frames','3900','--dump-screen',str(auto_screen)],check=True)
    auto_display=auto_screen.read_bytes()
    assert b'Meer bevoegdheden' in auto_display and auto_display[35] == ord('V'), \
        'one-minute auto-start did not open source 1 with auto-page enabled'
    subprocess.run(common+['--frames','1300','--dump-fetches',str(loop_fetches)],check=True)
    assert loop_fetches.read_bytes()[:3] == bytes((0,2,0)), \
        'automatic subpage rotation did not wrap from the last subpage to the first'
    subprocess.run(common+['--auto-pause-frame','250','--frames','900',
        '--dump-fetches',str(pause_fetches)],check=True)
    assert pause_fetches.read_bytes() == bytes((0,)), \
        'paused subpage rotation performed an automatic fetch'
    subprocess.run(common+['--auto-pause-frame','250','--auto-resume-frame','350',
        '--frames','1000','--dump-fetches',str(resume_fetches)],check=True)
    assert resume_fetches.read_bytes()[:2] == bytes((0,2)), \
        'A did not resume automatic subpage rotation'
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
    subprocess.run(common+['--auto-source','0','--custom-server',
        'http://terra:8080','--frames','170',
        '--dump-screen',str(custom_dialog_screen)],check=True)
    custom_dialog=custom_dialog_screen.read_bytes()
    assert b'BASISADRES VAN UW EIGEN SERVER' in custom_dialog
    assert b'PICO ONTHOUDT ALLEEN EEN NIEUW ADRES' in custom_dialog
    assert b'SERVERADRES' in custom_dialog and b'ENTER OPSLAAN' in custom_dialog
    assert custom_dialog[9*40:9*40+4] == bytes((0x07,0x1d,0x04,0x20))
    example_server=subprocess.Popen(
        ['python3',str(ROOT/'server/server.py'),'--host','127.0.0.1','--port','0'],
        stdout=subprocess.PIPE,text=True)
    try:
        custom_url=example_server.stdout.readline().strip().rsplit(' ',1)[-1]
        assert custom_url.startswith('http://127.0.0.1:')
        subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
            '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--live',
            '--font',str(EMU/'assets/Default.fnt'),'--headless','--auto',
            '--auto-source','0','--custom-server',custom_url,
            '--flash',str(emulated_flash),
            '--frames','650','--dump-screen',str(custom_screen)],check=True)
    finally:
        example_server.terminate()
        example_server.wait()
    custom=custom_screen.read_bytes()
    assert custom[19:37] == b' '*18 and custom[37:40] == b'100'
    assert b'Your own Teletekst server' in custom
    assert emulated_flash.read_bytes()[0:8] == b'P2WPURL1'
    flash_timestamp=emulated_flash.stat().st_mtime_ns
    time.sleep(0.02)
    subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(EMU/'tests/fixtures/nos-100.json'),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--auto','--auto-source','0','--flash',str(emulated_flash),
        '--frames','650','--dump-screen',str(restored_custom_screen)],check=True)
    assert b'Meer bevoegdheden' in restored_custom_screen.read_bytes()
    assert emulated_flash.stat().st_mtime_ns == flash_timestamp, \
        'unchanged custom URL rewrote emulated flash'
    custom_conceal_fixture.write_text(json.dumps(
        page_response(ROOT/'server/pages',101)),encoding='ascii')
    custom_conceal_command=[str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(custom_conceal_fixture),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--auto','--auto-source','0','--custom-server',
        'http://terra:8080','--frames','650']
    subprocess.run(custom_conceal_command+[
        '--dump-screen',str(custom_concealed_screen)],check=True)
    subprocess.run(custom_conceal_command+[
        '--auto-key','R','--dump-screen',str(custom_revealed_screen)],check=True)
    custom_concealed=custom_concealed_screen.read_bytes()
    custom_revealed=custom_revealed_screen.read_bytes()
    conceal_index=custom_concealed.index(b'\x18A piano!')
    assert custom_revealed[conceal_index:conceal_index+9] == b'\x03A piano!'
    expected_revealed=bytearray(custom_concealed)
    expected_revealed[conceal_index]=0x03
    assert custom_revealed[40:] == expected_revealed[40:]
    subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(EMU/'tests/fixtures/nos-100.json'),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--auto','--auto-key','Z','--frames','650',
        '--dump-screen',str(zoom_screen)],check=True)
    zoom=zoom_screen.read_bytes()
    assert zoom[0] == 0x0d and zoom[40:80] == b' '*40
    reveal_page=bytearray(b' '*960)
    reveal_page[80:90]=b'NOS Telet '
    reveal_page[40:54]=bytes((0x07,))+b'PUBLIC'+bytes((0x18,))+b'SECRET'
    reveal_fixture.write_text(json.dumps({
        'nextSubPage':'',
        'binaryDisplay':base64.b64encode(reveal_page).decode('ascii'),
    }))
    subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',str(reveal_fixture),
        '--font',str(EMU/'assets/Default.fnt'),'--headless','--auto','--auto-key','?',
        '--frames','650','--dump-screen',str(reveal_screen)],check=True)
    revealed=reveal_screen.read_bytes()
    assert revealed[47] == 0x07 and revealed[48:54] == b'SECRET'
    subprocess.run([str(build/'p2000t-emulator'),'--monitor',str(monitor),
        '--cartridge',str(ROOT/'src/p2wp-cartridge.bin'),'--fixture',
        str(EMU/'tests/fixtures/nos-100.json'),'--font',str(EMU/'assets/Default.fnt'),
        '--headless','--auto','--auto-key','H','--frames','650',
        '--dump-screen',str(help_screen)],check=True)
    help_page=help_screen.read_bytes()
    assert b'VERBORGEN TEKST ONTHULLEN' in help_page
    assert b'VORIGE / VOLGENDE PAGINA' in help_page
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
print('P2WP/2-7 compatibility + robust errors + navigation: passed')
