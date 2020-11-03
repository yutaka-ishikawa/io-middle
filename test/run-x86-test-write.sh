#!/bin/bash
MPIEXEC=/usr/bin/mpiexec

do_test ()
{
    [ -d $1 ] || mkdir $1
    rm -f $1/tdata-*
    $MPIEXEC -n 2 ./mytest -l 4 -f $1/tdata-0
    $MPIEXEC -n 3 ./mytest -l 1 -f $1/tdata-1
    $MPIEXEC -n 3 ./mytest -l 2 -f $1/tdata-2
    $MPIEXEC -n 3 ./mytest -l 3 -f $1/tdata-3
    $MPIEXEC -n 3 ./mytest -l 4 -f $1/tdata-4
    ls -lt $1/
}

do_verify ()
{
    cmp results-nomiddle/tdata-0 $1/tdata-0
    if [ $? = 0 ]; then echo -n "Success "; else echo -n "Fail "; fi; echo "$1/tdata-0"
    cmp results-nomiddle/tdata-1 $1/tdata-1
    if [ $? = 0 ]; then echo -n "Success "; else echo -n "Fail "; fi; echo "$1/tdata-1"
    cmp results-nomiddle/tdata-2 $1/tdata-2
    if [ $? = 0 ]; then echo -n "Success "; else echo -n "Fail "; fi; echo "$1/tdata-2"
    cmp results-nomiddle/tdata-3 $1/tdata-3
    if [ $? = 0 ]; then echo -n "Success "; else echo -n "Fail "; fi; echo "$1/tdata-3"
    cmp results-nomiddle/tdata-4 $1/tdata-4
    if [ $? = 0 ]; then echo -n "Success "; else echo -n "Fail "; fi; echo "$1/tdata-4"
}

if [ $# = 0 ]; then
    echo "Usage: run-x86-test.sh [gen_ref|simple|multi|worker|multi-worker|verify]"
    exit
fi
   
case $1 in
    gen_ref)
	do_test ./results-nomiddle
	;;
    simple)
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results-simple; \
	 do_test ./results-simple)
	;;
    multi)
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results-multi; \
	 export IOMIDDLE_LANES=4; \
	 do_test ./results-multi)
	;;
    worker)
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results-worker; \
	 export IOMIDDLE_WORKER=1; \
	 do_test ./results-worker)
	;;
    multi-worker)
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results-multi-worker; \
	 export IOMIDDLE_LANES=4; \
	 export IOMIDDLE_WORKER=1; \
	 do_test ./results-multi-worker)
	;;
    verify)
	if [ $# != 2 ]; then
	    echo "Specify which results";
	    exit
	fi
	do_verify $2
	;;
    *)
	echo "Usage: run-x86-test.sh [gen_ref|simple|multi|worker|multi-worker|verify]"
	;;
esac

exit

