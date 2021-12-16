#!/usr/bin/python

import sys
import xtea
#import crcmod
import binascii
import argparse
import struct
import random
#from lzg import lzg

parser = argparse.ArgumentParser(description='Process some firmware')
parser.add_argument('--fw-file', dest='fw_file', action='store', help='Firmware file to process')
parser.add_argument('--out-file', dest='out_file', action='store', help='Firmware file to output')
parser.add_argument('--board', dest='boardver', action='store', help='Board version')
parser.add_argument('--crypt-mode', dest='crypt_mode', action='store', default="ECB", help='Enable encryption')
parser.add_argument('--crypt-iv', dest='crypt_iv', action='store', default="12345678", help='Block mode IV')
#parser.add_argument('--compress', dest='compress', action='store_const', const=True, default=False, help='Compress firmware')

xtea_modes = {
    "None": 0,
    "ECB": xtea.MODE_ECB,
    "CBC": xtea.MODE_CBC,
}

board_keys = {
    "v1": "\x78\x56\x43\x12\x78\x56\x43\x12\x78\x56\x43\x12\x78\x56\x43\x12",
}

board_id = {
    "v1": 0,
}

args = parser.parse_args()

if not args.boardver in board_keys.keys():
    raise Exception("unknown board id")

if not args.crypt_mode in xtea_modes.keys():
    raise Exception("unknown mode")

if args.fw_file is None:
    raise Exception("no file set")

if args.crypt_mode == 'CBC' and args.crypt_iv == "12345678":
    raise Exception("Iv is default!!!")

if args.crypt_iv == "random":
    random.seed()
    args.crypt_iv = struct.pack("II",  random.randint(0,2**32-1), random.randint(0,2**32-1))
    if args.crypt_iv == "\xff\xff\xff\xff\xff\xff\xff\xff":
        args.crypt_iv = struct.pack("II",  random.randint(0,2**32-1), random.randint(0,2**32-1))

if not len(args.crypt_iv) == 8:
    raise Exception("Iv is not 8long")

if args.crypt_mode == 'CBC':
    print "IV = ", repr(args.crypt_iv)

filein = open(args.fw_file, 'rb')
firmware = filein.read()
filein.close()


fwlen = len(firmware)
print "Lenght = ", fwlen
pad = fwlen % 8
if pad > 0:
    firmware = firmware + "\xff" * (8 - pad)
fwlen = len(firmware)

print "Lenght paded = ", len(firmware)

#crc
crc = binascii.crc32(firmware) & 0xffffffff
#crcfn = crcmod.predefined.mkPredefinedCrcFun('crc-32')
#crc = crcfn(firmware)
print "Crc = %x" % crc

#if args.compress:
#
#    coder = lzg.LZG()
#    compressed = coder.compress(firmware)
#
#    # remove header
#    compressed = compressed[16:]
#
#    cfwlen = len(compressed)
#    print "Compressed lenght = ", cfwlen
#    cpad = cfwlen % 8
#    if cpad > 0:
#        compressed = compressed + "\xff" * (8 - cpad)
#    cfwlen = len(compressed)
#    print "Compressed padded lenght = ", cfwlen
#
#    firmware = compressed


#xtea
if xtea_modes[args.crypt_mode] > 0:
    xt = xtea.new(key=board_keys[args.boardver], mode=xtea_modes[args.crypt_mode], IV=args.crypt_iv, endian='<')
    firmware = xt.encrypt(firmware)

fo = open(args.out_file, 'wb')

fo.write('\x52\x42\x57\x46')
fo.write(struct.pack('<B', board_id[args.boardver]))
if True: #not args.compress:
    fo.write(struct.pack('<B', xtea_modes[args.crypt_mode]))
else:
    fo.write(struct.pack('<B', xtea_modes[args.crypt_mode] + 0x80))
fo.write(struct.pack('<H', 0))
fo.write(struct.pack('<I', fwlen))
fo.write(struct.pack('<I', crc))
fo.write(args.crypt_iv)
fo.write(firmware)
fo.close()

