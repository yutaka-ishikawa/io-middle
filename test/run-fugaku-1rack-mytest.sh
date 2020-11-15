#!/bin/sh
#------ pjsub option --------#
#PJM -N "IO-MIDDLE" # jobname
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
#	PJM -L "node=192"
#PJM -L "node=384"
#	PJM -L "node=768"
#	PJM -L "node=1152"
#	PJM -L "node=32x18x16:strict"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:30:00"
#	PJM -L "rscunit=rscunit_ft01,rscgrp=dvsys-huge1,jobenv=linux"
#	PJM -L "rscunit=rscunit_ft01,rscgrp=dvsys-sin"
#PJM -L "rscgrp=dvsys-mck1,jobenv=linux2"
#	PJM -L "rscgrp=dvsys-small"
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

##############################
DDIR="/share"
MPIOPT="-of ior-result/%n.%j.out -oferr ior-result/%n.%j.err"

rm -f $DDIR/tdata-*

MPIOPT="-ofout ./results-middle/%n.%j.out -oferr ./results-middle/%n.%j.err"
LEN=38400
NP=384

echo "########################################################################"
echo "WITHOUT IO-MIDDLE"
echo "LD_PRELOAD       = " $LD_PRELOAD
echo "LD_LIBRARY_PATH  = " $LD_LIBRARY_PATH
echo "IOMIDDLE_CONFIRM = " $IOMIDDLE_CONFIRM
echo "IOMIDDLE_WORKER  = " $IOMIDDLE_WORKER
echo "IOMIDDLE_LANES   = " $IOMIDDLE_LANES
echo "LEN = " $LEN; echo "NP = " $NP

mpiexec -n $NP $MPIOPT ./mytest -l $LEN -f $DDIR/tdata-$LEN-0

export LD_PRELOAD=../src/io_middle.so
export IOMIDDLE_CARE_PATH=$DDIR
export IOMIDDLE_CONFIRM=1
export IOMIDDLE_WORKER=1
export IOMIDDLE_STAT=1
NFLIST="2 3 4 6 8"
for NF in $NFLIST; do
	export LD_PRELOAD=../src/io_middle.so
	export IOMIDDLE_FORWARDER=$NF
	echo
	echo
	echo "########################################################################"
	echo "LD_PRELOAD       = " $LD_PRELOAD
	echo "LD_LIBRARY_PATH  = " $LD_LIBRARY_PATH
	echo "IOMIDDLE_CONFIRM = " $IOMIDDLE_CONFIRM
	echo "IOMIDDLE_FORWARDER  = " $IOMIDDLE_FORWARDER
	echo "IOMIDDLE_WORKER  = " $IOMIDDLE_WORKER
	echo "IOMIDDLE_CARE_PATH  = " $IOMIDDLE_CARE_PATH

	mpiexec -n $NP $MPIOPT ./mytest -l $LEN -f $DDIR/tdata-$LEN-$NF
	unset LD_PRELOAD
	ls -lt $DDIR
	rm -f $DDIR/tdata-$LEN--$NF
done

echo
echo #################### Shell Environment ####################
printenv
exit
