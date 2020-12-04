#!/bin/bash
#PJM -N "IOR-FWDR-1rack-str12-2014Ki" # jobname
#PJM -L "elapse=00:40:00"
#PJM -L "rscunit=rscunit_ft01"
#	PJM -L "rscgrp=dvsys-huge"
#	PJM -L "rscgrp=dvsys-sin"
#	PJM -L "rscgrp=dvsys-mck5"
#	PJM -L "rscgrp=dvsys-mck1,jobenv=linux2"
#	PJM -L "node=384"
#	PJM -L "node=192"
#	PJM -L "node=4x6x16:strict"
#	PJM -L "node=12x3x32"
#	PJM -L "node=4x3x8:strict"
#	PJM -L "node=16"
#	PJM -L "rscgrp=dvsys-mck5,jobenv=linux2"
#	PJM -L "node=1152"
#PJM -L "rscgrp=eap-llio"
#PJM -L "node=4x6x16:strict"
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
#	PJM --llio stripe-count=6
#PJM --llio stripe-count=12
#	PJM --llio sharedtmp-size=95258Mi
#PJM --llio sharedtmp-size=89278Mi
#PJM --llio localtmp-size=0
#	PJM --llio cn-cache-size=128Mi
#	PJM --llio stripe-size=2048Ki
#PJM --llio stripe-size=1024Ki
#PJM --llio async-close=off
#PJM --llio auto-readahead=on
#	PJM --llio perf

##############################
WORK="/share"
MPIOPT="-of ior-result/%n.%j.out -oferr ior-result/%n.%j.err"
MPIOPT_IO_MIDDLE="-of ior-result/%n.%j.out -oferr ior-result/%n.%j.err env LD_PRELOAD=../src/io_middle.so"

IOR=/home/g9300001/u93027/work/io500/bin/ior
#IOROPT1_1="-C -Q 1 -g -G 27 -k -e -O stoneWallingStatusFile=./result/ior-hard.stonewall_47008 -O stoneWallingWearOut=1 -t 47008 -b 47008 -s 10000 -w -D 30 -a POSIX"
IOROPT1_1="-C -Q 1 -g -G 27 -k -e -O stoneWallingStatusFile=./result/ior-hard.stonewall_47008 -O stoneWallingWearOut=1 -t 47008 -b 47008 -s 1000 -w -D 30 -a POSIX"

#NFLIST="96 128 192"
#NFLIST="2 3 4"
NFLIST="2 3 4 6 8"
TEMP=`hostname`.$$
mkdir -p ${WORK}/${TEMP}
printenv | grep LLIO

echo "PJM --llio stripe-size=1024Ki"
echo "TEST!!!"
df -h ${WORK}
#echo "VANILLA IOR"
#mpiexec ${MPIOPT} ${IOR} ${IOROPT1_1} -o ${WORK}/${TEMP}/file-nomiddle

###export LD_PRELOAD=../src/io_middle.so
export IOMIDDLE_CARE_PATH=/share/${TEMP}/
export IOMIDDLE_CONFIRM=1
export IOMIDDLE_WORKER=1
export IOMIDDLE_STAT=2
for NF in $NFLIST; do
	export IOMIDDLE_FORWARDER=$NF
	echo
	echo
	echo "########################################################################"
	echo "MPIOPT_IO_MIDDLE      = " $MPIOPT_IO_MIDDLE
	echo "LD_LIBRARY_PATH  = " $LD_LIBRARY_PATH
	echo "IOMIDDLE_CONFIRM = " $IOMIDDLE_CONFIRM
	echo "IOMIDDLE_FORWARDER  = " $IOMIDDLE_FORWARDER
	echo "IOMIDDLE_WORKER  = " $IOMIDDLE_WORKER
	echo "IOMIDDLE_CARE_PATH  = " $IOMIDDLE_CARE_PATH

	mpiexec ${MPIOPT_IO_MIDDLE} ${IOR} ${IOROPT1_1} -o ${WORK}/${TEMP}/file-iomiddle-forwarder-$NF
	unset LD_PRELOAD
	ls -lt ${WORK}/${TEMP}/
	rm -f ${WORK}/${TEMP}/file-iomiddle-forwarder-$NF
done

echo
echo #################### Shell Environment ####################
printenv
exit
