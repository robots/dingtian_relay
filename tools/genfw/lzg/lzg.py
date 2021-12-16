from ctypes import *
import io
import os

CLIB = CDLL(os.path.dirname(__file__) + '/../liblzg/src/lib/liblzg.so')


class LZG:


	def __init__(self, level=5, fast=True):

		level = min(max(level,1),9)

		self.config = {'level':level, 'fast':fast}

	def compress(self, rawtext):

		class Config(Structure):
			_fields_ = [('level',c_int32), ('fast',c_int)]

		# initialize stuff
		config = Config(level=self.config['level'], fast=self.config['fast'])

		# get the worst-case on encoded size
		maxEncSize = CLIB['LZG_MaxEncodedSize'](len(rawtext))
		cout = create_string_buffer(maxEncSize)

		encSize = CLIB['LZG_Encode'](rawtext, len(rawtext), cout, len(cout), byref(config))
	
		#ret = []
		#for i in range(encSize):
		#	ret.append(ord(cout[i]))

		return cout[:encSize]

	def decompress(self, encbuf):

		cin = create_string_buffer(len(encbuf))
		for i,val in enumerate(encbuf):
			cin[i] = chr(val)

		estDecSize = CLIB['LZG_DecodedSize'](cin, len(cin))
		cout = create_string_buffer(estDecSize)
		decSize = CLIB['LZG_Decode'](cin, len(cin), cout, len(cout))

		return string_at(cout, decSize)

