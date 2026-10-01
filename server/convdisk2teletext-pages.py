#!/usr/bin/env python -v
'''
This program was written in favor off the P2000T teletekst slot2 module and based on the given server.py sample started on github.
https://github.com/ifilot/p2000t-teletekst-cartridge`

It extracts the pp2/ppp files from a P2000T disk image and converts them to a .bin files for the teletext module server (option -o)
OR starts a server to host these pages from memory. (option -s)
OR lists the disk images contents of all filetypes without extracting them. (option -l)

TODO:
- Add .cas file support (cassette files) (.cas example images needed)

'''
import argparse
import textwrap
import os
import re
import sys
import glob
import logging
import base64
import json
from dataclasses import dataclass
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

_VERSION_ = "0.2b"
_COPYRIGHT_ = "(c) Bart Eversdijk '2026. All rights reserved."

# Setup logger and add verbose logging level (-V option)
logger = logging.getLogger('p2000t')
VERBOSE_LEVELV_NUM = 15
logging.addLevelName(VERBOSE_LEVELV_NUM, "VERBOSE")
def verbose(self, message, *args, **kws):
    if self.isEnabledFor(VERBOSE_LEVELV_NUM):
        self._log(VERBOSE_LEVELV_NUM, message, args, **kws) 
logging.Logger.verbose = verbose

# Known file extensions for P2000T image files extensions
knownexts = ['PPP','PP2']

# Tape header
BASEADRS = 0x6547

# Disk track and block configuration (16 sectors per track each sector 256 bytes)
SECTORSIZE = 256
BLOCKS_PER_TRACK = 16
TRACKSIZE = BLOCKS_PER_TRACK * SECTORSIZE

LINE_SIZE = 40
LINES_PER_PAGE = 24
SCREEN_SIZE = LINES_PER_PAGE * LINE_SIZE

TELETEXT_RED     = "\x01"
TELETEXT_GREEN   = "\x02"
TELETEXT_YELLOW  = "\x03"
TELETEXT_BLUE    = "\x04"
TELETEXT_MAGENTA = "\x05"
TELETEXT_CYAN    = "\x06"
TELETEXT_WHITE   = "\x07"

TELETEXT_GR_RED     = "\x11"
TELETEXT_GR_GREEN   = "\x12"
TELETEXT_GR_YELLOW  = "\x13"
TELETEXT_GR_BLUE    = "\x14"
TELETEXT_GR_MAGENTA = "\x15"
TELETEXT_GR_CYAN    = "\x16"
TELETEXT_GR_WHITE   = "\x17"

ANSI_RED     = "\033[38;2;255;0;0m"      # ff0000
ANSI_GREEN   = "\033[38;2;0;255;0m"      # 00ff00
ANSI_YELLOW  = "\033[38;2;255;255;0m"    # ffff00
ANSI_BLUE    = "\033[38;2;0;0;255m"      # 0000ff
ANSI_MAGENTA = "\033[38;2;255;0;255m"    # ff00ff 
ANSI_CYAN    = "\033[38;2;0;255;255m"    # 00ffff
ANSI_WHITE   = "\033[38;2;255;255;255m"  # ffffff
ANSI_RESET   = "\033[0m"

PAGE_NAME    = re.compile(r"^([1-8][0-9]{2})(?:-([1-9][0-9]?))?$")
REQUEST_PATH = re.compile(r"^/json/([1-8][0-9]{2})(?:-([1-9][0-9]?))?/?$")
BYTE_ESCAPE  = re.compile(rb"\\x([0-9a-fA-F]{2})")

# --- define dataclasses for teletext pages and disk index entries
@dataclass
class TeletextPage:
    number: int
    filename: str
    content: bytes

@dataclass
class JWSIndexEntry:
    name: str
    ext: str
    type: str
    size: int
    startadr: int
    side: int
    startblock: int
    lastblock: int
    blocks: int

@dataclass
class PPPIndexEntry:
    header: bytes
    name: str
    ext: str
    type: str
    content: bytes

@dataclass
class DiskEntry:
    diskname: str
    startpage: int
    endpage: int
    num_pages: int

# --- Helper functions for formatting and file operations ---------------------------
def format_index_entry(pagenumber, name):
    page = f"{pagenumber:3d}"
    return f"{TELETEXT_GREEN}{page}{TELETEXT_MAGENTA}{name}{TELETEXT_BLUE}"


def format_hex_text(data, bytes_per_line=32, column_size=8):
    lines = []
    for offset in range(0, len(data), bytes_per_line):
        chunk = data[offset:offset + bytes_per_line]
        columns = []
        for col_start in range(0, bytes_per_line, column_size):
            col = chunk[col_start:col_start + column_size]
            col_hex = " ".join(f"{b:02X}" for b in col).ljust(column_size * 3 - 1)
            columns.append(col_hex)
        decoded = "".join(chr(b) if 32 <= b <= 126 else "." for b in chunk)

        lines.append(f"{offset:04X}  {'  '.join(columns)}  |{decoded}|")

    return "\n".join(lines)
  
def readContent(infilename):
    logger.debug(f"- opening file: {infilename}" )
    with open(infilename, 'rb') as infile:
        logger.debug("- Reading data")
        data = infile.read()
    
    logger.debug(f"- Content size {len(data)} bytes")

    return data

def writeFile(outfilename, data):
    logger.debug(f"- Opening file: {outfilename}")
    with open(outfilename, 'wb') as outfile:
        logger.debug("- Writing data")
        outfile.write(data)
        logger.debug(f"- Content size {len(data)} bytes")

# --- Create and Write page to file for the teletext module -----------------------------------
def createAndWritePageToFile(outdirectory, pagenumber, content, name):

    page = TeletextPage(number=pagenumber, filename=name, content=b"")
    content = bytearray(content)
        
    # file needs to be 960 bytes long, so we need to pad it with " " (0x20) bytes if it is shorter than that.
    if len(content) < SCREEN_SIZE:
        content += bytes([0x20] * (SCREEN_SIZE - len(content)))
    elif len(content) > SCREEN_SIZE:
        content = content[:SCREEN_SIZE]

    # add header top top of the page
    pageheader = "%16s %3d" % (name.strip(), pagenumber)
    for i, b in enumerate(pageheader.encode('ascii', errors='replace')):
        content[i + 19] = b

    if outdirectory != None:
        # Write the extracted data to a .bin file for the teletext module
        logger.verbose("Writing %s to %s\\%03d.bin" % (name, outdirectory, pagenumber))
        outfilename = os.path.join(outdirectory, f"{pagenumber:03d}.bin")
        writeFile(outfilename, bytes(content))

    page.content = bytes(content)
    return page
    

def createEmptyIndexPage(startpage, title="disk index", subpage=1, totalsubpages=1):
    """Create an empty index page starting at the given startpage."""
    content = bytearray(bytes([0x20] * SCREEN_SIZE))
    pageheader = "%16s %03d" % (title, startpage)
    for i, b in enumerate(pageheader.encode('ascii', errors='replace')):
        content[i + 19] = b

    if totalsubpages > 1:
        content[40:40+LINE_SIZE] = bytes(f"{TELETEXT_GR_BLUE},,,,,,,,,,,,,,{TELETEXT_GREEN}index{TELETEXT_GR_BLUE},,,,,,,,,,{TELETEXT_YELLOW}{subpage}/{totalsubpages}{TELETEXT_GR_BLUE},,,", 'ascii', errors='replace')
    else:                                
        content[40:40+LINE_SIZE] = bytes(f"{TELETEXT_GR_BLUE},,,,,,,,,,,,,,{TELETEXT_GREEN}index{TELETEXT_GR_BLUE},,,,,,,,,,,,,,,,,,", 'ascii', errors='replace')
    content[-LINE_SIZE:] = bytes(f"{TELETEXT_GR_BLUE},,,,,,,,,,,,,,{TELETEXT_GREEN}by BEKKIE{TELETEXT_GR_BLUE},,,,,,,,,,,,,,", 'ascii', errors='replace')
    return content

def createDiskIndexPage(startpage, disks):
    """Create a disk index page starting at the given startpage."""
    content = createEmptyIndexPage(startpage)
    for p, idx in enumerate(disks):
        name = os.path.splitext(os.path.basename(idx.diskname))[0]
        line = "\03%3d\06%-16s \03%3d - %3d \05(%3d)" % (idx.startpage, name, idx.startpage+1, idx.endpage, idx.num_pages-1)
        logger.info(f"Processing disk {p}: {name}")
        for j, b in enumerate(line.encode('ascii', errors='replace')):
            content[120 + 1 + (p * LINE_SIZE) + j] = b

    page = TeletextPage(
        number=startpage,
        filename='Disk index',
        content=content
    )

    logger.warning(f"Created disk index page: #899")
    return page

def createIndexPage(outdirectory, startpage, index, title):
    """Create an index page starting at the given startpage and write it to the specified output directory."""
    pages = []

    subpage = 0
    totalsubpages = len(index) // 42 + (1 if len(index) % 42 != 0 else 0)
    content = createEmptyIndexPage(startpage, title, subpage+1, totalsubpages)

    i = 0
    for p, idx in enumerate(index):
        if i >= 42:
            # Write the extracted data to a .bin file for the teletext module
            filename = "%03d%s.bin" % (startpage, (f"-{subpage:d}" if subpage > 0  else ""))
            if outdirectory != None:
                logger.verbose(f"Writing index page to {outdirectory}\\{filename}")
                outfilename = os.path.join(outdirectory, filename)
                writeFile(outfilename, bytes(content))

            page = TeletextPage(
                number=startpage, 
                filename=filename, 
                content=content
            )
            pages.append(page)

            i = 0
            subpage += 1
            content = createEmptyIndexPage(startpage, title, subpage+1, totalsubpages)

        line = ("\03%3d\06%s" % (startpage + p + 1, idx.name)).encode('ascii', errors='replace')
        if i >= 21:
            for j, b in enumerate(line):
                content[120 + 18 + ((i - 22) * LINE_SIZE) + j] = b
        else:
            for j, b in enumerate(line):
                content[80 + (i * LINE_SIZE) + j] = b
        i += 1

    # Write the extracted data to a .bin file for the teletext module
    filename = "%03d%s.bin" % (startpage, (f"-{subpage:d}" if subpage > 0  else ""))
    if outdirectory != None:
        logger.verbose(f"Writing index page to {outdirectory}\\{filename}")
        outfilename = os.path.join(outdirectory, filename)
        writeFile(outfilename, bytes(content))

    page = TeletextPage(
        number=startpage,
        filename=filename,
        content=content
    )
    pages.append(page)

    return pages

# --- JWS disk image handling --------------------------------------------------------------------------------------------------

def listJwsContents(index, metadata=None, colors=True):
    '''List the contents of a JWS disk image.'''
    def print_free_space(blocncnt, colors):
        if colors:
            if 608 - blocncnt > 0:
                print(f"{' ':<22s}{ANSI_GREEN}vrij{ANSI_CYAN}    {608 - blocncnt:3d}{ANSI_RESET}")
            else:
                print(f"{' ':<22s}{ANSI_GREEN}vol{ANSI_CYAN}{ANSI_RESET}")
        else:
            if 608 - blocncnt > 0:
                print( f"{' ':<22s}vrij    {608 - blocncnt:3d}")
            else:
                print( f"{' ':<22s}vol")

    if metadata != None and len(metadata) > 0:
        logger.verbose(f"Disk metadata: {metadata}")
        if colors:
            print(f"{ANSI_CYAN}{metadata['copyright']}{ANSI_RESET}")
            print(f"{ANSI_YELLOW}{metadata['version']} {ANSI_CYAN}{metadata['versionsub']}{ANSI_RESET}")
            print(f"{ANSI_GREEN}{metadata['disk_type']}{ANSI_RESET}")
        else:
            print(f"{metadata['copyright']}")
            print(f"{metadata['version']} {metadata['versionsub']}")
            print(f"{metadata['disk_type']}")

    print(f"{ANSI_CYAN}kant 1{ANSI_RESET}" if colors else "kant 1")

    last_side = 0
    blocncnt = 0

    for i, idx in enumerate(index):
        if idx.side == 1 and last_side == 0:
            print_free_space(blocncnt, colors)
            print(f"{ANSI_CYAN}kant 2{ANSI_RESET}" if colors else "kant 2")
            last_side = 1
            blocncnt = 0

        blocks = idx.lastblock - idx.startblock + 1
        if colors:
            print(f"{ANSI_MAGENTA}{i + 1:3d}:{ANSI_GREEN} {idx.name:<16} {idx.ext:<3} {ANSI_CYAN}{idx.type:<1}   {blocks:3d} {idx.size:6d}{ANSI_RESET}")
        else:
            print(f"{i + 1:3d}: {idx.name:<16} {idx.ext:<3} {idx.type:<1}   {blocks:3d} {idx.size:6d}") 
        blocncnt += blocks

    if last_side == 0:
        print_free_space(blocncnt, colors)
        print(f"{ANSI_CYAN}kant 2{ANSI_RESET}" if colors else "kant 2")
        blocncnt = 0

    print_free_space(blocncnt, colors)


def extractDiskBlock(rawdata, side, track, block, tracks=40, sides=2):
    '''
    Image is |sd0 tr0|sd1 tr0|sd0 tr2|sd1 tr2|sd0 tr3...sd0 tr39|sd1 tr39
    '''     

    total_tracks = int(len(rawdata) / TRACKSIZE)
    logger.debug("=== %d = int(len(%d) / %d) ", total_tracks, len(rawdata), TRACKSIZE)
    if total_tracks > 40:
        tracks = int(total_tracks / 2)
        sides = 2 
    else:    
        tracks = tracks
        sides = 1
    logger.debug("* Found %s disk-image with %d tracks", "DualSided" if sides == 2 else "SingleSided", tracks )

    if block > 15 or block < 0:
        logger.error("Block number %d is out of range (0-15)", block)
        return None
    if track > tracks or track < 0:
        logger.error("Track number %d is out of range (0-%d)", track, tracks)
        return None
    if side > sides or side < 0:
        logger.error("Side number %d is out of range (0-%d)", side, sides)
        return None

    offset = (track * sides * TRACKSIZE) + (side * TRACKSIZE) + (block * SECTORSIZE)
    logger.debug("=== offset = (%d * %d * %d) + (%d * %d) + (%d * %d) == %04x", track, sides,  TRACKSIZE, side, TRACKSIZE,  block, SECTORSIZE, offset)
    return rawdata[offset : offset + SECTORSIZE]

def extractDiskIndexItem(entry):
    '''
    Extract the index of the disk image
    this is listed on track 1 side 0 and track 1 side 1
     0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31
    46 72 61 78 78 6F 6E 20 2B 20 73 63 6F 72 65 73 42 41 53 42 B9 5F 47 65 00 21 00 80 00 01 02 00
    F   r  a  x  x  o  n     +     s  c  o  r  e  s  B  A  S  B  ¹  _  G  e  � !  �  € �   � 
    Each entry is 16 bytes long and contains the following information:
        Name (16 bytes),
        Extention (3 bytes) 
        Type (1 byte)
        size (2 bytes) [5FB9h]
        Start addres (2 bytes)  [6547h]
        Side (1 byte) 
        Start block (2 bytes) [0021h]
        Last block (2 bytes) [0080h]  
        3 unknown bytes

        All sectors are numbered starting from 1, (1 = track 0, sector 0), called block.
        The first block containing actual file data starts with number 33 (which corresponds to side 0, track 2, sector 0).
        (608 blocks of 256 bytes per side = 307200 bytes of file data + 32 blocks * 256 bytes for the index/system code)
    '''

    index = JWSIndexEntry(
        name=entry[0:16].decode('ascii', errors='replace'),
        ext=entry[16:19].decode('ascii', errors='replace'),
        type=entry[19:20].decode('ascii', errors='replace'),
        size=entry[20] + (entry[21] << 8),
        startadr=entry[22] + (entry[23] << 8),
        side=entry[24],
        startblock=entry[25] + (entry[26] << 8),
        lastblock=entry[27] + (entry[28] << 8),
        blocks=0
    )
    index.blocks=index.size // 256 + (1 if index.size % 256 > 0 else 0)
    return index
  
def extractDiskIndex(diskImage, listonly=False):
    '''
    Extract the disk index and metadata from a JWS disk image.
    The JWS meta is located at the last 65 bytes of track 0 side 0.
        bytes (0-23) Copyright information
        bytes (28-42) version information
        bytes (43-61) disk type info

    Returns a tuple (index, metadata) where:
        index: list of index entries
        metadata: dictionary containing disk metadata
    '''
    metadata = {}
    meta_block = extractDiskBlock(diskImage, side=0, track=0, block=15)

    STARTCOPYRIGHT = 0xBF
    STARTVERSION = 0xDB
    STARTVERSIONSUB = 0xE6
    STARTDISKTYPE = 0xEF
    STARTDRIVE = 0xF7
    magicdata     = meta_block[STARTDRIVE:STARTDRIVE+5].decode('ascii', errors='replace')
    if magicdata == 'drive':
        metadata['copyright']  = meta_block[STARTCOPYRIGHT:STARTCOPYRIGHT+24].decode('ascii', errors='replace').replace('\ufffd', ' ')
        metadata['version']    = meta_block[STARTVERSION:STARTVERSION+10].decode('ascii', errors='replace')
        metadata['versionsub'] = meta_block[STARTVERSIONSUB:STARTVERSIONSUB+2].decode('ascii', errors='replace')
        metadata['disk_type']  = meta_block[STARTDISKTYPE:STARTDISKTYPE+13].decode('ascii', errors='replace') + " x"
    
    logger.debug ("Disk Metadata: %s", metadata)

    index = []
    # Directory entries are stored on track 1 on block 8 - 16 on side 0
    for bl in range(8, 16):
        data = extractDiskBlock(diskImage, side=0, track=1, block=bl)
        if data[0] != 0x00:
            for i in range(0, len(data), 32):
                entryraw = data[i:i+32]
                if entryraw[0] != 0x00:
                    idx = extractDiskIndexItem(entryraw)
                    if listonly or idx.ext in knownexts:
                        index.append(idx)

    # Directory entries are stored on track 1 on block 0 - 7 on side 1
    for bl in range(0, 7):
        data = extractDiskBlock(diskImage, side=1, track=1, block=bl)
        if data[0] != 0x00:
            for i in range(0, len(data), 32):
                entryraw = data[i:i+32]
                if entryraw[0] != 0x00:
                    idx = extractDiskIndexItem(entryraw)
                    if listonly or idx.ext in knownexts:
                        index.append(idx)

    for idx in index:
        logger.debug("%s %s %s %3d %7d StartAdr: %04X StartBlock: %3d LastBlock: %3d Side: %d", 
                                    idx.name, idx.ext, idx.type, idx.blocks, idx.size, idx.startadr, idx.startblock, idx.lastblock, idx.side)

    return (index, metadata)

def jws_disks(diskimage, title, outdirectory="", startpage=100, listonly=False, colors=True):
    '''
        Convert a JWS disk image to teletext pages.
        A JWS disk layout:
        - Side 0 Track 0:   JWS system code
        - Side 0 Track 1:   Sector 0-7: JWS system code | Sector 8-15: Directory entries side 0
        - Side 0 Track 2-x: (depending on disk layout 35/40/80), Data blocks follow the directory entries

        - Side 1 Track 0:   - empty -
        - Side 1 Track 1:   Sector 0-6: Directory entries side 1
        - Side 1 Track 2-x: (depending on disk layout 35/40/80), Data blocks follow the directory entries

    '''
    (index, metadata) = extractDiskIndex(diskimage, listonly=listonly)
    pages = []
            
    if listonly:
        listJwsContents(index, metadata=metadata, colors=colors)
    else:
        pages.append(createIndexPage(outdirectory, startpage, index, title))
        
        startpage += 1

        for idx in index:
            if idx.ext not in knownexts:
                logger.verbose("Skipping %s %s" % (idx.name, idx.ext))
                continue

            logger.debug("Extracting %s.%s from side %d, blocks %d-%d, size %d" % (idx.name, idx.ext, idx.side, idx.startblock, idx.lastblock, idx.size))
            
            data = bytes([0x20] * LINE_SIZE) # PP2 pages need start at line 2 so fill the first line with spaces
            for blocknr in range(idx.startblock, idx.lastblock + 1):
                # Block number starts from 1, so we need to subtract 1 when calculating track and sector.
                track = ((blocknr - 1)// BLOCKS_PER_TRACK)
                sector = (blocknr - 1) % BLOCKS_PER_TRACK
            
                offset = (track * 2 * TRACKSIZE) + (idx.side * TRACKSIZE) + (sector * SECTORSIZE)
                logger.debug("Extracting bloknr: %d sector %d from side %d, track %d, offset == %04x" % (blocknr, sector, idx.side, track, offset))
                            
                newblock = extractDiskBlock(diskimage, side=idx.side, track=track, block=sector)
                if len(data) + len(newblock) < idx.size:
                    data += newblock
                else:
                    data += newblock[:idx.size - len(data) + LINE_SIZE - 2]
                    break

            # Print dump in two hex columns plus decoded ASCII text.
            logger.debug("File: %s %s \n%s" % (idx.name, idx.ext, format_hex_text(data)))

            page = createAndWritePageToFile(outdirectory, pagenumber=startpage, content=data, name=idx.name)
            pages.append(page)

            startpage += 1

    logger.debug(" --- done ---")
    if len(pages) > 1:
        logger.info(f"Found {len(pages)} PP2 entries")
    return pages


# --- PPP disk image handling --------------------------------------------------------------------------------------------------

def listPPPContents(index, colors=True):
    for i, idx in enumerate(index):
        if colors:
            print(f"{ANSI_MAGENTA}{i + 1:3d}:{ANSI_GREEN} {idx.name:<16} {idx.ext:<3} {ANSI_CYAN}{idx.type:<1}{ANSI_RESET}")
        else:
            print(f"{i + 1:3d}: {idx.name:<16} {idx.ext:<3} {idx.type:<1}")

def ppp_disks(diskimage, title, outdirectory=".", startpage=100, listonly=False, colors=True):
    '''
    A PPP disk (Marks Plaatjes Disk - PPPDOS) usses the first 6 tracks storing system code, no index data was found
    From track 6 onwards, the actual image data is stored. Each image is 4 sectors = 1024 bytes long (including metadata).
    Not sure how the index is structured or how PPPDOS is able to locate it's files within the disk image.
    
    Extract the index of the disk image
    00 D4 00 04 80 03 61 20  50 32 30 30 30 20 50 50  50 40 00 00 D4 00 D4 82  70 6C 61 61 74 6A 65 01  |......a P2000 PPP@......plaatje.|
    
    Each entry is 1024 (4 sectors) bytes long and contains the following information:
        Header (6 bytes) 00 D4 00 04 80 03
        Name (6 bytes)
        Extention (3 bytes)  (PPP)
        At-sign (1 byte) 40h (@)
        Header (6 bytes)     (00 D4 00 04 80 03)
        "plaatje." (9 bytes)
        image (880 bytes)    = 40 * 22 lines
        112 unknown bytes
    '''
    # search for byte("PPP@\0\0\D4") in the diskimage
    PPP_MAGIC = b"PPP@\x00\x00\xD4"

    # One image block consists of 4 sectors (4 * SECTORSIZE bytes)
    IMAGEBLOCK = 4 * SECTORSIZE
    PPPimgSize = 880  # size of the PPP image = 22 lines * 40 characters per line

    index = []
    imagelist = []
        
    # PPP images are stored per sector, so scan all sectors and look for the PPP_MAGIC signature (@ byte 14)
    num_of_sectors = len(diskimage) // IMAGEBLOCK
    logger.debug("Number of sectors: %d image size %04x" % (num_of_sectors, len(diskimage)))

    for sectornr in range(num_of_sectors):
        start = sectornr * IMAGEBLOCK
        end = start + IMAGEBLOCK

        logger.debug("Look at sector %d, offset %04X" % (sectornr, start))
        if end <= len(diskimage):
            sector = diskimage[start:end]
            if sector.find(PPP_MAGIC) == 14:
                logger.debug("Found PPP_MAGIC at sector %d, offset %04X" % (sectornr, start))

                # Collect the index information and content from the 4 sectors
                index = PPPIndexEntry(
                    header=sector[0:6],
                    name=sector[8:14].decode('ascii', errors='replace'),
                    ext=sector[14:17].decode('ascii', errors='replace'),
                    type=sector[17:18].decode('ascii', errors='replace'),
                    content=bytes([0x20] * LINE_SIZE) + sector[32:32+PPPimgSize]
                )
                
                logger.verbose("Checking for PPP file at %04X %s" % (start, index.name))
                logger.debug("File: %s %s \n%s" % (index.name, index.ext, format_hex_text(index.content)))
    
                imagelist.append(index)

    logger.info("Found %d PPP entries" % len(imagelist))

    pages = []

    if listonly:
        listPPPContents(imagelist, colors=colors)
    else:
        pages.append(createIndexPage(outdirectory, startpage, imagelist, title))

        startpage += 1
        for idx in imagelist:
            # Write the extracted data to a .bin file for the teletext module
            page = createAndWritePageToFile(outdirectory, pagenumber=startpage, content=idx.content, name=idx.name)
            pages.append(page)
            
            startpage += 1

    return pages

def collectpages(infile, outdirectory=".", startpage=100, listonly=False, type="auto", colors=True):

    if listonly:
        print(f"{ANSI_CYAN}Disk naam: {ANSI_YELLOW}{infile}{ANSI_RESET}" if colors else f"Disk naam: {infile}")
    else:
        logger.info(f"Checking file: {infile}")

    image = readContent(infilename=infile)

    if type == "auto":
        # Find magic bytes on track 1 (sector 8-15), to determine disk type
        if b' (PPPDOS)\x00' in image[0x2800:0x3000]:
            type = "ppp"
        else: # assume jws
            type = "jws"

    title = os.path.splitext(os.path.basename(infile))[0]
    if type == "jws":
        images = jws_disks(image, title, outdirectory=outdirectory, startpage=startpage, listonly=listonly, colors=colors)
    elif type.lower() == "ppp":
        images = ppp_disks(image, title, outdirectory=outdirectory, startpage=startpage, listonly=listonly, colors=colors)
    else:
        logger.error(f"Unknown disk type: {type}")
        images = []

    return images

# ----- server handling --------------------------------------------------------------------------------------------------
def page_response(pages, nr, subpage=0):
    '''Return the content for a given page number and subpage.'''
    if pages is None:
        return None

    page = pages.get(nr)
    if page is None:
        logger.warning(f"Page {nr} does not exist")
        return None

    nextSubPage = ""
    logger.debug(f"Page {nr} exists in disks_directory")

    if isinstance(page, list):
        logger.debug(f"Page {nr} is a list of subpages cnt {len(page)} subpage {subpage}")
        if subpage >= len(page):
            logger.warning(f"Subpage {subpage} does not exist for page {nr}")
            return None
        content = page[subpage].content
        nextSubPage = f"{nr}-{subpage + 1}" if subpage + 1 < len(page) else ""
    else:
        if subpage != 0:
            logger.warning(f"Subpage {subpage} does not exist for page {nr}")
            return None
        content = page.content

    page_numbers = sorted(pages)
    position = page_numbers.index(nr) if nr in pages else -1
    prevPage = page_numbers[position - 1] if position > 0 else ""

    # Show disk index page (899) as previous page for page 100 if this page exists
    if nr == 100 and page_numbers[-1] == 899:
        prevPage = 899
    nextPage = page_numbers[position + 1] if position >= 0 and position + 1 < len(page_numbers) else 100

    if len(content) != SCREEN_SIZE:
        logger.warning(f"Page {nr} (sub: {subpage}) has incorrect size {len(content)}")

    return {
        "prevPage": prevPage,
        "nextPage": nextPage,
        "nextSubPage": nextSubPage,
        "binaryDisplay": base64.b64encode(content).decode("ascii"),
    }

class TeletekstHandler(BaseHTTPRequestHandler):
    """Serve custom Teletext pages and a small discovery response."""
    
    def do_GET(self):
        if self.path == "/":
            self.send_body(200, b"P2000T test server: use /json/100\n", "text/plain")
            return

        match = REQUEST_PATH.fullmatch(self.path)
        if match is None:
            self.send_error(404)
            return
        try:
            response = page_response(
                self.server.pages,
                int(match.group(1)),
                int(match.group(2) or 0),
            )
        except (OSError, UnicodeError, ValueError) as error:
            logger.exception("Internal application error processing request logic")
            self.send_error(500, str(error))
            return
        if response is None:
            self.send_error(404, "Teletekst page not found")
            return
        body = json.dumps(response, separators=(",", ":")).encode("ascii")
        self.send_body(200, body, "application/json")

    def send_body(self, status, body, content_type):
        self.send_response(status)
        self.send_header("Content-Type", f"{content_type}; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

def server(host="127.0.0.1", port=8080, pages=[]):
    '''Start the Teletext server on the specified host and port with the given pages.'''
    server = ThreadingHTTPServer((host, port), TeletekstHandler)
    server.pages = pages
    print(
        f"Serving {len(pages)} pages at "
        f"http://{host}:{server.server_port}",
        flush=True,
    )
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


# ----- main entry point --------------------------------------------------------------------------------------------------------
def main(args):
    disks = []
    pages = {}
    index_pages = []
    pagenum=int(args.number)
    
    logger.info("* P2000T disk image file to teletext page converter/server *")
    # Scan for disk image files matching the provided pattern (single, multiple files and/or wildcard patterns)
    for pattern in args.file:
        # Seach for disk image files matching the current pattern
        diskimages = glob.glob(pattern, recursive=True)
        if not diskimages:
            if os.path.isfile(pattern):
                diskimages = [pattern]
            else:
                logger.debug(f"Skipping missing file/pattern: {pattern}")
                continue

        for match in diskimages:
            if os.path.isfile(match):
                disk = DiskEntry(diskname=match, startpage=pagenum, endpage=0, num_pages=0)
                
                # Process each found disk image file 
                content = collectpages(infile=disk.diskname, outdirectory=args.outdirectory, startpage=pagenum, listonly=args.listonly, type=args.type, colors=args.colors)
                if args.listonly == False and (content is None or len(content) < 2):
                    logger.verbose(f"Warning: No content collected for page: {disk.diskname}")
                    continue

                # Convert collected content into teletext pages
                for item in content:
                    if type(item) == list:
                        pagenum = item[0].number
                        index_pages.append(pagenum)
                    else:
                        pagenum = item.number
                    logger.verbose(f"Adding page {pagenum} to pages list")
                    pages[pagenum] = item
                    pagenum += 1

                disk.endpage = pagenum - 1
                disk.num_pages = disk.endpage - disk.startpage + 1
                disks.append(disk)

                # Go to next rounded number in 50 sequence
                pagenum = (pagenum // 50 + 1) * 50

    if args.listonly == False:
        if len(disks) == 0:
            logger.error("No disk images found.")
            sys.exit(1)

        if len(disks) > 1:
            # Create an index page 899) if multiple disks are found and add a reference to it on all the index pages
            pages[899] = createDiskIndexPage(899, disks)
            index_pages.append(899)
            logger.verbose(f"INDEX PAGES: {index_pages}")
            for i, idxnr in enumerate(index_pages):
                nxtindx = index_pages[(i + 1) % len(index_pages)]
                previndx = index_pages[(i - 1) % len(index_pages)]
                logger.verbose(f"Creating index line for page {idxnr}: prev={previndx}, next={nxtindx}") 
                newindexline = bytes(f"{TELETEXT_GR_BLUE},,{TELETEXT_YELLOW}<{previndx}{TELETEXT_GR_BLUE},,,,,,{TELETEXT_GREEN}by BEKKIE{TELETEXT_GR_BLUE},,,,,,{TELETEXT_YELLOW}{nxtindx}>{TELETEXT_GR_BLUE},,", 'ascii', errors='replace')
                if type (pages[idxnr]) == list:
                    for i in pages[idxnr]:
                        i.content[-LINE_SIZE:] = newindexline
                else:
                    pages[idxnr].content[-LINE_SIZE:] = newindexline
        
        logger.info(f"Processed {len(disks)} disk image(s).")
        logger.debug([i for i in enumerate(pages)])

    if args.server:            
        server(host=args.host, port=args.port, pages=pages)        

# ----- command line handling --------------------------------------------------------------------------------------------------
class RawFormatter(argparse.HelpFormatter):
    def _fill_text(self, text, width, indent):
        lines = "\n".join([textwrap.fill(line, width) for line in textwrap.indent(textwrap.dedent(text), indent).splitlines()]) 
        return lines
    def _split_lines(self, text, width):
        lines = super()._split_lines(text, width)
        if text.endswith('\n'):
            lines += ['']
        return lines

program_description = f'''
    Exatract the pp2 files from a p2000t disk image and converts them to a .bin files for the teletext module server.
    Extracts the pp2/ppp files from a P2000T disk image and converts them to a .bin files for the teletext module server (option -o)
    OR starts a server to host these pages from memory. (option -s)
    OR lists the disk images contents of all filetypes without extracting them. (option -l)
    '''

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=program_description, formatter_class=RawFormatter)

    parser.add_argument('--version', '-v', dest='version',      action='version',      version='%(prog)s {version} {copyright}'.format(version=_VERSION_, copyright=_COPYRIGHT_))
    parser.add_argument('--file',    '-f', dest='file',         default=[],            help='input file(s) to parse, wildcard supported', nargs="+")
    parser.add_argument('--type',    '-t', dest='type',         default="auto",        help='disk type auto (auto detected), "jws" or "ppp" (default: auto)')
    parser.add_argument('--out',     '-o', dest='outdirectory', default="",            help='output outdirectory (required when extracting files)')
    parser.add_argument('--number',  '-n', dest='number',       default="100",         help='output file number (default: 100)\n')

    parser.add_argument('--list',    '-l', dest='listonly',     action='store_true',   help="Show file listing from disk image(s) only")
    parser.add_argument('--nocolor', '-b', dest='colors',       action='store_false',  help="Show files output in black and white (default: in color)\n")

    parser.add_argument('--server',   '-s', dest='server',      action='store_true',     help="Start server hosting pages i.s.o. extracting files")
    parser.add_argument("--host",     '-H', dest='host',        default="127.0.0.1",     help="Host address for the server (default: 127.0.0.1)")
    parser.add_argument("--port",     '-p', dest='port',        type=int, default=8080,  help="Port for the server (default: 8080)\n")

    parser.add_argument('--debug',   '-D', dest='debug',        action='store_true',   help="Show detailed and debug information")
    parser.add_argument('--verbose', '-V', dest='verbose',      action='store_true',   help="Show detailed information")
    parser.add_argument('--silent',  '-S', dest='silent',       action='store_true',   help="Show only errors and critical information")
            
    args = parser.parse_args()

    # Setup logger with correct log level
    logger = logging.getLogger('p2000t')
    logger.setLevel( logging.DEBUG if args.debug else \
                    (VERBOSE_LEVELV_NUM if args.verbose else \
                    (logging.ERROR if args.silent else logging.INFO)
                    ))
    if logger.level < 20:
        logging.basicConfig(format='[%(levelname)-8s] %(message)s')
    else:
        logging.basicConfig(format='%(message)s')

    if args.server:
        if args.listonly:
            parser.error("Cannot start server in list-only mode.")
            parser.print_help()
            sys.exit(2)
        args.outdirectory = None

    elif args.outdirectory == "" and not args.listonly:
        parser.error("No output directory specified.")
        parser.print_help()
        sys.exit(2)

    main(args)