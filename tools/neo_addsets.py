#!/usr/bin/env python3
"""Add plain (unprotected) Neo-Geo sets of current MAME to the 0.37b5 driver.
usage: neo_addsets.py <modern neogeo.cpp> <0.37 drivers/neogeo.cpp> <0.37 driver.cpp> set..."""
import re, sys, importlib.util
spec = importlib.util.spec_from_file_location('conv', __file__.replace('neo_addsets.py', 'neo_romconv_lib.py'))
conv = importlib.util.module_from_spec(spec); spec.loader.exec_module(conv)
modern_src, old_path, drv_path = sys.argv[1:4]
new = open(modern_src, encoding='latin-1').read()
old = open(old_path, encoding='latin-1').read()
drv = open(drv_path, encoding='latin-1').read()
modern = {m.group(1): m.group(2) for m in re.finditer(r'^ROM_START\(\s*(\w+)\s*\)(.*?)^ROM_END', new, re.S | re.M)}
games = {m.group(2): m for m in re.finditer(r'^GAME\(\s*(\d+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,[^"]*"([^"]*)"\s*,\s*"([^"]*)"', new, re.M)}
for s in sys.argv[4:]:
    g = games[s]
    year, parent, company, title = g.group(1), g.group(3), g.group(4), g.group(5)
    block = conv.convert(s, modern[s])
    m = re.search(r'^ROM_START\(\s*%s\s*\).*?^ROM_END' % s, old, re.S | re.M)
    if m:
        old = old[:m.start()] + block + old[m.end():]
    else:
        anchor = '/******************************************************************************/\n\n/* For MGD-2 dumps */'
        old = old.replace(anchor, block + '\n\n' + anchor, 1)
    line = 'GAME( %s, %-8s, %-8s, neogeo, neogeo,  neogeo, ROT0_16BIT, "%s", "%s" )' % (year, s, parent, company, title)
    m = re.search(r'^GAME\(\s*\d+\s*,\s*%s\s*,.*$' % s, old, re.M)
    if m:
        old = old[:m.start()] + line + old[m.end():]
    else:
        anchor = 'GAME( 2004, samsh5sp'
        i = old.index(anchor); j = old.index('\n', i) + 1
        old = old[:j] + line + '\n' + old[j:]
    if re.search(r'TESTDRIVER\(\s*%s\s*\)' % s, drv):
        drv = re.sub(r'TESTDRIVER\(\s*%s\s*\)' % s, '\tDRIVER( %s )' % s, drv)
    elif not re.search(r'DRIVER\(\s*%s\s*\)' % s, drv):
        drv = drv.replace('\tDRIVER( samsh5sp )', '\tDRIVER( samsh5sp )\n\tDRIVER( %s )' % s, 1)
    print('added', s, '(parent %s)' % parent)
open(old_path, 'w', encoding='latin-1').write(old)
open(drv_path, 'w', encoding='latin-1').write(drv)
