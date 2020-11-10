#!/bin/bash
#PJM -N "IOR-IOMIDDLE" # jobname
#PJM -L "elapse=00:10:00"
#PJM -L "rscunit=rscunit_ft01"
#	PJM -L "rscgrp=dvsys-huge"
#PJM -L "rscgrp=dvsys-sin"
#	PJM -L "node=4x6x16:strict"
#PJM -L "node=192"
#	PJM -L "node=4x3x8:strict"
#	PJM -L "node=16"
#PJM -S
#PJM -o 'ior-result/%n.%j.out'
#PJM -e 'ior-result/%n.%j.err'
#PJM --spath 'ior-result/%n.%j.stats'
#PJM --mpi "max-proc-per-node=1"
#PJM --mpi "rank-map-bynode"
#PJM -L freq=2200
#PJM -L proc-core=unlimited

#PJM --llio cn-read-cache=off
#PJM --llio sio-read-cache=off
#PJM --llio cn-cached-write-size=0
#	PJM --llio stripe-count=24
#PJM --llio stripe-count=6
#PJM --llio sharedtmp-size=95258Mi
#PJM --llio localtmp-size=0
#	PJM --llio cn-cache-size=128Mi
#PJM --llio stripe-size=2048Ki
#PJM --llio async-close=off
#PJM --llio auto-readahead=on
#	PJM --llio perf

##############################
WORK="/share"
MPIOPT="-of ior-result/%n.%j.out -oferr ior-result/%n.%j.err"

IOR=/home/g9300001/u93027/work/io500/bin/ior
IOROPT1_1="-C -Q 1 -g -G 27 -k -e -O stoneWallingStatusFile=./result/ior-hard.stonewall_47008 -O stoneWallingWearOut=1 -t 47008 -b 47008 -s 10 -w -D 30 -a POSIX"

TEMP=`hostname`.$$
mkdir -p ${WORK}/${TEMP}

mpiexec ${MPIOPT} ${IOR} ${IOROPT1_1} -o ${WORK}/${TEMP}/file-nomiddle

export LD_PRELOAD=../src/io_middle.so
export IOMIDDLE_CARE_PATH=/share/${TEMP}/
export IOMIDDLE_CONFIRM=1
export IOMIDDLE_WORKER=1
export IOMIDDLE_LANES=4
echo "########################################################################"
echo "LD_PRELOAD       = " $LD_PRELOAD
echo "LD_LIBRARY_PATH  = " $LD_LIBRARY_PATH
echo "IOMIDDLE_CONFIRM = " $IOMIDDLE_CONFIRM
echo "IOMIDDLE_FORWARDER  = " $IOMIDDLE_FORWARDER
echo "IOMIDDLE_LANES  = " $IOMIDDLE_LANES
echo "IOMIDDLE_WORKER  = " $IOMIDDLE_WORKER
echo "IOMIDDLE_CARE_PATH  = " $IOMIDDLE_CARE_PATH

mpiexec ${MPIOPT} ${IOR} ${IOROPT1_1} -o ${WORK}/${TEMP}/file-iomiddle-lane-4

echo "########################################################################"
export IOMIDDLE_LANES=8
echo "LD_PRELOAD       = " $LD_PRELOAD
echo "LD_LIBRARY_PATH  = " $LD_LIBRARY_PATH
echo "IOMIDDLE_CONFIRM = " $IOMIDDLE_CONFIRM
echo "IOMIDDLE_FORWARDER  = " $IOMIDDLE_FORWARDER
echo "IOMIDDLE_LANES  = " $IOMIDDLE_LANES
echo "IOMIDDLE_WORKER  = " $IOMIDDLE_WORKER
echo "IOMIDDLE_CARE_PATH  = " $IOMIDDLE_CARE_PATH

mpiexec ${MPIOPT} ${IOR} ${IOROPT1_1} -o ${WORK}/${TEMP}/file-iomiddle-lane-8

unset LD_PRELOAD
ls -lt ${WORK}/${TEMP}/

exit
