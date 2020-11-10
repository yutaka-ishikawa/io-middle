#!/bin/bash

for file in *.out
do
    echo $file
    grep -v ERROR $file > $file\2
done
