#!/bin/bash
MPIEXEC=/usr/bin/mpiexec

do_test ()
{
    $MPIEXEC -n 2 ./mytest -l 4 -r -v -f $1/tdata-0
    $MPIEXEC -n 3 ./mytest -l 1 -r -v -f $1/tdata-1
    $MPIEXEC -n 3 ./mytest -l 2 -r -v -f $1/tdata-2
    $MPIEXEC -n 3 ./mytest -l 3 -r -v -f $1/tdata-3
    $MPIEXEC -n 3 ./mytest -l 4 -r -v -f $1/tdata-4
}


if [ $# = 0 ]; then
    echo "Usage: run-x86-test.sh [no-middle|simple|multi|worker|multi-worker]"
    exit
fi
   
case $1 in
    no-middle)
	do_test ./results-nomiddle
	do_test ./results-simple
	do_test ./results-multi
	do_test ./results-worker
	do_test ./results-multi-worker
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
    *)
	echo "Usage: run-x86-test.sh [no-middle|simple|multi|worker|multi-worker]"
	;;
esac

exit

