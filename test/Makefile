# export IO_MIDDLE_ARCH=x86
# export IO_MIDDLE_ARCH=fugaku
include ../dep/mkdef.$(IO_MIDDLE_ARCH)

all: mytest myverify
mytest: mytest.c testlib.h testlib.o ../src/io_middle.so
	$(MPICC) $(CFLAGS) -o mytest mytest.c testlib.o
myverify: myverify.o testlib.h testlib.o
	$(CC) -o myverify myverify.o testlib.o
testlib.o: testlib.c testlib.h
	$(MPICC) $(CFLAGS) -c -o $@ $<
#
#
#
run-test-ioforwarder:
	rm -f ./results-forwarder/tdata-9
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results-forwarder; \
	 export IOMIDDLE_FORWARDER=3; \
	$(MPIEXEC) -n 9 ./mytest -l 18 -f ./results-forwarder/tdata-9)
run-test-all: run-test-write run-test-read
#
run-test-write: run-test-gen_ref run-test-simple run-test-multi run-test-rest
run-test-gen_ref:
	sh ./run-x86-test-write.sh gen_ref
run-test-simple:
	sh ./run-x86-test-write.sh simple
run-test-multi:
	sh ./run-x86-test-write.sh multi
run-test-rest:
	sh ./run-x86-test-write.sh worker
	sh ./run-x86-test-write.sh multi-worker
	sh ./run-x86-test-write.sh verify results-simple
	sh ./run-x86-test-write.sh verify results-multi-worker
#
run-test-read:
	sh ./run-x86-test-read.sh simple
	sh ./run-x86-test-read.sh multi
	sh ./run-x86-test-read.sh worker
	sh ./run-x86-test-read.sh multi-worker
#
run-test-x86-2org-r: # read
	$(MPIEXEC) -n 2 ./mytest -l 4 -r -v -f ./results-nomiddle/tdata-0
run-test-x86-2-r: # read
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	$(MPIEXEC) -n 2 ./mytest -l 4 -r -v -f ./results-simple/tdata-0)
run-test-x86-cmp:
	cmp ./results/tdata-0 ./results-nomiddle/tdata-0
	cmp ./results/tdata-1 ./results-nomiddle/tdata-1
	cmp ./results/tdata-2 ./results-nomiddle/tdata-2
	cmp ./results/tdata-3 ./results-nomiddle/tdata-3
	cmp ./results/tdata-4 ./results-nomiddle/tdata-4
run-test-x86-nomiddle:
	rm -f ./results-nomiddle/tdata-*
	$(MPIEXEC) -n 2 ./mytest -l 4 -f ./results-nomiddle/tdata-0
	$(MPIEXEC) -n 3 ./mytest -l 1 -f ./results-nomiddle/tdata-1
	$(MPIEXEC) -n 3 ./mytest -l 2 -f ./results-nomiddle/tdata-2
	$(MPIEXEC) -n 3 ./mytest -l 3 -f ./results-nomiddle/tdata-3
	$(MPIEXEC) -n 3 ./mytest -l 4 -f ./results-nomiddle/tdata-4
run-test-x86-3-wr: w24 w31 w32 w33 w34
#	tdata-0 376064(47008*8 (2x4)), tdata-1 141024(47008*3 (3x1)), tdata-2 282048 (47008*6 (3x2))
#	tdata-3 423072(47008*9 (3x3)), tdata-4 564096(47008*4 (3x4))
#     WRITE
w24:
	rm -f ./results/tdata-*;
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	 $(MPIEXEC) -n 2 ./mytest -l 4 -f ./results/tdata-0)
#
w31:
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	 $(MPIEXEC) -n 3 ./mytest -l 1 -f ./results/tdata-1)
#
w32:
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	 $(MPIEXEC) -n 3 ./mytest -l 2 -f ./results/tdata-2)
#
w33:
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	 $(MPIEXEC) -n 3 ./mytest -l 3 -f ./results/tdata-3)
#
w34:
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	 export IOMIDDLE_WORKER=1; \
	 $(MPIEXEC) -n 3 ./mytest -l 4 -f ./results/tdata-4)
#     READ
#	(export LD_PRELOAD=../src/io_middle.so; \
#	 export IOMIDDLE_CARE_PATH=./results; \
#	 export IOMIDDLE_WORKER=1; \
#	$(MPIEXEC) -n 3 ./mytest -l 4 -r -f ./results/tdata-4)
#
run-test-x86-3-trunc:
	($(MPIEXEC) -n 3 ./mytest -l 4 -f ./results/tdata-3)
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	$(MPIEXEC) -n 3 ./mytest -l 2 -t -f ./results/tdata-3)
	ls -l ./results/tdata-3; echo "must be 282048"

run-test-x86-12-trunc:
	rm -f ./results/tdata-12; rm -f ./results-nomiddle/tdata-12
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	 $(MPIEXEC) -n 12 ./mytest -l 5 -f ./results/tdata-12)

	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	 $(MPIEXEC) -n 12 ./mytest -l 3 -t -f ./results/tdata-12)
#
	 $(MPIEXEC) -n 12 ./mytest -l 3 -t -f ./results-nomiddle/tdata-12
#
#
run-test-x86-12:
	rm -f ./results/tdata-12; rm -f ./results-nomiddle/tdata-12
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	 $(MPIEXEC) -n 12 ./mytest -l 3 -f ./results/tdata-12)
#
	 $(MPIEXEC) -n 12 ./mytest -l 3 -f ./results-nomiddle/tdata-12
	ls -lt ./results*/tdata-12
#
#
run-test-x86-debug:
	export IOMIDDLE_DEBUG=2; \
	export LD_PRELOAD=../src/io_middle.so; \
	$(MPIEXEC) -n 4 ./mytest -d -l 1
run-verify-x86-4:
	./myverify -c 4 -w -l 4	# -c is strip count -l is length
clean:
	rm -f *.o mytest

run-clean:
	rm -f core.*
