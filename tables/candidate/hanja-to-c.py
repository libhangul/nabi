#!/usr/bin/env python
#

# A tool for merging commented hanja dictionary file with new hanja template
# file

import sys, string

if len(sys.argv) == 1:
	print 'Usage: hanja-merge [commented files]...'
	sys.exit(1)

table = { }
for file_name in sys.argv[1:]:
	try:
		file = open(file_name, 'r')
	except:
		sys.stderr.write("Cant open file: %s\n" % file_name)
		sys.exit(1)

	# load the table
	category = ''
	for line in file.readlines():
		line = line.strip()

		if len(line) == 0:
			continue

		if line[0] == '[':
			category = line[1:].split(']')[0]
			table[category] = {}
			table[category]['keylist'] = []
		else:
			if category == '':
				continue

			list = line.strip().split('=')
			key = list[0]
			value = list[1]
			table[category][key] = value
			table[category]['keylist'].append(key)
	file.close()

# print the table
categories = table.keys()
categories.sort()
for category in categories:
	print 'static const CandidateItem item_%04x[] = {' % ord(category.decode('utf-8'))
	print '    { 0x%04x, \"\" }, /* %s */' % (ord(category.decode('utf-8')), category)
	keys = table[category]['keylist']
	for key in keys:
		print '    { 0x%04x, \"%s\" }, /* %s */' % (ord(key.decode('utf-8')), table[category][key], key)
		continue
	print '    { 0x0, \"\" }'
	print '};'
	print ''

print 'static const CandidateItem *candidate_table[] = {'
sys.stdout.write('    ')
i = 1
for category in categories[:-1]:
	sys.stdout.write('item_%04x, ' % ord(category.decode('utf-8')))
	if i % 4 == 0:
	    sys.stdout.write('\n    ')
	i = i + 1

sys.stdout.write('item_%04x\n' % ord(categories[-1].decode('utf-8')))
print '};'
