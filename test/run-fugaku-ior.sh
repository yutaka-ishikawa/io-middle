#!/bin/sh
#------ pjsub option --------#
#PJM -N "IOR-MIDDLE" # jobname
#PJM -S		# output statistics
#PJM --spath "results-middle/%n.%j.stat"
#PJM -o "results-middle/%n.%j.out"
#PJM -e "results-middle/%n.%j.err"
#	PJM -L "node=3"
#	PJM -L "node=2"
#	PJM -L "node=3"
#	PJM -L "node=16"
#	PJM -L "node=64"
#	PJM -L "node=96"
#PJM -L "node=192"
#	PJM -L "node=384"
#	PJM -L "node=768"
#	PJM -L "node=1152"
#	PJM -L "node=32x18x16:strict"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:05:00"
#	PJM -L "rscunit=rscunit_ft01,rscgrp=dvsys-huge1,jobenv=linux"
#PJM -L "rscunit=rscunit_ft01,rscgrp=dvsys-sin,jobenv=linux"
#PJM -L proc-core=unlimited
#export XOS_MMM_L_HPAGE_TYPE=none

#PJM --llio cn-read-cache=off
#PJM --llio sio-read-cache=off
#PJM --llio cn-cached-write-size=0
#	PJM --llio stripe-count=24
#PJM --llio stripe-count=6
#PJM --llio sharedtmp-size=95258Mi
#PJM --llio localtmp-size=0
#PJM --llio cn-cache-size=128Mi
#PJM --llio stripe-size=2048Ki
#	PJM --llio async-close=on
#PJM --llio async-close=off
#PJM --llio auto-readahead=on
#	PJM --llio perf

IOR=/home/g9300001/u93027/work/io500/bin/ior
MPIOPT="-of ./results-middle/%n.%j.out"
WORK="/share"
TEMP="ishikawa"

mkdir -p ${WORK}/${TEMP}
df ${WORK}/${TEMP}

export LD_PRELOAD=../src/io_middle.so
export IOMIDDLE_WORKER=1
export IOMIDDLE_LANES=4

export IOMIDDLE_CARE_PATH=./results-middle/

IOROPT1="-C -Q 1 -g -G 27 -k -e -O stoneWallingStatusFile=./result/ior-hard.stonewall_47008 -O stoneWallingWearOut=1 -t 47008 -b 47008 -s 100 -w -D 300 -a POSIX -vvvvvv"

mpiexec ${MPIOPT} ${IOR} ${IOROPT1} -o ${WORK}/${TEMP}/file
