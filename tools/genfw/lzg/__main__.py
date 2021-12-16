import sys
from lzg import LZG

def main(args):
	
	coder = LZG()

	if len(args) != 2:
		args.append('abcdefghijlkmnopqrstuvwxyz'*100)

	print 'Original ({} length): "{}"'.format(len(args[1]), args[1] if len(args[1])<=50 else args[1][:50]+'...')
	compressed = coder.compress(args[1])
	print 'Compressed ({} length): "{}"'.format(len(compressed), compressed if len(compressed)<=50 else compressed[:50])
	decompressed = coder.decompress(compressed)
	print 'Decompressed ({} length): "{}"'.format(len(decompressed), decompressed if len(decompressed)<=50 else decompressed[:50]+'...')

if __name__ == '__main__':
	main(sys.argv)
