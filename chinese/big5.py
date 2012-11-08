# Converts Nano-X BG2UBG.KU Big5/Unicode conversion table to a format we
# can use with the HZK fonts.

import struct
import sys

table = open(sys.argv[1]).read()

print 'uint16_t hzk2uni[] = {'

for i in range(0, len(table) / 4 - 1):
  big5, unicode = struct.unpack('<HH', table[i * 4 : i * 4 + 4])

  # decode Big5
  cl = big5 & 0xff
  ch = big5 >> 8
  if cl < 127:
    cl -= 64
  else:
    cl -= 98
  seq = (ch - 161) * 157 + cl

  print '    [0x%04x] = 0x%04x,' % (seq, unicode)

print '};'
