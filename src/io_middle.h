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
#include <stdint.h>
#define __USE_GNU
#include <dlfcn.h>
#include <mpi.h>
#include "hooklib.h"

#define DLEVEL_ALL	0x1
#define DLEVEL_HIJACKED	0x2
#define DLEVEL_BUFMGR	0x4
#define DLEVEL_CONFIRM	0x8
#define DLEVEL_WORKER	0x10

#define MODE_UNKNOWN	0
#define MODE_READ	1
#define MODE_WRITE	2

#define WRK_CMD_DONE	NULL
#define WRK_CMD_WRITE	io_write
#define WRK_CMD_READ	io_read
#define WRK_CMD_FIN	io_workerfin

typedef struct fdinfo {
    union {
	struct {
	    unsigned int notfirst:1,	/* not first lseek/read/write call */
			 frstrwcall:1,	/* first read/write call or not */
			 rslvstr:1,     /* needs to resolve stripe size */
			 dntcare: 1,
			 trunc: 1,	/* flag of open with O_TRUNC */
			 dirty:1,	/* buffer is dirty or not */
			 rwmode: 2;	/* read or write mode */
	};
	int	attrall;
    };
    int		flags;	  /* flags specified by open/creat */
    int		mode;	  /* mode specified by open/creat */
    char	*path;	  /* file path */
    int		strsize;  /* stripe size */
    int		strcnt;	  /* stripe count */
    int		buflanes; /* number of stripes */
    int		bufcount; /* one stripe (== stripe count) */
    size_t	bufsize;  /* total size */
    int		iofd;	  /* file descriptor */
    int		filoff;   /* offset of file */
    int		filcurb;  /* start block# must be written */
    int		filtail;  /* seek poiint of block#, this block has not been written */
    off64_t	filblklen;/* block length = stripsize*nprocs */
    off64_t	filpos;   /* file position in byte, the beggining of write/read position  */
    off64_t	bufpos;   /* buffer position in byte */
    int		lanepos;  /* lane posision for read */
    char	*ubuf;
    char	*sbuf;
} fdinfo;

struct ioinfo {
    int		debug;
    int		aio;
    int		nprocs;
    int		rank;
    int		frank;
    int		mybuflanes;
    int		mybufcount;
    int		reqtrunc;
    uint64_t	fdlimit;
    fdinfo	*fdinfo;
    /* the following variables are for woker thread */
    unsigned int	wrk_enable:1, wrk_enblflg:1;
    pthread_t		wrk_thid;
    pthread_barrier_t	wrk_sync;
    pthread_mutex_t	wrk_mtx;
    pthread_cond_t	wrk_cnd;
    size_t	(*wrk_cmd)(int, void*, size_t, off64_t);
    size_t	wrk_cret;	/* return value to the client */
    int		wrk_cfd;	/* client fd */
    void	*wrk_cbuf;	/* client buffer, allocated by the client */
    size_t	wrk_csiz;	/* client requested size */
    off64_t	wrk_cpos;	/* client requested file possition */
    int		wrk_sig;
    int		wrk_nfst;	/* not first time to be invoked */
    char	*wrk_rbuf;	/* used for read created by the worker */
    size_t	wrk_rret;	/* */
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

#define Myrank 	(_inf.rank)
#define Frank 	(_inf.frank)
#define Nprocs 	(_inf.nprocs)
#define Wenable	(_inf.wrk_enable)
#define Wenbflg	(_inf.wrk_enblflg)
#define Wthid	(_inf.wrk_thid)
#define Wsync	(_inf.wrk_sync)
#define Wmtx	(_inf.wrk_mtx)
#define Wcnd	(_inf.wrk_cnd)
#define Wcret	(_inf.wrk_cret)
#define Wcmd	(_inf.wrk_cmd)
#define Wcfd	(_inf.wrk_cfd)
#define Wcbuf	(_inf.wrk_cbuf)
#define Wcsiz	(_inf.wrk_csiz)
#define Wcpos	(_inf.wrk_cpos)
#define Wsig	(_inf.wrk_sig)
#define Wrbuf	(_inf.wrk_rbuf)
#define Wrret	(_inf.wrk_rret)
#define Wnfst	(_inf.wrk_nfst)
