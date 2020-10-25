#!/bin/sh
#------ pjsub option --------#
#PJM -N "IO-MIDDLE" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#	PJM -L "node=2"
#	PJM -L "node=16"
#	PJM -L "node=64"
#	PJM -L "node=96"
#PJM -L "node=384"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:00:20"
#PJM -L "rscunit=rscunit_ft01,rscgrp=dvsys-huge1,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft01,rscgrp=dvsys-sin,jobenv=linux"
#PJM -L proc-core=unlimited

export LD_PRELOAD=../src/io_middle.so
#export LD_LIBRARY_PATH=../src/:$LD_LIBRARY_PATH

export IOMIDDLE_CARE_PATH=./results-middle/
##export IOMIDDLE_DEBUG=15
export IOMIDDLE_DEBUG=8
mpiexec ./mytest -l 4 -f ./results-middle/tdata
