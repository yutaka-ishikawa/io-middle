#!/bin/bash
#PJM -N "IOR" # jobname
#PJM -L "elapse=00:20:00"
#PJM -L "rscunit=rscunit_ft01"
#PJM -L "rscgrp=dvsys-huge"
#	PJM -L "rscgrp=dvsys-sin"
#	PJM -L "node=4x6x16:strict"
#PJM -L "node=9x12x32:strict"
#	PJM -L "node=2"
#	PJM -L "node=4x3x8:strict"
#	PJM -L "node=16"
#PJM -S
#PJM -o 'results-middle/%n.%j.out'
#PJM -e 'results-middle/%n.%j.err'
#PJM --spath 'ior-result/%n.%j.stats'
#PJM --mpi "max-proc-per-node=1"
#PJM --mpi "rank-map-bynode"
#PJM -L freq=2200
#PJM -L proc-core=unlimited

#PJM --llio cn-read-cache=off
#PJM --llio sio-read-cache=off
#PJM --llio cn-cached-write-size=0
#PJM --llio stripe-count=24
#	PJM --llio stripe-count=6
#PJM --llio sharedtmp-size=95258Mi
#PJM --llio localtmp-size=0
#PJM --llio cn-cache-size=128Mi
#PJM --llio stripe-size=2048Ki
#PJM --llio async-close=on
#PJM --llio auto-readahead=on
#PJM --llio perf

##############################
WORK="/share"
MPIOPT="-of ./results-middle/%n.%j.out"

IOR=/home/g9300001/u93027/work/io500/bin/ior
IOROPT1="-C -Q 1 -g -G 27 -k -e -O stoneWallingStatusFile=./result/ior-hard.stonewall_47008 -O stoneWallingWearOut=1 -t 47008 -b 47008 -s 100 -w -D 300 -a POSIX -vvvvvv"

TEMP=`hostname`.$$
mkdir -p ${WORK}/${TEMP}
df ${WORK}/${TEMP}

#echo "WITH IO_MIDDLE"
#export LD_PRELOAD=../src/io_middle.so

echo "WITHOUT_IO_MIDDLE"

export IOMIDDLE_CARE_PATH=/share/${TEMP}/
export IOMIDDLE_WORKER=1
export IOMIDDLE_LANES=4

mpiexec ${MPIOPT} ${IOR} ${IOROPT1_1} -o ${WORK}/${TEMP}/file

unset LD_PRELOAD
ls -l ${WORK}/${TEMP}
printenv
exit
