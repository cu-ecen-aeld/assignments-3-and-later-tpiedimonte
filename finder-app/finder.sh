#!/bin/bash

seachdir=$1
text=$2

if [ -z ${seachdir} -o -z ${text} ]
then
	echo "Incorrect script arguments"
	exit 1
fi

if [ ! -d ${seachdir} ]
then
	echo "Directory does not exist"
	exit 1
fi

count=$(find ${seachdir} -type f | wc -l)
search=$(grep -r ${text} ${seachdir} | wc -l)

echo "The number of files are ${count} and the number of matching lines are ${search}"
exit 0

