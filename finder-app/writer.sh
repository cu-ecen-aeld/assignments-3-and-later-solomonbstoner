#!/bin/bash

if [[ $# != 2 ]];
then
	echo "Incorrect number of arguments!"
	exit 1
fi

# I can't think of a better solution
mkdir -p $1
rmdir $1
echo $2 > $1
