#!/bin/sh
#------ pjsub option --------#
#PJM -N "IO-NOMIDDLE" # jobname
#PJM -S		# output statistics
#PJM --spath "results-nomiddle/%n.%j.stat"
#PJM -o "results-nomiddle/%n.%j.out"
#PJM -e "results-nomiddle/%n.%j.err"
#	PJM -L "node=9216"
#	PJM -L "node=1152"
#	PJM -L "node=768"
#	PJM -L "node=384"
#PJM -L "node=192"
#	PJM -L "node=16"
#	PJM -L "node=3"
#	PJM -L "node=2"
#	PJM -L "node=3"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=01:00:00"
#	PJM -L "rscunit=rscunit_ft01,rscgrp=dvsys-huge1,jobenv=linux"
#PJM -L "rscunit=rscunit_ft01,rscgrp=dvsys-sin"
#PJM -L proc-core=unlimited

MPIOPT="-of results-nomiddle/%n.%j.out"
##export LD_PRELOAD=../src/io_middle.so
##export LD_LIBRARY_PATH=../src/:$LD_LIBRARY_PATH
##export IOMIDDLE_CARE_PATH=./results/
##export IOMIDDLE_DEBUG=15
##export IOMIDDLE_DEBUG=8

rm -f ./results-nomiddle/tdata-*
mpiexec $MPIOPT ./mytest -l 1920 -f ./results-nomiddle/tdata-192

#rm -f ./results-nomiddle/tdata-16
#mpiexec ./mytest -l 4 -f ./results-nomiddle/tdata-16
#mpiexec ./mytest -l 4 -f ./results-nomiddle/tdata-2

#mpiexec ./mytest -l 1 -f ./results-nomiddle/tdata-3
