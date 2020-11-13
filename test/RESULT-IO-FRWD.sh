#!/bin/bash
#for i in ior-result/IOR-FWDR-*.out; do
echo $*

for i in $*; do
    sh ./EXTRACT.sh $i
done
