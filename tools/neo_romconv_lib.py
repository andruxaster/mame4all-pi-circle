#!/usr/bin/env python3
"""Convert Neo-Geo ROM definitions of current MAME (src/mame/snk/neogeo.cpp) to the
MAME 0.37b5 syntax used by MAME4ALL, so that current ROM sets load.

usage: neo_romconv.py <modern neogeo.cpp> <0.37 drivers/neogeo.cpp> <0.37 driver.cpp>
Rewrites the ROM_START blocks (and renamed set names) in place; prints a report."""
import re, sys


# 0.37 set name -> current MAME set name
RENAME = {'sengokh': 'sengokuh', 'burningh': 'burningfh', 'pbobble': 'pbobblen',
          'tws96': 'twsoc96', 'mosyougi': 'moshougi'}
# sets converted by hand (encrypted / protected), leave alone
SKIP = {'mslug', 'mslugx', 'mslug3', 'mslug4', 'mslug5', 'samsho5', 'samsh5sp', 'neogeo'}

#modern = {m.group(1): m.group(2) for m in re.finditer(r'^ROM_START\(\s*(\w+)\s*\)(.*?)^ROM_END', new, re.S | re.M)}

def crc(s):
    m = re.search(r'CRC\(([0-9a-fA-F]+)\)', s)
    return '0x' + m.group(1).lower() if m else '0x00000000'

def args(line):
    m = re.search(r'\((.*)\)', line)
    return [a.strip() for a in m.group(1).split(',')] if m else []

REGIONS = {'"cslot1:maincpu"': 'REGION_CPU1', '"cslot1:sprites"': 'REGION_GFX2',
           '"cslot1:ymsnd:adpcma"': 'REGION_SOUND1 | REGIONFLAG_SOUNDONLY',
           '"cslot1:ymsnd:adpcmb"': 'REGION_SOUND2 | REGIONFLAG_SOUNDONLY'}

def convert(name, body):
    out, region, last = [], None, None
    body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
    for raw in body.split('\n'):
        line = raw.split('//')[0].strip()
        if not line:
            continue
        mac = line.split('(')[0].strip()
        a = args(line)
        if mac == 'ROM_DEFAULT_BIOS':
            continue	# mahjong panel BIOS selection, the 0.37 driver has its own
        if mac == 'ROM_REGION' and a[1] == '"mcu"':
            region = 'skip'; continue	# undumped/unused MCU (ridhero, lbowling, trally)
        if region == 'skip' and mac in ('ROM_LOAD', 'ROM_CONTINUE', 'ROM_FILL'):
            continue
        if mac == 'ROM_REGION':
            tag = a[1]
            if tag not in REGIONS:
                raise ValueError('region ' + tag)
            region = tag
            out.append('\tROM_REGION( %s, %s )' % (a[0], REGIONS[tag]))
        elif mac in ('NEO_SFIX_128K', 'NEO_SFIX_64K'):
            out.append('\t%s( %s, %s )' % (mac, a[0], crc(line)))
        elif mac in ('NEO_BIOS_AUDIO_64K', 'NEO_BIOS_AUDIO_128K', 'NEO_BIOS_AUDIO_256K', 'NEO_BIOS_AUDIO_512K'):
            out.append('\t%s( %s, %s )' % (mac.replace('AUDIO', 'SOUND'), a[0], crc(line)))
        elif mac == 'ROM_LOAD16_WORD_SWAP':
            if region != '"cslot1:maincpu"': raise ValueError('word swap outside maincpu')
            out.append('\tROM_LOAD_WIDE_SWAP( %s, %s, %s, %s )' % (a[0], a[1], a[2], crc(line))); last = 'wide'
        elif mac == 'ROM_LOAD16_BYTE':
            if region != '"cslot1:sprites"': raise ValueError('16-bit byte load outside sprites')
            off = int(a[1], 16)
            m = 'ROM_LOAD_GFX_EVEN' if off % 2 == 0 else 'ROM_LOAD_GFX_ODD'
            out.append('\t%s( %s, 0x%07x, %s, %s )' % (m, a[0], off & ~1, a[2], crc(line))); last = m
        elif mac == 'ROM_LOAD':
            out.append('\tROM_LOAD( %s, %s, %s, %s )' % (a[0], a[1], a[2], crc(line))); last = 'plain'
        elif mac == 'ROM_CONTINUE':
            off, ln = int(a[0], 16), a[1]
            if last == 'wide':
                out.append('\tROM_CONTINUE( 0x%06x, %s | ROMFLAG_WIDE | ROMFLAG_SWAP )' % (off, ln))
            elif last == 'ROM_LOAD_GFX_EVEN':	# GFX_EVEN = odd host byte on LSB_FIRST
				# keep the parity of the preceding load
                out.append('\tROM_CONTINUE( 0x%07x, %s | ROMFLAG_ALTERNATE )' % ((off & ~1) | 1, ln))
            elif last == 'ROM_LOAD_GFX_ODD':
                out.append('\tROM_CONTINUE( 0x%07x, %s | ROMFLAG_ALTERNATE )' % (off & ~1, ln))
            elif last == 'plain':
                out.append('\tROM_CONTINUE( 0x%06x, %s )' % (off, ln))
            else:
                raise ValueError('continue')
        elif mac == 'ROM_Y_ZOOM':
            continue
        else:
            raise ValueError(mac)
    return 'ROM_START( %s )\n%s\nROM_END' % (name, '\n'.join(out))

