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
run-test-x86-4:
	export LD_PRELOAD=../src/io_middle.so; \
	$(MPIEXEC) -n 4 ./mytest -l 4
run-test-x86-4r:
	export LD_PRELOAD=../src/io_middle.so; \
	$(MPIEXEC) -n 4 ./mytest -l 4 -r
run-test-x86-3:
	rm -f ./results/tdata-3; rm -f ./results-nomiddle/tdata-3
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	$(MPIEXEC) -n 3 ./mytest -l 4 -f ./results/tdata-3)
#
	($(MPIEXEC) -n 3 ./mytest -l 4 -f ./results-nomiddle/tdata-3)
#
run-test-x86-3-mlanes:
	rm -f ./results/tdata-3; rm -f ./results-nomiddle/tdata-3
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	 export IOMIDDLE_LANES=2; \
	$(MPIEXEC) -n 3 ./mytest -l 10 -f ./results/tdata-3)
#
	($(MPIEXEC) -n 3 ./mytest -l 10 -f ./results-nomiddle/tdata-3)
#
run-test-x86-3-worker:
	rm -f ./results/tdata-3; rm -f ./results-nomiddle/tdata-3
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	 export IOMIDDLE_WORKER=1; \
	$(MPIEXEC) -n 3 ./mytest -l 8 -f ./results/tdata-3)
#
	($(MPIEXEC) -n 3 ./mytest -l 8 -f ./results-nomiddle/tdata-3)
run-test-x86-3-wr: # write & read
#     WRITE
	rm -f ./results/tdata-*; rm -f ./results-nomiddle/tdata-*
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	$(MPIEXEC) -n 3 ./mytest -l 1 -f ./results/tdata-1)
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	$(MPIEXEC) -n 3 ./mytest -l 2 -f ./results/tdata-2)
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	$(MPIEXEC) -n 3 ./mytest -l 3 -f ./results/tdata-3)
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	$(MPIEXEC) -n 3 ./mytest -l 4 -f ./results/tdata-4)
	export IOMIDDLE_WORKER=1; \
#     READ
	(export LD_PRELOAD=../src/io_middle.so; \
	 export IOMIDDLE_CARE_PATH=./results; \
	 export IOMIDDLE_WORKER=1; \
	$(MPIEXEC) -n 3 ./mytest -l 4 -r -f ./results/tdata-4)
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
