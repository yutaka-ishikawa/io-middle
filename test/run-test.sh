#!/bin/sh
#------ pjsub option --------#
#PJM -N "STAT-TEST" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#	PJM -L "node=3"
#PJM -L "node=3"
#	PJM -L "node=3"
#	PJM -L "node=16"
#	PJM -L "node=64"
#	PJM -L "node=96"
#	PJM -L "node=384"
#	PJM -L "node=768"
#PJM --mpi "max-proc-per-node=12"
#PJM -L "elapse=00:05:00"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft01,rscgrp=dvsys-sin,jobenv=linux"
#PJM -L proc-core=unlimited

MPI_OPT="-of ./results/%n.%j.out -oferr ./results/%n.%j.err"

rm -f ./results-nomiddle/tdata-*; rm -f ./results-middle/tdata-*
echo "Reference data: ./results-nomiddle/tdata-3"
mpiexec $MPI_OPT ./mytest -l 360 -f ./results-nomiddle/tdata-3

echo "Testing"
export LD_PRELOAD=../src/io_middle.so
export IOMIDDLE_CARE_PATH=./results-middle/
export IOMIDDLE_CONFIRM=1
export IOMIDDLE_WORKER=1
export IOMIDDLE_FORWARDER=3
export IOMIDDLE_STAT=2

mpiexec $MPI_OPT ./mytest -l 360 -f ./results-middle/tdata-3

echo "Verifying"
unset LD_PRELOAD
mpiexec $MPI_OPT ./mytest -l 360 -r -v -f ./results-middle/tdata-3

exit


