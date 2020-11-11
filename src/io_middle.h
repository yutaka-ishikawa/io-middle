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

#ifdef STATISTICS
#include "utf_tsc.h"
#define TIMER_READ	0
#define TIMER_WRITE	1
#define TIMER_OPEN	2
#define TIMER_CLOSE	3
#define TIMER_SPLIT	4
#define TIMER_GATHER	5
#define TIMER_SCATTER	6
#define TIMER_MAX	7
static char *timer_str[TIMER_MAX] = {
    "Read", "Write", "Open", "Close", "MPI_Split", "MPI_Gather", "MPI_Scatter" };
#endif /* STATISTICS */

#define DLEVEL_ALL	0x1
#define DLEVEL_HIJACKED	0x2
#define DLEVEL_BUFMGR	0x4
#define DLEVEL_CONFIRM	0x8
#define DLEVEL_WORKER	0x10
#define DLEVEL_READ	0x20
#define DLEVEL_FWRDR	0x40
#define DLEVEL_RSV1	0x80

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
    int		bufend;	  /* read end */
    size_t	bufsize;  /* total size */
    int		iofd;	  /* file descriptor */
    int		filoff;   /* offset of file */
    int		filcurb;  /* start block# must be written */
    int		filtail;  /* seek point of block#, this block has not been written */
    off64_t	filchklen;/* chunck length = stripsize*nprocs */
    off64_t	filpos;   /* file position in byte, the beggining of write/read position  */
    off64_t	bufpos;   /* buffer position in byte */
    int		lanepos;  /* lane posision for read */
    char	*ubuf;
    char	*sbuf;
    char	*dbuf[2];
#ifdef STATISTICS
    uint64_t	io_tm_start;
    uint64_t	io_time_tot[TIMER_MAX];
    uint64_t	io_time_max[TIMER_MAX];
    uint64_t	io_time_min[TIMER_MAX];
    size_t	io_sz[TIMER_MAX];
#endif /* STATISTICS */
} fdinfo;

typedef size_t (*io_cmd)(int, void*, size_t, off64_t);

struct ioinfo {
    int		init;
    int		debug:15,
		tmr:1,
		fwrdr:16;
    int		nprocs;
    int		rank;
    int		frank;
    MPI_Comm	clstr;
    int		cl_color;
    int		cl_nprocs;
    int		cl_rank;
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
    io_cmd	wrk_cmd;
    size_t	wrk_cret;	/* return value to the client */
    int		wrk_cfd;	/* client fd */
    void	*wrk_cbuf;	/* client buffer, allocated by the client */
    size_t	wrk_csiz;	/* client requested size */
    off64_t	wrk_cpos;	/* client requested file possition */
    off64_t	wrk_wsiz;	/* worker io size */
    off64_t	wrk_wpos;	/* worker current possition */
    off64_t	wrk_wlen;	/* */
    size_t	wrk_wret;	/* */
    int		wrk_sig;
    int		wrk_nfst;	/* not first time to be invoked */
    int		wrk_tiktok;	/* toggle */
#ifdef STATISTICS
    uint64_t	tm_hz;
#endif /* STATISTICS */
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
#define Fwrdr	(_inf.fwrdr)
#define Clstr	(_inf.clstr)
#define Color	(_inf.cl_color)
#define Cprocs	(_inf.cl_nprocs)
#define Crank	(_inf.cl_rank)
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
#define Wwsiz	(_inf.wrk_wsiz)
#define Wwpos	(_inf.wrk_wpos)
#define Wwlen	(_inf.wrk_wlen)
#define Wwret	(_inf.wrk_wret)
#define Wsig	(_inf.wrk_sig)
#define Wtiktok (_inf.wrk_tiktok)
#define Wnfst	(_inf.wrk_nfst)

#ifdef STATISTICS
#define Wtmhz	(_inf.tm_hz)

#define STAT_BEGIN(fd) do {	\
    if (_inf.tmr) {			       \
	_inf.fdinfo[fd].io_tm_start = tick_time();	\
    }							\
} while(0);

#define STAT_END(fd, type, sz) do {		\
    if (_inf.tmr) {\
	uint64_t	tm = tick_time() - _inf.fdinfo[fd].io_tm_start; \
	_inf.fdinfo[fd].io_time_max[type]				\
	    = tm > _inf.fdinfo[fd].io_time_max[type] ? tm : _inf.fdinfo[fd].io_time_max[type]; \
	_inf.fdinfo[fd].io_time_min[type]				\
	    = tm < _inf.fdinfo[fd].io_time_max[type] ? tm : _inf.fdinfo[fd].io_time_min[type]; \
	_inf.fdinfo[fd].io_time_tot[type] += tm;			\
	_inf.fdinfo[fd].io_sz[type] += sz;					\
    } \
} while(0);
#define TIMER_SECOND(t)	((t) != UINT64_MAX ? (double)(t)/(double)Wtmhz : 0)
#define SIZE_MiB(t)	((double)(t)/(double)(1024*1024))
#else
#define STAT_BEGIN(type)
#define STAT_END(fd, type, sz)
#endif /* STATISTICS */
