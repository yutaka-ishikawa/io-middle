#!/bin/bash
#PJM -N "IOR" # jobname
#PJM -L "elapse=00:00:20"
#PJM -L "rscunit=rscunit_ft01"
#	PJM -L "rscgrp=dvsys-huge"
#PJM -L "rscgrp=dvsys-sin"
#	PJM -L "node=4x6x16:strict"
#	PJM -L "node=2"
#PJM -L "node=4x3x8:strict"
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
#PJM --llio cn-cache-size=128Mi
#PJM --llio stripe-size=2048Ki
#PJM --llio async-close=on
#PJM --llio auto-readahead=on
#	PJM --llio perf

##############################
WORK="/share"
MPIOPT="-of result/%n.%j.out"

##IOR=/vol0002/fjuser/fj0058/io500/io500-sc20_1rack/io500_lseek64/bin/ior
IOR=/home/g9300001/u93027/work/io500/bin/ior
####IOR=/home/g9300001/u93027/work/io500/bin/ior-lseek
IOROPT1="-C -Q 1 -g -G 27 -k -e -O stoneWallingStatusFile=./result/ior-hard.stonewall_47008 -O stoneWallingWearOut=1 -t 47008 -b 47008 -s 100000 -w -D 300 -a POSIX"
IOROPT2="-C -Q 1 -g -G 27 -k -e -O stoneWallingStatusFile=./result/ior-hard.stonewall_47008 -O stoneWallingWearOut=1 -t 47008 -b 47008 -s 100000 -r -R -a POSIX"
#
#IOROPT1_1="-C -Q 1 -g -G 27 -k -e -O stoneWallingStatusFile=./result/ior-hard.stonewall_47008 -O stoneWallingWearOut=1 -t 47008 -b 47008 -s 100 -w -D 300 -a POSIX -vvvvvv"
IOROPT1_1="-C -Q 1 -g -G 27 -k -e -O stoneWallingStatusFile=./result/ior-hard.stonewall_47008 -O stoneWallingWearOut=1 -t 47008 -b 47008 -s 100 -w -D 300 -a MPIIO --collective --mpiio.showHints"
##############################

TEMP=`hostname`.$$

##export XOS_MMM_L_HPAGE_TYPE=none

mkdir -p ${WORK}/${TEMP}

#export LD_PRELOAD=../src/io_middle.so
#export LD_LIBRARY_PATH=../src/:$LD_LIBRARY_PATH
#export IOMIDDLE_DEBUG=15
export IOMIDDLE_CARE_PATH=/share/${TEMP}/

mpiexec ${MPIOPT} ${IOR} ${IOROPT1_1} -o ${WORK}/${TEMP}/file
exit
#mpiexec ./mytest -l 4 -f ./results-middle/tdata-768
#printenv
mpiexec ${MPIOPT} ${LOAD}/IOR ${IOROPT2} -o ${WORK}/${TEMP}/file
#for tsbs in ${TSBS}
#do
#  tsz=`echo $tsbs | cut -d: -f1`
#  bsz=`echo $tsbs | cut -d: -f2`
#
#  echo "WORK DIRECTORY = (${WORK}/${TEMP})"
#  mpiexec ${MPIOPT} ${LOAD}/IOR ${IOROPT} -b ${bsz} -i ${ITERATION} -t ${tsz} -o ${WORK}/${TEMP}/file
#
#done

rm -fr ${WORK}/${TEMP}
exit 0
