#!/bin/sh
#------ pjsub option --------#
#PJM -N "IO-MIDDLE" # jobname
#PJM -S		# output statistics
#PJM --spath "results-middle/%n.%j.stat"
#PJM -o "results-middle/%n.%j.out"
#PJM -e "results-middle/%n.%j.err"
#PJM -L "node=3"
#	PJM -L "node=2"
#	PJM -L "node=3"
#	PJM -L "node=16"
#	PJM -L "node=64"
#	PJM -L "node=96"
#	PJM -L "node=192"
#	PJM -L "node=384"
#	PJM -L "node=768"
#	PJM -L "node=1152"
#	PJM -L "node=32x18x16:strict"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:04:00"
#	PJM -L "rscunit=rscunit_ft01,rscgrp=dvsys-huge1,jobenv=linux"
#PJM -L "rscunit=rscunit_ft01,rscgrp=eap-small"
#PJM -L proc-core=unlimited
#export XOS_MMM_L_HPAGE_TYPE=none

MPIOPT="-ofout ./results-middle/%n.%j.out -oferr ./results-middle/%n.%j.err"

#export LD_PRELOAD="../src/io_middle.so /opt/FJSVxtclanga/tcsds-1.2.27b/lib64/libfjstring_internal.so"
#export LD_PRELOAD="./io_middle.so /opt/FJSVxtclanga/tcsds-1.2.27b/lib64/libfjstring_internal.so"
##export LD_LIBRARY_PATH=../src:$LD_LIBRARY_PATHww

##export LD_PRELOAD=../src/io_middle.so
##export LD_LIBRARY_PATH=./:$LD_LIBRARY_PATH
export LD_PRELOAD=./libio_middle.so
export IOMIDDLE_DEBUG=31
export IOMIDDLE_WORKER=1
export IOMIDDLE_LANES=4
export IOMIDDLE_CONFIRM=1
export IOMIDDLE_CARE_PATH=./results-middle/
echo $LD_LIBRARY_PATH

mpiexec $MPIOPT	./mytest -l 3 -f ./results-middle/tdata-3

#mpiexec $MPIOPT	-x LD_LIBRARY_PATH=../src/:/opt/FJSVxtclanga/tcsds-1.2.27b/lib64:$LD_LIBRARY_PATH \
#	 ./mytest -l 3 -f ./results-middle/tdata-3

#mpiexec $MPIOPT ./mytest -l 192 -f ./results-middle/tdata-192

#mpiexec $MPIOPT ./mytest -l 3000 -f ./results-middle/tdata-3
#printenv
exit

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
