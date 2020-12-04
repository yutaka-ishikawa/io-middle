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
#PJM -L "node=192"
#	PJM -L "node=384"
#	PJM -L "node=768"
#	PJM -L "node=1152"
#	PJM -L "node=32x18x16:strict"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:30:00"
#	PJM -L "rscunit=rscunit_ft01,rscgrp=dvsys-huge1,jobenv=linux"
#PJM -L "rscunit=rscunit_ft01,rscgrp=dvsys-sin"
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

mpiexec ../src/a.out

exit

DDIR=./results-data-middle
rm -f $DDIR/tdata-*

MPIOPT="-ofout ./results-middle/%n.%j.out -oferr ./results-middle/%n.%j.err"
LEN=1920
NP=192

echo "########################################################################"
echo "WITHOUT IO-MIDDLE"
echo "LD_PRELOAD       = " $LD_PRELOAD
echo "LD_LIBRARY_PATH  = " $LD_LIBRARY_PATH
echo "IOMIDDLE_CONFIRM = " $IOMIDDLE_CONFIRM
echo "IOMIDDLE_WORKER  = " $IOMIDDLE_WORKER
echo "IOMIDDLE_LANES   = " $IOMIDDLE_LANES
echo "LEN = " $LEN; echo "NP = " $NP

mpiexec -n $NP $MPIOPT ./mytest -l $LEN -f $DDIR/tdata-$LEN-0

echo
echo
echo
echo "########################################################################"
export LD_PRELOAD=../src/io_middle.so
export IOMIDDLE_CONFIRM=1
export IOMIDDLE_CARE_PATH=$DDIR/
echo "LD_PRELOAD       = " $LD_PRELOAD
echo "LD_LIBRARY_PATH  = " $LD_LIBRARY_PATH
echo "IOMIDDLE_CONFIRM = " $IOMIDDLE_CONFIRM
echo "IOMIDDLE_WORKER  = " $IOMIDDLE_WORKER
echo "IOMIDDLE_LANES   = " $IOMIDDLE_LANES
echo "LEN = " $LEN; echo "NP = " $NP

mpiexec -n $NP $MPIOPT ./mytest -l $LEN -f $DDIR/tdata-$LEN-1
echo
echo
echo
echo "########################################################################"
export IOMIDDLE_LANES=4
echo "LD_PRELOAD       = " $LD_PRELOAD
echo "LD_LIBRARY_PATH  = " $LD_LIBRARY_PATH
echo "IOMIDDLE_CONFIRM = " $IOMIDDLE_CONFIRM
echo "IOMIDDLE_LANES   = " $IOMIDDLE_LANES
echo "IOMIDDLE_WORKER  = " $IOMIDDLE_WORKER
echo "LEN = " $LEN; echo "NP = " $NP

mpiexec -n $NP $MPIOPT ./mytest -l $LEN -f $DDIR/tdata-$LEN-2

echo
echo
echo
echo "########################################################################"
export IOMIDDLE_WORKER=1
echo "LD_PRELOAD       = " $LD_PRELOAD
echo "LD_LIBRARY_PATH  = " $LD_LIBRARY_PATH
echo "IOMIDDLE_CONFIRM = " $IOMIDDLE_CONFIRM
echo "IOMIDDLE_LANES   = " $IOMIDDLE_LANES
echo "IOMIDDLE_WORKER  = " $IOMIDDLE_WORKER
echo "LEN = " $LEN; echo "NP = " $NP

mpiexec -n $NP $MPIOPT ./mytest -l $LEN -f $DDIR/tdata-$LEN-3

echo
echo
echo
ls -lt $DDIR/tdata-*

exit

###########################
###########################

##mpiexec $MPIOPT ./mytest -l 192 -f ./results-middle/tdata-192
#mpiexec $MPIOPT ./mytest -l 3000 -f ./results-middle/tdata-3
#printenv
mkdir /share/tmp/
export IOMIDDLE_CARE_PATH=/share/tmp/
mpiexec ./mytest -l 6 -f /share/tmp/tdata-2
exit

#ls -ld /share/tmp/
#echo "hello" >/share/tmp/hello
#ls -l /share/tmp/
#cat /share/tmp/hello

export IOMIDDLE_CARE_PATH=/share/tmp/
export LD_PRELOAD=../src/io_middle.so

#export IOMIDDLE_DEBUG=31
#export IOMIDDLE_DEBUG=16	# WORKER
#export IOMIDDLE_DEBUG=8

export IOMIDDLE_WORKER=1
export IOMIDDLE_LANES=4

mpiexec ./mytest -l 160 -f /share/tmp/tdata-2

#mpiexec ./mytest -l 921600 -f /share/tmp/tdata-2
#mpiexec ./mytest -l 384000 -f /share/tmp/tdata-2

#mpiexec ./mytest -l 3 -f /share/tmp/tdata-2
#mpiexec ./mytest -l 3 -f ./results-middle/tdata-3

exit

echo
echo
ldd ./mytest
echo
unset LD_PRELOAD
#ls -l /share/tmp/


#mpiexec ./mytest -l 3840 -f /share/tmp/tdata-2
#mpiexec ./mytest -l 76800 -f /share/tmp/tdata-2

#rm -f ./results-middle/tdata-*
#mpiexec ./mytest -l 4 -V -d -f ./results-middle/tdata-2
#mpiexec ./mytest -l 4 -f ./results-middle/tdata-768

#mpiexec ./mytest -l 4 -f ./results-middle/tdata-16
#mpiexec ./mytest -l 4 -f ./results-middle/tdata-768
#mpiexec ./mytest -l 4 -f ./results-middle/tdata-16
#mpiexec ./mytest -l 4 -f ./results-middle/tdata-2

#mpiexec ./mytest -l 1 -f ./results-middle/tdata-3
