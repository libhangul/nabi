#!/usr/bin/env python
#
# $Author$
# $date$
# $id$

# A tool for merging commented hanja dictionary file with new hanja template
# file

import sys, string

if len(sys.argv) == 3:
	old_file_name = sys.argv[1]
	new_file_name = sys.argv[2]
else:
	print 'Usage: hanja-merge [commented file] [new template]'
	sys.exit(1)

try:
	old_file = open(old_file_name, 'r')
except:
	sys.stderr.write("Cant open file: %s\n" % old_file_name)
	sys.exit(1)
try:
	new_file = open(new_file_name, 'r')
except:
	sys.stderr.write("Cant open file: %s\n" % new_file_name)
	sys.exit(1)

old_table = { }
new_table = { }

category = ''
for line in old_file.readlines():
	line = line.strip()

	if len(line) == 0:
		continue

	if line[0] == '[':
		category = line[1:].split(']')[0]
		old_table[category] = {}
	else:
		if category == '':
			continue

		list = line.strip().split('=')
		key = list[0]
		value = list[1]
		old_table[category][key] = value

category = ''
for line in new_file.readlines():
	line = line.strip()

	if len(line) == 0:
		continue

	if line[0] == '[':
		category = line[1:].split(']')[0]
		new_table[category] = {}
	else:
		if category == '':
			continue

		key = line.strip().split('=')[0]
		if old_table.has_key(category) and old_table[category].has_key(key):
			value = old_table[category].pop(key)
		else:
			value = ''
		new_table[category][key] = value

list = new_table.keys()
list.sort()

# print merged result
for category in list:
	print '[' + category + ']'
	keys = new_table[category].keys()
	keys.sort()
	for key in keys:
		print key + '=' + new_table[category][key]

# print rest
for category in old_table.keys():
	if len(old_table[category]) == 0:
		continue

	sys.stderr.write('[' + category + ']\n')
	keys = old_table[category].keys()
	for key in keys:
		sys.stderr.write(key + '=' + old_table[category][key] + '\n')

