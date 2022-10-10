#!/bin/bash

file=$1
text=$2

if [ -z ${file} -o -z ${text} ]
then
	echo "Incorrect script arguments"
	exit 1
fi



mkdir -p "${file%/*}"
echo "${text}" > ${file}
exit 0

