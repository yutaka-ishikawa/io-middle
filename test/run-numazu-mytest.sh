#!/bin/sh
#------ pjsub option --------#
#PJM -N "IO-MIDDLE" # jobname
#PJM -S		# output statistics
#PJM --spath "results-middle/%n.%j.stat"
#PJM -o "results-middle/%n.%j.out"
#PJM -e "results-middle/%n.%j.err"
#	PJM -L "node=3"
#PJM -L "node=3"
#	PJM -L "node=3"
#	PJM -L "node=16"
#	PJM -L "node=64"
#	PJM -L "node=96"
#	PJM -L "node=384"
#	PJM -L "node=768"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:05:00"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft01,rscgrp=dvsys-sin,jobenv=linux"
#PJM -L proc-core=unlimited

##export XOS_MMM_L_HPAGE_TYPE=none

#export IOMIDDLE_DEBUG=15
#export IOMIDDLE_DEBUG=8

rm -f ./results-middle/tdata-*
echo

###########
# NO MIDDLE for checking mytest program
###########
MPI_OPT="-of ./results-nomiddle/%n.%j.out"
mpiexec $MPI_OPT ./mytest -l 3 -f ./results-nomiddle/tdata-3
mpiexec $MPI_OPT ./mytest -l 3 -r -v -f ./results-nomiddle/tdata-3

export LD_PRELOAD=../src/io_middle.so
export IOMIDDLE_CARE_PATH=./results-middle/
#MPI_OPT="-of ./results-middle/%n.%j.out"
MPI_OPT=

mpiexec $MPI_OPT ./mytest -l 3 -f ./results-middle/tdata-3
mpiexec $MPI_OPT ./mytest -l 3 -r -v -f ./results-middle/tdata-3

%%ldd ./mytest
exit

#mpiexec ./mytest -l 4 -f ./results-middle/tdata-768

#mpiexec ./mytest -l 4 -f ./results-middle/tdata-16
#mpiexec ./mytest -l 4 -f ./results-middle/tdata-768
#mpiexec ./mytest -l 4 -f ./results-middle/tdata-16
#mpiexec ./mytest -l 4 -f ./results-middle/tdata-2

#mpiexec ./mytest -l 1 -f ./results-middle/tdata-3
