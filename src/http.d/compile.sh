#!/bin/bash

cd "$(dirname "$0")"

# Okay so maybe I'm gradually re-implementing `make`, what of it??????

build() {
	in=$1
	# Bash is crazy
	name=${1%.*}
	out=$name.include

	# Seems like nonexistent files are treated by `-nt` as "very old",
	# so this one test handles both cases actually.
	if [ "$in" -nt "$out" ]; then
		echo "hexdumping $in to $out";
	else
		return;
	fi;

	echo "static char const ${name}_bytes[] = {" > $out
	xxd -i < $in >> $out
	echo "};" >> $out
	bytes=`wc -c < $in`
	echo "static int ${name}_len = $bytes;" >> $out
}

build noContent.txt &
build onlyGets.txt &
build default.html &

wait
