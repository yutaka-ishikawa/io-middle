# export IO_MIDDLE_ARCH=x86
# export IO_MIDDLE_ARCH=fugaku
include ../dep/mkdef.$(IO_MIDDLE_ARCH)

all: io_middle.so

io_middle.so: hooklib.o io_middle.o
	$(MPICC) $(CFLAGS_SHARED) $(LDFLAGS_SHARED) -o $@ $^ -ldl
io_middle.o: io_middle.c io_middle.h
	$(MPICC) $(CFLAGS_SHARED) -c -o $@ $<
hooklib.o: hooklib.c
	$(CC) $(CFLAGS_SHARED) -c -o $@ $^
clean:
	rm -f *.o *.so

