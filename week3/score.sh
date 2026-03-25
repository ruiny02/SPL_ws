#!/bin/bash

A=$1
B=$2

make

echo $((A + B)) > answer.txt
echo $((A - B)) >> answer.txt

./w3 $A $B > output.txt

diff answer.txt output.txt > result.txt

make clean
