#!/bin/bash

# Very much slapped-together,
# at some point this might be
# promoted to an actual piece
# of the build process.

for x in noContent.txt onlyGets.txt default.html; do
	in=$x
	# Bash is crazy
	name=${x%.*}
	out=$name.include

	echo "static char const ${name}_bytes[] = {" > $out
	xxd -i < $in >> $out
	echo "};" >> $out
	bytes=`wc -c < $in`
	echo "static int ${name}_len = $bytes;" >> $out
done
