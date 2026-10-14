#!/usr/bin/env python
#
# Convert an ULTRAMID.INI config file (DMXGUS lump) to a Timidity
# configuration file.
#

import re
import sys

mapping_index = int(sys.argv[1]) / 256
infile = sys.argv[2]

patchname_for_num = {}
patch_mapping = {}

with open(sys.argv[2]) as ultramid:
	for line in ultramid:
		line = line.strip()
		if line.startswith("#"):
			continue
		fields = re.split('\s*,\s*', line)
		if len(fields) < 6:
			continue
		pnum = int(fields[0])
		pname = fields[5]
		patchname_for_num[pnum] = pname
		patch_mapping[pnum] = int(fields[mapping_index])


print "bank 0"
print

for pfrom, pto in sorted(patch_mapping.items()):
	if pfrom < 128:
		print "%i %s" % (pfrom, patchname_for_num[pto])

print
print "drumset 0"
print

for pfrom, pto in sorted(patch_mapping.items()):
	if pfrom > 128:
		print "%i %s" % (pfrom - 128, patchname_for_num[pto])



