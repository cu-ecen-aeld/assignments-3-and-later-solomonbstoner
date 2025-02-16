#!/bin/sh

if [[ $# != 2 ]];
then
	echo "Incorrect number of arguments!"
	exit 1
fi

if [[ ! -d $1 ]];
then
	echo "$1 is NOT a directory!"
	exit 1
fi

num_lines=0
num_files=0

for file in "$1"/*; # Iterate through all non-hidden files
do
	# $file is the full path
	if [[ ! -f $file ]];
	then
		continue
	fi
	num_lines=$((num_lines + $(grep -c $2 $file))) # Add the number of matched lines in each file
	num_files=$((num_files + 1))
done

echo "The number of files are $num_files and the number of matching lines are $num_lines"
