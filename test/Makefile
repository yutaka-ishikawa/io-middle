# export IO_MIDDLE_ARCH=x86
# export IO_MIDDLE_ARCH=fugaku
include ../dep/mkdef.$(IO_MIDDLE_ARCH)

all: mytest myverify
mytest: mytest.c testlib.h testlib.o ../src/io_middle.so
	$(MPICC) -DMPI -o mytest mytest.c testlib.o
myverify: myverify.o testlib.h testlib.o
	$(CC) -o myverify myverify.o testlib.o
testlib.o: testlib.c testlib.h
	$(MPICC) -c -o $@ $<
run-test-x86-4:
	export LD_PRELOAD=../src/io_middle.so; \
	$(MPIEXEC) -n 4 ./mytest -l 4
run-test-x86-4r:
	export LD_PRELOAD=../src/io_middle.so; \
	$(MPIEXEC) -n 4 ./mytest -l 4 -r
run-test-x86-3:
	rm ./results/tdata-3; rm ./results-nomiddle/tdata-3
	(export LD_PRELOAD=../src/io_middle.so; \
	export IOMIDDLE_CARE_PATH=./results; \
	$(MPIEXEC) -n 3 ./mytest -l 2 -f ./results/tdata-3)
#
	($(MPIEXEC) -n 3 ./mytest -l 2 -f ./results-nomiddle/tdata-3;)
run-test-x86-12:
	export LD_PRELOAD=../src/io_middle.so; \
	export IOMIDDLE_CARE_PATH=./results; \
	$(MPIEXEC) -n 12 ./mytest -l 5 -f ./results/tdata-12
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
