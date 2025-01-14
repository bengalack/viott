import sys
import os

# Build symbol file from map-file
# (argv[0]: python filename) 
# argv[1]: path
# argv[2]: filename without extension

def is_hex(s):
	try:
		int(s, 16)
		return True
	except ValueError:
		return False

f1 = open(sys.argv[1] + os.sep + sys.argv[2] + '_.sym','w')

with open(sys.argv[1] + os.sep + sys.argv[2] + '.map','r') as f2:
	for line in f2:
		line1 = line.strip()
		words = line1.split()
		if len(words) > 1:
			if words[1].startswith( 'l__' ):  # get rid of lengths (I think they are) - they're NOT addresses, and messes up openmsx debugger
				continue

			if words[1].startswith( 's__' ): 
				continue

			if words[1].startswith( '.__' ): 
				continue

			if is_hex(words[0]):
				f1.write(words[1] + ': equ ' + words[0] + "H\n")

exit()
