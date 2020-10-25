#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define __USE_GNU
#include <dlfcn.h>
#include <mpi.h>
#include "hooklib.h"

#define DLEVEL_ALL	1
#define DLEVEL_HIJACKED	2
#define DLEVEL_BUFMGR	4

#define MODE_UNKNOWN	0
#define MODE_READ	1
#define MODE_WRITE	2

typedef struct fdinfo {
    union {
	struct {
	    unsigned int notfirst:1,
			 dntcare: 1,
			 mode:2;
	};
	int	attrall;
    };
    int		strsize;  /* stripe size */
    int		strcnt;	  /* stripe count */
    int		bufcount; /* block count */
    size_t	bufsize;  /* */
    int		iofd;	  /* file descriptor */
    int		filoff;   /* offset of file */
    int		filcurb;  /* start block# must be written */
    int		filtail;  /* tail block# must be written */
    off64_t	filblklen; /* block length = stripsize*nprocs */
    off64_t	filpos; /* file position in byte */
    off64_t	bufpos; /* buffer position in byte */
    char	*ubuf;
    char	*sbuf;
} fdinfo;

struct ioinfo {
    int		debug;
    int		nprocs;
    int		rank;
    int		mybufcount;
    uint64_t	fdlimit;
    fdinfo	*fdinfo;
};

#define DEBUG(level)	if (_inf.debug&(level))

#define IOMIDDLE_IFERROR(cond, format, ...) do { \
    if (cond) {					\
	dbgprintf(format, __VA_ARGS__);		\
	abort();				\
    }						\
} while(0);

#define MPI_CALL(mpicall) do {			\
    int	rc;					\
    rc = mpicall;				\
    if (rc != 0) {				\
	fprintf(stderr, "%s MPI error\n", __func__);\
	abort();				\
    }						\
} while(0);

/*
 * DEBUGWRITE is implemented by bypassing hooked system call.
 * It must be used inside the write system call hook function.
 * If printf function is used inside the write system call hook,
 * printf eventually issues the write system call and this means
 * infinite loop happens.
 */
#define DEBUGWRITE(format, ...)			\
if (_inf.debug) {				\
    int	sz;					\
    char dbuf[256];				\
    sz = snprintf(dbuf, 128,  format, __VA_ARGS__);\
    __real_write(2, dbuf, sz);			\
}
