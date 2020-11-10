#!/bin/bash
for i in ior-result/IOR-FWDR-*.out; do
    echo $i
    sh ./EXTRACT.sh $i
done
