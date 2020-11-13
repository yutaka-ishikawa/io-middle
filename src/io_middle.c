/*
 * IO Middleware implementing a two phase I/O protocol
 *		2020/10/25, yutaka.ishikawa@riken.jp
 *
 * Assumptions:
 *	1) A file descriptor is always used for read or write operation,
 *	   not combination of read and write.
 *	2) lseek is always issued for the next stripe.
 *	3) read and write sizes are always the same length over all processes.
 *	   This length is used for stripe size.
 * Captured system calls:
 *	creat, open, close, read, lseek, lseek64, write
 * Shell environments:
 *	IOMIDDLE_CARE_PATH 
 *	   -- file path taken care by this middleware.
 *	     The user must specify this variable.
 *	IOMIDDLE_CONFIRM
 *	   -- display this middleware works in stderr.
 *	IOMIDDLE_FORWARDER
 *	   -- Number of IO forwarders. The number must divide proccess count without remainder.
 *	     If not specify, all proccesses are IO forwarders.
 *	IOMIDDLE_WORKER
 *	   -- if specify, asynchronous I/O read/writer is performed 
 *	      by a worker thread.
 *	IOMIDDLE_LANES
 *	   -- Speficfy the number of lanes. One lane is (stripsize * stripe count)
 *	      This is effective if IOMIDDLE_FORWARDER is not specify.
 *	IOMIDDLE_TUNC
 *	   -- if specify, enable global file truncation at the close time.
 *	      Note that this behavior is different than POSIX,
 *	      because procccesses independently close a file descriptor
 *	      whose file pointer is local and differs than others in POSIX.
 *	IOMIDDLE_DISABLE
 *	   -- disable this middleware hooks.
 *	IOMIDDLE_DEBUG
 *	   -- debug messages are displayed.
 * Usage:
 *	$ cd ../src
 *	$ (export LD_PRELOAD=../src/io_middle.so; \
 *	   export IOMIDDLE_CARE_PATH=./results; \
 *	   mpiexec -n 12 ./mytest -l 3 -f ./results/tdata-12)
 *	Note that the LD_PRELOAD environment should not be set on your bash
 *	   currently used.  If set, all commands/programs invoked on that bash 
 *	   are contolled under this IO Middleware.
 * TODO:
 *	dup and dup2 must be also captured.
 */
#include "io_middle.h"
#include <mpi.h>
#include <limits.h>
#include <assert.h>

#define POS_TO_BLK(off, strsize)		((off)/(strsize))
#define REQBLK_TO_CURBLK(blk, strcnt, nprocs)	((blk)/((strcnt)*(nprocs)))

static struct ioinfo _inf;
static char	care_path[PATH_MAX];

static void	worker_init(int);
static void	worker_fin();
static void	io_init(int flg, int fd);
static int	io_setup(io_cmd, int fd, size_t siz, off64_t pos);
static void	io_write_bufupdate(fdinfo *info);
static void	io_fin(int fd);
static size_t	io_issue(io_cmd,  int fd, void *buf, size_t size, off64_t pos);
#ifdef STATISTICS
static void	stat_show(fdinfo*);
#endif /* STATISTICS */

#if 0
static char	*dont_path[] = {
    "/",
    "/mnt1/",
    "linux-vdso",
    "/etc/",
    "/proc/",
    "/usr/lib/",
    "/lib/",
    "/dev/",
    "/sys/",
    "../src/",
    "log",
    0
};
#endif

static char dbgbuf[1024];
static int
dbgprintf(const char *fmt, ...)
{
    va_list	ap;
    int		rc;

    snprintf(dbgbuf, 1024, "[%d] %s", Myrank, fmt);
    va_start(ap, fmt);
    rc = vfprintf(stderr, dbgbuf, ap);
    va_end(ap);
    fflush(stderr);
    return rc;
}

static void
info_show(int fd, const char *fname)
{
    DEBUGWRITE("[%d] %s: nprocs(%d) bufsize(%ld) strsize(%d) "
	       "block size(%ld) mybufcount(%d)\n",
	       Myrank, fname, Nprocs,
	       _inf.fdinfo[fd].bufsize, _inf.fdinfo[fd].strsize,
	       _inf.fdinfo[fd].filchklen, _inf.mybufcount);
}

void
data_show(const char *msg, int *ip, int len, int idx)
{
    int	i;
    dbgprintf("%s: ", msg);
    for (i = 0; i < len; i++) {
	fprintf(stderr, "[%d] %d ", idx + i, ip[i]); fflush(stderr);
    }
    fprintf(stderr, "\n");
}

static int
is_dont_care_path(const char *path)
{
    if (care_path[0] && !strncmp(care_path, path, strlen(care_path))) {
	return 0;
    } else {
	return 1;
    }
#if 0
    int rc = 0;
    int i;
    for (i = 0; dont_path[i] != 0; i++) {
	int len = strlen(dont_path[i]);
	if (!strcmp(path, dont_path[i]) /* exact matching */
	    || (i != 0 && !strncmp(path, dont_path[i], len))) {
	    rc = 1; break;
	}
    }
    return rc;
#endif
}

static inline void
rank_init()
{
    if (Myrank < 0) {
	MPI_Comm_size(MPI_COMM_WORLD, &Nprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &Myrank);
	if (Myrank == 0) {
	    char        *cp = getenv("IOMIDDLE_CONFIRM");
	    if (cp && atoi(cp) > 0) {
		fprintf(stderr,
			"IO_MIDDLE is attached:\n"
			"CARE_PATH = %s\n"
			"IO_FORWARDER is %d\n"
			"STATISTICS is %d\n",
			care_path, Fwrdr, _inf.tmr); fflush(stderr);
	    }
	}
    }
}

static inline int
info_flagcheck(int flags)
{
    rank_init();
    return (Myrank ==0) ? flags : (flags & ~O_TRUNC);
}

/*
 * flags and mode are values specified by arguments
 */
static void
info_init(const char *path, int fd, int flags, int mode)
{
    fdinfo	*info = &_inf.fdinfo[fd];

    rank_init();
    info->iofd   = fd;
    info->bufpos = 0;
    info->filpos = 0;
    info->bufcount = 0;
    info->dntcare  = 0;
    info->flags    = flags;
    info->mode   = mode;
    info->trunc = (((flags|mode) & O_TRUNC) == O_TRUNC);

    info->path = malloc(strlen(path) + 1);
    if (info->path == NULL) {
	dbgprintf("%s Cannot allocate memory, size=\n", __func__, strlen(path));
	abort();
    }
    strcpy(info->path, path);
#ifdef STATISTICS
    memset(info->io_time_tot, 0, sizeof(info->io_time_max));
    memset(info->io_time_max, 0, sizeof(info->io_time_max));
    memset(info->io_sz, 0, sizeof(info->io_sz));
    {
	int	i;
	for (i = 0; i < TIMER_MAX; i++) {
	    info->io_time_min[i] = UINT64_MAX;
	}
    }
    Wtmhz = tick_helz(0);
#endif /* STATISTICS */
}

/*
 * Global operation 
 */
static void
buf_init(int fd, int strsize, int frank)
{
    int	strcnt = Nprocs;

    if (Fwrdr) {
	int	fprocs = Nprocs/Fwrdr;	/* number of members cared by one forwarder */
	int	color  = frank / fprocs;
	int	key    = frank % fprocs;

	Cprocs = fprocs;
	if (Cprocs*Fwrdr != Nprocs) {
	    dbgprintf("%s: Forwarder count must divide proc count exactly\n", __func__);
	    abort();
	}
	STAT_BEGIN(fd);
	MPI_CALL(MPI_Comm_split(MPI_COMM_WORLD, color, key, &Clcomm));
	STAT_END(fd, TIMER_SPLIT, 0);
	Color = color;	Crank = key;
	DEBUG(DLEVEL_FWRDR) {
	    dbgprintf("%s: Color(%d) Cprocs(%d) Crank(%d)\n", __func__, Color, Cprocs, Crank);
	}
	{
	    /* 384, 4 fowarder (96), 0, 96, 192, 288 */
	    MPI_Group	group;
	    int	*ranks = malloc(sizeof(int)*Fwrdr);
	    int	i;
	    assert (ranks != NULL);
	    for (i = 0; i < Fwrdr; i++) {
		ranks[i] = fprocs*i;
		if (ranks[i] == Myrank) {
		    Cfwrdr = 1;
		    // dbgprintf("%s: Forwarder(%d)\n", __func__, Myrank);
		}
	    }
	    MPI_CALL(MPI_Comm_group(MPI_COMM_WORLD, &group));
	    MPI_CALL(MPI_Group_incl(group, Fwrdr, ranks, &Cfwgrp));
	    MPI_CALL(MPI_Comm_create(MPI_COMM_WORLD, Cfwgrp, &Cfwcomm));
	    free(ranks);
	}

	if (_inf.mybuflanes != 1) {
	    fprintf(stderr, "io_middle: IOMIDDLE_LANES is set to 1 from %d\n", _inf.mybuflanes);
	}
	_inf.mybufcount = 1; /* set to 1 */
    } else {
	/* _inf.mybuflanes = is set by shell environment variable, IOMIDDLE_LANES */
	_inf.mybufcount = strcnt * _inf.mybuflanes;
    }
    _inf.frank = frank;
    _inf.fdinfo[fd].notfirst = 1;
    _inf.fdinfo[fd].frstrwcall = 1;
    _inf.fdinfo[fd].strsize = strsize;
    _inf.fdinfo[fd].strcnt = strcnt;
    _inf.fdinfo[fd].filoff = _inf.fdinfo[fd].strsize*Myrank;
    _inf.fdinfo[fd].filchklen = strsize * strcnt;
    _inf.fdinfo[fd].buflanes = 0;
    _inf.fdinfo[fd].bufsize = _inf.fdinfo[fd].filchklen;
    _inf.fdinfo[fd].ubuf = malloc(_inf.fdinfo[fd].bufsize*_inf.mybuflanes);
    _inf.fdinfo[fd].sbuf = malloc(_inf.fdinfo[fd].bufsize*_inf.mybuflanes);
    /* for double buffering */
    _inf.fdinfo[fd].dbuf[0] = _inf.fdinfo[fd].sbuf;
    _inf.fdinfo[fd].dbuf[1] = malloc(_inf.fdinfo[fd].bufsize*_inf.mybuflanes);
    IOMIDDLE_IFERROR(
	(_inf.fdinfo[fd].ubuf == NULL || _inf.fdinfo[fd].sbuf == NULL
	 || _inf.fdinfo[fd].dbuf[1] == NULL),
	"%s", "Cannot allocate IO middleware buffer\n");
    memset(_inf.fdinfo[fd].ubuf, 0, _inf.fdinfo[fd].bufsize*_inf.mybuflanes);
    memset(_inf.fdinfo[fd].sbuf, 0, _inf.fdinfo[fd].bufsize*_inf.mybuflanes);
    memset(_inf.fdinfo[fd].dbuf[1], 0, _inf.fdinfo[fd].bufsize*_inf.mybuflanes);
    _inf.fdinfo[fd].filcurb = Frank;
    _inf.fdinfo[fd].filtail = Frank;
    DEBUG(DLEVEL_BUFMGR) {
	dbgprintf("%s: blksize(%d) strsize(%d) strcnt(%d) file_rank(%d)\n", __func__,
		  _inf.fdinfo[fd].filchklen, _inf.fdinfo[fd].strsize, _inf.fdinfo[fd].strcnt, frank);
    }
}

/*
 * Checking if this file descriptor is controlled by this middleware.
 */
static inline int
dontcare_mode_check(int fd, int mode)
{
    if (_inf.init == 0 || _inf.fdinfo[fd].dntcare || _inf.fdinfo[fd].iofd == 0) {
	return 1;
    }
    if (_inf.fdinfo[fd].rwmode == MODE_UNKNOWN) {
	_inf.fdinfo[fd].rwmode = mode;
    }
    IOMIDDLE_IFERROR((_inf.fdinfo[fd].rwmode != mode), "%s",
		     "read and write issued\n");
    return 0;
}

/*
 *  Stripe size is checked or determined
 *	called by lseek/read/write
 */
static inline int
stripe_check_init(int fd, size_t len, int lseek)
{
    int	strsize = 0;
    size_t	exdata[2];

    if (!_inf.fdinfo[fd].notfirst) {
	int	frank;
	if (lseek) {
	    exdata[0] = len; exdata[1] = Myrank;
	    MPI_Allreduce(MPI_IN_PLACE, exdata, 2, MPI_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
	    strsize = exdata[0]/exdata[1];
	    frank = len/strsize;
	    buf_init(fd, strsize, frank);
	} else { /* read/write first */
	    exdata[0] = len; exdata[1] = Myrank;
	    MPI_Allreduce(MPI_IN_PLACE, exdata, 2, MPI_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
	    strsize = exdata[0]/exdata[1];
	    frank = len/strsize;
	    buf_init(fd, strsize, frank);
	}
	DEBUG(DLEVEL_BUFMGR) {
	    dbgprintf("%s: strsize(%d) file_rank(%d) len(%ld)\n", __func__, strsize, frank, len);
	}
    }
    return 0;

#ifdef STRICT_RANK_MAP
    int	rc = 0;
    if (!_inf.fdinfo[fd].notfirst) {
	if (lseek) {
	    if (Myrank == 0) {
		IOMIDDLE_IFERROR((len != 0),
				 "lseek (%ld) is issued before write/read on rank 0\n", len);
		/* len == 0 OK, strsize will be determined at the first read/write call  */
		goto ext;
	    }
	    strsize = len/Myrank;
	} else {
	    IOMIDDLE_IFERROR((Myrank != 0), /* unexpected usage ! */
			     "write/read is issued before lseek offset(%ld)\n",
			     len);
	    strsize = len;
	}
	IOMIDDLE_IFERROR((strsize == 0
		  || (Myrank != 0 && ((len/Myrank)*Myrank != len))),
			 "lseek offset is expected multiple of rank, "
			 "requested offset is %d on rank %d\n",
			 len, Myrank);
	buf_init(fd, strsize);
    } else {
	if (lseek == 0) {
	    if (len != _inf.fdinfo[fd].strsize) {
		dbgprintf("read/write size must be always stripe size: "
			  "requested size(%d) stripe size(%ld)\n",
			  len, _inf.fdinfo[fd].strsize);
		rc = -1;
	    }
	}
    }
ext:
    return rc;
#endif
}

static inline size_t
io_write(int fd, void *buf, size_t size, off64_t pos)
{
    size_t	sz;

    STAT_BEGIN(fd);
    //sz = pwrite(fd, buf, size, pos);
    __real_lseek64(fd, pos, SEEK_SET);
    sz = __real_write(fd, buf, size);
    STAT_END(fd, TIMER_WRITE, size);
    if (sz ==  -1ULL || sz < size) {
	char buf[1024];
	snprintf(buf, 1024, "%s: fd(%d) size(%ld) pos(%ld) path=%s",
		 __func__, fd, size, pos, _inf.fdinfo[fd].path);
	perror(buf);
    }

    return sz;
}

static inline size_t
io_read(int fd, void *buf, size_t size, off64_t pos)
{
    size_t	sz;

    STAT_BEGIN(fd);
    __real_lseek64(fd, pos, SEEK_SET);
    sz = __real_read(fd, buf, size);
    STAT_END(fd, TIMER_READ, size);
    DEBUG(DLEVEL_READ) {
	dbgprintf("%s: READ## fd(%d) pos(%ld), req-size(%ld) read-size(%ld), buf(%p)\n",
		  __func__, fd, pos, size, sz, buf);
    }
    return sz;
}

/*
 * buf_flush
 *  from write: 
 *  from close: 
 * ubuf : user data buffer, contining stripe * bufcount
 * sbuf : system data buffer
 * ubuf --> sbuf per stripe
 *
 * bufcount: # of strips has been written to the buffer
 *   (nprocs == bufcount): buffers are fulled over all processes
 *   (nprocs < bufcount):  ranks, smaller than bufcount, only keep data
 * Variables with prefix "fil" are file view
 *   filpos:  pointer
 *   filcurb: current block number (stripe size)
 *	Each block is the maximum contiguous area in file view.
 *		writing/reading to/from file is performed per block.
 *		filcurb is advanced by stripe count, not +1.
 *   filtail: last block buffered in local
 *   filchklen: chunk length (stripe size * nprocs)
 *		stripe count is equal to nprocs
 *  E.g.
 *	         rank 0	  rank 1     rank 2     rank 3
 *  ubuf   #0#1#2#3   #0#1#2#3   #0#1#2#3   #0#1#2#3
 *         <------------- Exchange ---------------->
 *	sbuf    chunk#0   chunk#1   chunk#2	chunk#3
 */
static size_t
buf_flush(fdinfo *info, int cls)
{
    size_t	cc = -1ULL;
    size_t	sz;
    int		curb = info->filcurb;
    int		strcnt = info->strcnt;
    size_t	strsize = info->strsize;
    size_t	chunksize = info->filchklen;

    if (info->dirty == 0) { /* no need to flush */ return 0; }
    if (Fwrdr) { /* IO forwarders only get data */
	int	forwarder = 0;
	if (info->bufcount != 1 && _inf.mybuflanes != 1) {
	    dbgprintf("%s: bufcount(%d) and mybuflanes(%d) must be 1\n",
		      __func__, info->bufcount, _inf.mybuflanes);
	    abort();
	}
	STAT_BEGIN(info->iofd);
	MPI_CALL(MPI_Gather(info->ubuf, strsize, MPI_BYTE,
			    info->sbuf, strsize, MPI_BYTE,
			    forwarder, Clcomm));
	STAT_END(info->iofd, TIMER_GATHER, strsize*Cprocs);
	if (forwarder == Crank) {
	    size_t	filpos = curb*strsize;
	    size_t	wlen = strsize*Cprocs;
	    // dbgprintf("%s: WRITE curb(%d) filpos(%ld) wlen(%ld)\n", __func__, curb, filpos, wlen);
	    sz = io_issue(WRK_CMD_WRITE, info->iofd, info->sbuf, wlen, filpos);
	    if (sz == 0 || sz == wlen) { /* first io_issue is always zero */
		cc = strsize;
	    } else {
		dbgprintf("%s: ERROR cc(%ld) wlen(%ld)\n", __func__, cc, wlen);
		cc = -1ULL;
	    }
	} else {
	    cc = strsize;
	}
	info->filcurb += strcnt;
    } else { /* All ranks get and write data */
	int	i, this_rank;
	int	my_wlen = 0;
	int	j = 0, uoff = 0, soff = 0;

	if (cls) { /* in order to flush remaining data */
	    info->buflanes += 1;
	}
	/* 0 < info->bufcount <= _inf.mybufcount */
	while (j < info->bufcount) {
	    this_rank = j / _inf.mybuflanes;
	    for (i = 0; i < _inf.mybuflanes && j < info->bufcount; i++, j++) {
		//dbgprintf("%s: j(%d) this_rank(%d) uoff(%d) soff(%d) sbuf(%p)\n",
		// __func__, j, this_rank, uoff, soff, info->sbuf);
		STAT_BEGIN(info->iofd);
		MPI_CALL(
		    MPI_Gather(info->ubuf + uoff, strsize, MPI_BYTE,
			       info->sbuf + soff, strsize, MPI_BYTE,
			       this_rank, MPI_COMM_WORLD));
		STAT_END(info->iofd, TIMER_GATHER, strsize*Nprocs);
		uoff += strsize;
		soff += chunksize;
		info->filcurb += strcnt;
	    }
	    if (this_rank == Myrank) {
		my_wlen = soff; /* saved */
	    }
	    soff = 0;
	}
	if (my_wlen > 0) {
	    int	nblks = strcnt * strcnt * _inf.mybuflanes; /* number of blocks in one flush */
	    int	lblks = strcnt * _inf.mybuflanes; /* number of blocks per one forworder */
	    int	nth = curb / nblks;
	    int	wblks = nth*nblks + lblks*Frank;
	    size_t filpos = strsize * wblks;
	
	    DEBUG(DLEVEL_BUFMGR) {
		dbgprintf("%s: %s WRITE curb(%d) w-blks(%d) filpos(%ld) wlen(%ld) "
			  "nth(%d) nblks(%d) chunksize(%d) Frank(%d)\n",
			  __func__, cls == 0 ?"WRITE_FLUSH":"CLOSE_FLUSH", curb, wblks, filpos,
			  my_wlen, nth, nblks, chunksize, Frank);
	    }
	    sz = io_issue(WRK_CMD_WRITE, info->iofd, info->sbuf, my_wlen, filpos);
	    if (sz == 0 || sz == my_wlen) { /* first io_issue is always zero */
		cc = strsize;
	    } else {
		dbgprintf("%s: ERROR cc(%ld) my_wlen(%ld)\n", __func__, cc, my_wlen);
		cc = -1ULL;
	    }
	} else {
	    int	nblks = strcnt * strcnt * _inf.mybuflanes; /* number of blocks in one flush */
	    int	nth = curb / nblks;
	    size_t filpos = chunksize * (nth*nblks + Frank);
	    DEBUG(DLEVEL_BUFMGR) {
		dbgprintf("%s: %s NOWRITE curb(%d) filpos(%ld) nth(%d) nblks(%d) chunksize(%d) Frank(%d)\n",
			  __func__, cls == 0 ?"WRITE_FLUSH":"CLOSE_FLUSH", curb, filpos, nth, nblks, chunksize, Frank);
	    }
	    cc = strsize;
	}
    }
    io_write_bufupdate(info);
    info->buflanes = 0;
    info->bufcount = 0;
    info->bufpos = 0;
    info->dirty = 0;
    return cc;
}

/*
 * creat system call
 */
int
_iomiddle_creat(const char* path, mode_t mode)
{
    int	fd;

    // fprintf(stderr, "%s: YI*** invoked\n", __func__);
    if (is_dont_care_path(path)) {
	fd = __real_creat(path, mode);
	return fd;
    }
    DEBUG(DLEVEL_HIJACKED|DLEVEL_CONFIRM) {
	fprintf(stderr, "%s DO-CARE path=%s\n", __func__, path);
    }
    fd = __real_creat(path, mode);
    if (fd >= 0) {
	if (_inf.init == 0) {
	    extern void	_myhijack_ini2();
	    _myhijack_ini2();
	    _inf.init = 1;
	}
	info_init(path, fd, 0, mode);
    }
    return fd;
}

/*
 * open system call
 */
int
_iomiddle_open(const char *path, int flags, ...)
{
    int	fd;
    int mode = 0;
    int dont_care = is_dont_care_path(path);

    DEBUG(DLEVEL_ALL) {
	fprintf(stderr, "[%d] %s path=%s\n", Myrank, __func__, path);
    }
    if (flags & O_CREAT) {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, int);
        va_end(arg);
    }
    if (dont_care) {
	fd = __real_open(path, flags, mode);
	if (_inf.init == 0) { /* still don't care */
	    return fd;
	}
    } else {
	int umode, uflags;
	uint64_t	tm_start;

	if (_inf.init == 0) {
	    extern void	_myhijack_ini2();
	    _myhijack_ini2();
	    _inf.init = 1;
	}
	umode  = info_flagcheck(mode);
	uflags = info_flagcheck(flags);
	tm_start = tick_time();
	fd = __real_open(path, uflags, umode);
	_inf.fdinfo[fd].io_tm_start = tm_start;
	STAT_END(fd, TIMER_OPEN, 0);
    }
    if (fd < 0) goto err;
    if (dont_care) {
	_inf.fdinfo[fd].notfirst = 1;
	_inf.fdinfo[fd].dntcare = 1;
    } else {
	//fprintf(stderr, "[%d] open() DO-CARE file fd(%d) path(%s)\n",
	//Myrank, fd, path);
	DEBUG(DLEVEL_HIJACKED|DLEVEL_CONFIRM) {
	    dbgprintf("[%d] open() DO-CARE file fd(%d) path(%s)\n",
		      Myrank, fd, path);
	}
	worker_init(Wenbflg);
	info_init(path, fd, flags, mode);
	io_init(Wenbflg, fd);
    }
err:
    return fd;
}

/*
 * close system call
 */
int
_iomiddle_close(int fd)
{
    int	rc = 0;
    fdinfo	*info;

    // fprintf(stderr, "%s: YI*** invoked fd(%d)\n", __func__, fd);
    /* file dscriptor 0 is never used */
    if (_inf.init == 0 || _inf.fdinfo[fd].dntcare || _inf.fdinfo[fd].iofd == 0) {
	rc = __real_close(fd);
	return rc;
    }
    info =  &_inf.fdinfo[fd];
    DEBUG(DLEVEL_HIJACKED) { fprintf(stderr, "%s DO-CARE fd(%d)\n", __func__, fd); }
    if (info->bufcount > 0) {
	size_t	sz;
	DEBUG(DLEVEL_BUFMGR) {
	    dbgprintf("%s: bufcount(%d)\n", __func__, info->bufcount);
	}
	sz = buf_flush(info, 1);
	/* FIXME: how this error propagates ? */
	rc = (sz == 0 || sz == _inf.fdinfo[fd].strsize) ? 0 : -1;
	if (rc == -1) {
	    dbgprintf("%s: ERROR sz = %ld (frstrwcall=%d strsize(%ld))\n",
		      __func__, sz, _inf.fdinfo[fd].frstrwcall, _inf.fdinfo[fd].strsize);
	}
    }
    /* io_fin() first to synchronize worker */
    io_fin(fd);
    if (_inf.reqtrunc && info->trunc) {
	off64_t	filpos = info->filpos;
	if (Myrank == 0) {
	    /* Rank 0 only has created or opened this file with the O_TRUNC flag
	     * if specified. */
	    MPI_Reduce(&info->filpos, &filpos, 1, MPI_UNSIGNED_LONG_LONG,
		       MPI_MAX, 0, MPI_COMM_WORLD);
	    if (filpos != info->filpos) {
		__real_lseek64(info->iofd, filpos, SEEK_SET);
	    }
	    STAT_BEGIN(fd);
	    rc = __real_close(fd);
	    STAT_END(fd, TIMER_CLOSE, 0);
	} else {
	    STAT_BEGIN(fd);
	    rc = __real_close(fd);
	    STAT_END(fd, TIMER_CLOSE, 0);
	    MPI_Reduce(&info->filpos, &filpos, 1, MPI_UNSIGNED_LONG_LONG,
		       MPI_MAX, 0, MPI_COMM_WORLD);
	}
    } else {
	rc = __real_close(fd);
    }
#ifdef STATISTICS
    stat_show(info);
#endif /* STATISTICS */
    info->attrall = 0;
    info->iofd = -1;
    free(info->ubuf);
    free(info->sbuf);
    info->ubuf = 0;
    info->sbuf = 0;
    return rc;
}

/*
 * write system call
 *	DO not call printf/fprintf stuffs inside this function for debugging.
 */
ssize_t
_iomiddle_write(int fd, const void *buf, size_t len)
{
    size_t	rc = len;
    fdinfo	*info;

    // fprintf(stderr, "%s: invoked\n", __func__);
    if (dontcare_mode_check(fd, MODE_WRITE)) {
	rc = __real_write(fd, buf, len);
	return rc;
    }
    DEBUG(DLEVEL_HIJACKED) {
	DEBUGWRITE("[%d] %s DO-CARE fd(%d) len(%ld)\n",
		   Myrank, __func__, fd, len);
    }
    info = &_inf.fdinfo[fd];
    if (stripe_check_init(fd, len, 0)) {
	DEBUG(DLEVEL_HIJACKED) { info_show(fd, __func__); }
	abort();
    }
    DEBUG(DLEVEL_BUFMGR) {
	dbgprintf("%s filcurb(%d)\n", __func__, info->filcurb);
    }
    IOMIDDLE_IFERROR((len != info->strsize),
		     "write length must be the stripe size. "
		     "len(%ld) stripe size(%d)\n", len, info->strsize);
    if (info->frstrwcall) {
	io_setup(WRK_CMD_WRITE, fd, info->bufsize, 0); /* pos is ignored */
	info->frstrwcall = 0;
    }
    memcpy(info->ubuf + info->bufpos, buf, len);
    info->bufpos += len; info->bufcount++;
    info->filpos += len;
    info->dirty = 1;
    DEBUG(DLEVEL_BUFMGR) {
	dbgprintf("%s: bufcount(%d) mybufcount(%d) len(%ld) "
		  "info->strsize(%d)\n",
		  __func__, info->bufcount, _inf.mybufcount, len, info->strsize);
    }
    if (info->bufcount == _inf.mybufcount) {
	DEBUG(DLEVEL_BUFMGR) {
	    dbgprintf("%s: info->buflanes(%d) mybuflanes(%d)\n", __func__, info->buflanes, _inf.mybuflanes);
	}
	rc = buf_flush(info, 0);
	if (rc != len) {
	    dbgprintf("%s: ERROR here rc(%ld)\n", __func__, rc);
	}
    }
    {  /* next expected block */
	size_t	nextpos = (info->filpos - len) + info->filchklen;
	int	blk = POS_TO_BLK(nextpos, info->strsize);
	info->filtail = blk;
    }
    if (rc != len) {
	fprintf(stderr, "%s: ERROR return rc(%ld)\n", __func__, rc);
    }
    return rc;
}

/*
 * read system call
 */
ssize_t
_iomiddle_read(int fd, void *buf, size_t len)
{
    int	j;
    size_t	cc, rc = len;
    fdinfo *info;

    // fprintf(stderr, "%s: YI*** invoked fd(%d)\n", __func__, fd); 
    if (dontcare_mode_check(fd, MODE_READ)) {
	rc = __real_read(fd, buf, len);
	return rc;
    }
    DEBUG(DLEVEL_HIJACKED|DLEVEL_READ) {
	dbgprintf("%s DO-CARE fd(%d) len(%ld)\n", __func__, fd, len);
    }
    if (stripe_check_init(fd, len, 0)) {
	DEBUG(DLEVEL_HIJACKED) { info_show(fd, __func__); }
	abort();
    }
    info = &_inf.fdinfo[fd];
    if (info->frstrwcall || info->bufcount == info->bufend) {
	int	curb = info->filcurb;
	int	strcnt = info->strcnt;
	size_t	chunksize = info->filchklen;
	size_t	strsize = info->strsize;
	int	nblks = strcnt * _inf.mybuflanes; /* number of blocks in one read */
	int	lblks = strcnt * _inf.mybuflanes; /* number of blocks per one forworder */
	int	nth = curb / nblks;
	int	rblks = nth*nblks + lblks*Frank;
	size_t	filpos = strsize * rblks;
	size_t	filsize = strsize * lblks;
	int	soff, uoff;

	if (info->frstrwcall) { /* the first-time read/write call */
	    io_setup(WRK_CMD_READ, fd, filsize, filpos);
	    info->frstrwcall = 0;
	}
	DEBUG(DLEVEL_BUFMGR|DLEVEL_READ) {
	    dbgprintf("%s:\t READ fillen(%ld) filpos(%ld) curb(%d) "
		      "strcnt(%d) rblks(%d) nblks(%d) nth(%d) strsize(%d)\n",
		      __func__, filsize, filpos, curb, strcnt, rblks, nblks, nth, strsize);
	}
	cc = io_issue(WRK_CMD_READ, info->iofd, &info->sbuf, filsize, filpos);
	DEBUG(DLEVEL_BUFMGR|DLEVEL_READ) {
	dbgprintf("%s:\t\t READ cc(%ld) sbuf(%p)\n", __func__, cc, info->sbuf);
	}
	{
	    size_t	exdata[2];
	    if ((int64_t) cc < 0) {
		exdata[0] = 0; exdata[1] = -1ULL;
	    } else {
		exdata[0] = cc/info->strsize; exdata[1] = 0;
	    }
	    MPI_CALL(
		MPI_Allreduce(MPI_IN_PLACE, exdata, 2, MPI_LONG_LONG,
			      MPI_SUM, MPI_COMM_WORLD));
	    if (exdata[0] == 0) { /* no data is read */
		dbgprintf("%s: NO DATA IS READ\n", __func__);
		rc = 0;	goto ext;
	    }
	    if ((int64_t) exdata[1] < 0) { /* Errors happen */
		dbgprintf("%s: READ ERROR\n", __func__);
		rc = 0;	goto ext;
	    }
	    info->bufend = exdata[0]/info->strcnt; /* determined by actual read size */
	}
	soff = 0; uoff = 0; j = 0;
	while (j < info->bufend) {
	    int	i;
	    int this_rank = j / _inf.mybuflanes;
	    for (i = 0; i < _inf.mybuflanes && j < info->bufend; i++, j++) {
		// dbgprintf("%s: j(%d) i(%d) this_rank(%d) soff(%d) uoff(%d) len(%d)\n",
		// __func__, j, i, this_rank, soff, uoff, info->strsize);
		STAT_BEGIN(info->iofd);
		MPI_CALL(
		    MPI_Scatter(info->sbuf + soff,
				info->strsize, MPI_BYTE,
				info->ubuf + uoff, info->strsize, MPI_BYTE,
				this_rank, MPI_COMM_WORLD));
		STAT_END(info->iofd, TIMER_SCATTER, strsize*Nprocs);
		uoff += info->strsize;
		soff += chunksize;
	    }
	    soff = 0;
	}
	info->bufpos = 0; info->bufcount = 0;
    }
    DEBUG(DLEVEL_READ) {
	dbgprintf("%s:\t bufpos(%d) len(%ld)\n", __func__, info->bufpos, len);
    }
    memcpy(buf, info->ubuf + info->bufpos, len);
    info->bufpos += len;
    info->bufcount += 1;
    info->filcurb += info->strcnt;
    info->filtail += info->strcnt; /* next expected tail pointer */
    info->filpos += len;
ext:
    DEBUG(DLEVEL_BUFMGR|DLEVEL_READ) {
	dbgprintf("%s:\t RETURN tailblks(%d) bufcount(%d) bufpos(%d)\n",
		  __func__, info->filtail, info->bufcount, info->bufpos);
    }
    return rc;
}

static off64_t
lseek_general(int fd, off64_t reqfilpos)
{
    off64_t	rc;
    fdinfo *info;

    // dbgprintf("%s: fd(%d) off(%ld)\n", __func__, fd, reqfilpos);
    if (stripe_check_init(fd, reqfilpos, 1)) {
	DEBUG(DLEVEL_HIJACKED) { info_show(fd, __func__); }
	abort();
    }
    /* file position is now reqfilpos */
    info = &_inf.fdinfo[fd];
    rc = info->filpos = reqfilpos;
    if (!(Frank == 0 && reqfilpos == 0)) {
	int	blks   = POS_TO_BLK(reqfilpos, info->strsize);

	DEBUG(DLEVEL_READ) {
	    dbgprintf("%s: reqfilpos(%ld) blks(%d) tailblks(%d) Frank(%d)\n",
		      __func__, reqfilpos, blks, _inf.fdinfo[fd].filtail, Frank);
	}
	/* checking if this file position is the next */
	IOMIDDLE_IFERROR((blks != _inf.fdinfo[fd].filtail),
			 "lseek_general: offset is out of block area: %ld "
			 " (blks(%d) tailblks(%d))\n",
			 reqfilpos, blks, _inf.fdinfo[fd].filtail);
    }
    return rc;
}

/*
 * lseek system call
 */
off_t
_iomiddle_lseek(int fd, off_t offset, int whence)
{
    off_t	rc = 0;
    off_t	reqfilpos;

    // fprintf(stderr, "%s: YI*** invoked\n", __func__);    
    if (_inf.init == 0 || _inf.fdinfo[fd].dntcare || _inf.fdinfo[fd].iofd == 0) {
	rc = __real_lseek(fd, offset, whence);
	return rc;
    }
    DEBUG(DLEVEL_HIJACKED) {
	dbgprintf("%s DO-CARE fd(%d) offset(%ld) whence(%d)\n",
		  __func__, fd, offset, whence);
    }
    switch (whence) {
    case SEEK_SET:
	reqfilpos = offset;
	break;
    case SEEK_CUR:
	reqfilpos = _inf.fdinfo[fd].filpos + offset;
	break;
    case SEEK_END:
	fprintf(stderr, "lseek64: whence(%d) is not allowed\n", whence);
	abort();
    default:
	fprintf(stderr, "lseek64: unknown whence value %d\n", whence);
	abort();
    }
    rc = lseek_general(fd, (off_t) reqfilpos);
    return rc;
}

/*
 * lseek64 system call
 */
off64_t
_iomiddle_lseek64(int fd, off64_t offset, int whence)
{
    off64_t	rc = 0;
    off64_t	reqfilpos;

    // fprintf(stderr, "%s: YI*** invoked\n", __func__);    
    if (_inf.init == 0 || _inf.fdinfo[fd].dntcare || _inf.fdinfo[fd].iofd == 0) {
	rc = __real_lseek64(fd, offset, whence);
	return rc;
    }
    DEBUG(DLEVEL_HIJACKED) {
	dbgprintf("%s DO-CARE fd(%d) offset(%ld) whence(%d)\n",
		  __func__, fd, offset, whence);
    }
    switch (whence) {
    case SEEK_SET:
	reqfilpos = offset;
	break;
    case SEEK_CUR:
	reqfilpos = _inf.fdinfo[fd].filpos + offset;
	break;
    case SEEK_END:
	fprintf(stderr, "lseek64: whence(%d) is not allowed\n", whence);
	abort();
    default:
	fprintf(stderr, "lseek64: unknown whence value %d\n", whence);
	abort();
    }
    rc = lseek_general(fd, reqfilpos);
    return rc;
}

#include <sys/time.h>
#include <sys/resource.h>

/*
 * _myhijack_init:
 *  This function is invoked when one of the hijacked system call
 *  is invoked by the user.
 *	See hooklib.c
 */
void
_myhijack_init()
{
    char *cp;

    /* here we do not call any library calls except getenv() and strcpy() */
    cp = getenv("IOMIDDLE_DISABLE");
    if (cp && atoi(cp) == 1) {
	fprintf(stderr, "%s: init is called. diabled\n", __func__);
	return;
    }
    cp = getenv("IOMIDDLE_DEBUG");
    if (cp && atoi(cp) > 0) {
	_inf.debug = atoi(cp);
    }
    cp = getenv("IOMIDDLE_CARE_PATH");
    if (cp) {
	strcpy(care_path, cp);
    } else {
	printf("IOMIDDLE_CARE_PATH must be specified\n");
	exit(-1);
    }
    cp = getenv("IOMIDDLE_FORWARDER");
    if (cp && atoi(cp) > 0) {
	_inf.fwrdr = atoi(cp);
    }
    cp = getenv("IOMIDDLE_STAT");
    if (cp && atoi(cp) > 0) {
	_inf.tmr = atoi(cp);
    }
    _inf.init = 0;
    /* here are hijacked system call registration */
    _hijacked_creat = _iomiddle_creat;
    _hijacked_open = _iomiddle_open;
    _hijacked_close = _iomiddle_close;
    _hijacked_read = _iomiddle_read;
    _hijacked_lseek = _iomiddle_lseek;
    _hijacked_lseek64 = _iomiddle_lseek64;
    _hijacked_write = _iomiddle_write;
}

void
_myhijack_ini2()
{
    int	i;
    char	*cp;
    struct rlimit rlim;
    size_t	sz;

    // fprintf(stderr, "%s: YI!!!! invoked\n", __func__);
    cp = getenv("IOMIDDLE_TRUNC");
    if (cp && atoi(cp) > 0) {
	_inf.reqtrunc = 1;
    }
    cp = getenv("IOMIDDLE_WORKER");
    if (cp && atoi(cp) > 0) {
	Wenbflg = 1;	/* enable flag is set */
    }
    cp = getenv("IOMIDDLE_LANES");
    if (cp) {
	i = atoi(cp);
	if (i >= 1) {
	    /* mybuflanes will be checked again in buf_init()
	     * which will be invoked at the first lseek() or write() call */
	    _inf.mybuflanes = i;
	} else {
	    fprintf(stderr, "IOMIDDLE_LANES must be larger than 0, set it to 1\n");
	    _inf.mybuflanes = 1;
	}
    } else {
	_inf.mybuflanes = 1;
    }
    DEBUG(DLEVEL_CONFIRM) {
	printf("IOMIDDLE_CARE_PATH = %s\n", care_path);
    }
    DEBUG(DLEVEL_ALL) {
	printf("%s: init is called. debug(%d)\n", __func__, _inf.debug);
    }
    getrlimit(RLIMIT_NOFILE, &rlim);
    Myrank = -1;
    _inf.fdlimit = rlim.rlim_cur;
    sz = sizeof(fdinfo)*_inf.fdlimit;
    _inf.fdinfo = malloc(sz);
    IOMIDDLE_IFERROR((_inf.fdinfo == 0), "%s",
		     "Cannot allocate working memory\n");
    memset(_inf.fdinfo, 0, sz);
    /* Checking don't care fd's */
    for (i = 0; i < 3; i++) {
	_inf.fdinfo[i].dntcare = 1;
	_inf.fdinfo[i].notfirst = 1;
    }
}

/***************************************************************
 * Nonblocking read/write IO implemented by pthread
 *************************************************************/
#include <pthread.h>

/*
 * This function is used for just the function address
 */
static inline size_t
io_workerfin(int fd, void *buf, size_t size, off64_t pos)
{
    dbgprintf("%s dummy. never called\n", __func__);
    exit(0);
}

static inline void
worker_sigdone(size_t sz)
{
    DEBUG(DLEVEL_WORKER) {
	dbgprintf("%s: cmd(%s) sz(%ld)\n", __func__, (Wcmd == WRK_CMD_WRITE) ? "WRITE" : "READ", sz);
    }
    Wcret = sz;
    Wcmd = WRK_CMD_DONE;
    pthread_mutex_unlock(&Wmtx);
    pthread_cond_signal(&Wcnd);
}

void *
worker(void *p)
{
    size_t	sz;
    DEBUG(DLEVEL_WORKER) {
	dbgprintf("%s: invoked \n", __func__);
    }
    pthread_barrier_wait(&Wsync);
    DEBUG(DLEVEL_WORKER) {
	dbgprintf("%s: starts \n", __func__);
    }
    while (Wsig == 0) {
	pthread_mutex_lock(&Wmtx);
	if (Wcmd == WRK_CMD_DONE) {
	    DEBUG(DLEVEL_WORKER) {
		dbgprintf("%s: wait for request\n", __func__);
	    }
	    pthread_cond_wait(&Wcnd, &Wmtx);
	}
	DEBUG(DLEVEL_WORKER) {
	    dbgprintf("%s: cmd(%s) Wcfd(%d) Wcbuf(%p) Wcsize(%ld) Wcpos(%ld) Wwpos(%ld)\n",
		      __func__, (Wcmd == WRK_CMD_WRITE) ? "WRITE" : "READ",
		      Wcfd, Wcbuf, Wcsiz, Wcpos, Wwpos);
	}
	if (Wcmd == WRK_CMD_WRITE) {
	    sz = io_write(Wcfd, Wcbuf, Wcsiz, Wcpos);
	    worker_sigdone(sz); /* signal to the client, go ahead */
	} else if (Wcmd == WRK_CMD_READ) {
	    /*
	     * The first request has been already set up in io_setup()
	     */
	    if (Wcpos != Wwpos) {
		dbgprintf("%s: Unexpected file position: request(%ld) assume(%ld)\n", __func__, Wcpos, Wwpos);
	    }
	    if (Wcsiz != Wwsiz) {
		dbgprintf("%s: Unexpected read size: request(%ld) assume(%ld)\n", __func__, Wcsiz, Wwsiz);
	    }
	    /* worker is still working here */
	    Wwpos += Wwlen;
	    // dbgprintf("%s:\t IO_READ file-pos(%ld), size(%ld) Wtiktok(%d)\n", __func__, Wwpos, Wwsiz, Wtiktok);
	    Wwret = io_read(Wcfd, _inf.fdinfo[Wcfd].dbuf[Wtiktok], Wwsiz, Wwpos);
	    Wcpos = Wwpos;
	    // dbgprintf("%s:\t\t Wwret(%ld) Wwpos(%ld) Wwsiz(%ld)\n", __func__, Wwret, Wwpos, Wwsiz);
	    worker_sigdone(Wwret); /* signal to the client, go ahead */
	} else if (Wcmd == WRK_CMD_FIN) {
	    DEBUG(DLEVEL_WORKER) {
		dbgprintf("%s: Exiting\n", __func__);
	    }
	    pthread_exit(0);
	} else {
	    dbgprintf("%s: internal error\n", __func__);
	    abort();
	}
	DEBUG(DLEVEL_WORKER) {
	    dbgprintf("%s: done\n", __func__);
	}
    }
    DEBUG(DLEVEL_WORKER) {
	dbgprintf("worker ends with signal (%d)\n", Wsig);
    }
    return NULL;
}

static inline size_t
io_issue(io_cmd cmd, int fd, void *buf, size_t size, off64_t pos)
{
    size_t	sz;
    DEBUG(DLEVEL_WORKER) {
	dbgprintf("%s: cmd(%s) fd(%d) buf(%p) size(%ld) pos(%ld)\n",
		  __func__, (cmd == WRK_CMD_WRITE) ? "WRITE": "READ", fd, buf, size, pos);
    }
    if (Wenable && Wcfd == _inf.fdinfo[fd].iofd) {
	pthread_mutex_lock(&Wmtx);
	if (Wcmd != WRK_CMD_DONE) {
	    DEBUG(DLEVEL_WORKER) {
		dbgprintf("%s: wait for worker ready.\n",  __func__);
	    }
	    pthread_cond_wait(&Wcnd, &Wmtx);
	}
	DEBUG(DLEVEL_WORKER) {
	    dbgprintf("%s: requesting. prev ret(%ld)\n", __func__, Wcret);
	}
	/*
	 * The return value is the last operation. Be sure that the return
	 * value of the first call is always 0.
	 * The client must care of this condition.
	 */
	sz = Wcret;
	if (cmd == WRK_CMD_READ) {
	    if (pos != Wcpos) {
		dbgprintf("%s: something wrong: request-pos(%ld) assumed-pos(%ld)\n", __func__, pos, Wcpos);
	    }
	    *(void**) buf = _inf.fdinfo[fd].dbuf[Wtiktok];
	    /* Next read buffer is updated */
	    Wtiktok ^= 1;
	}
	/* request */
	Wcmd = cmd; Wcbuf = buf; Wcsiz = size; Wcpos = pos;
	pthread_mutex_unlock(&Wmtx);
	pthread_cond_signal(&Wcnd);
	DEBUG(DLEVEL_WORKER) {
	    dbgprintf("%s: done\n", __func__);
	}
    } else {
	/*
	 * Blocking IO for this request because
	 * worker is not working or this fd is not cared by the worker
	 */
	DEBUG(DLEVEL_WORKER) {
	    dbgprintf("%s: Blocking IO\n", __func__);
	}
	if (cmd == io_read) {
	    /* buf in io_read is pointer of pointer */
	    sz = cmd(fd, *(void**)buf, size, pos);
	} else if (cmd == io_write) {
	    sz = cmd(fd, buf, size, pos);
	} else {
	    dbgprintf("%s: internal error\n", __func__);
	    abort();
	}
    }
    return sz;
}

static void
io_init(int is_worker_enable, int fd)
{
    if (Wcfd != 0) {
	dbgprintf("%s: fd=%d is cared by the current worker\n", __func__, fd);
	return;
    }
    if (is_worker_enable) {
	//dbgprintf("%s: Initialize for fd=%d worker(%d)\n", __func__, fd, is_worker_enable);
	Wcmd = WRK_CMD_DONE;
	Wcfd = fd;
	Wcbuf = 0; Wcsiz = 0; Wcpos = 0;
	Wenable = is_worker_enable;
    }
}

static int
io_setup(io_cmd cmd, int fd, size_t siz, off64_t pos)
{
    fdinfo	*info;

    DEBUG(DLEVEL_READ) {
	dbgprintf("%s: cmd(%s) fd(%d) siz(%ld) pos(%ld)\n",
		  __func__, (cmd == WRK_CMD_WRITE) ? "WRITE" : "READ", fd, siz, pos);
    }
    if (!Wenable) return 0;
    if (fd != Wcfd) {
	fprintf(stderr, "%s: fd (%d) is handled by the worker.\n", __func__, fd);
	return -1;
    }
    info = &_inf.fdinfo[fd];
    if (cmd == WRK_CMD_WRITE) {
	Wcret = Wwret = 0; Wwsiz = info->bufsize; Wtiktok = 0;
    } else if (cmd == WRK_CMD_READ) {
	int	strcnt = info->strcnt;
	size_t	strsize = info->strsize;
	/* initial worker variables */
	Wwpos = Wcpos = pos; Wtiktok = 0;
	Wcret = Wwret = Wwsiz = strsize * strcnt * _inf.mybuflanes;
	Wwlen = strsize * strcnt * strcnt * _inf.mybuflanes;
	DEBUG(DLEVEL_READ) {
	    dbgprintf("%s: Wcpos(%d) Wwpos(%d) Wwlen(%d) Wcret(%ld) Wsiz(%ld) bufsize(%d) lanes(%d)\n", __func__, Wcpos, Wwpos, Wwlen, Wcret, Wwsiz, info->bufsize, _inf.mybuflanes);
	}
	/* prefetching here */
	io_read(info->iofd, info->sbuf, Wwsiz, pos);
    } else {
	fprintf(stderr, "%s: internal error: unknown command %p\n", __func__, cmd);
	abort();
    }
    DEBUG(DLEVEL_READ) {
	dbgprintf("%s:\t return 0\n", __func__);
    }
    return 0;
}

static void
io_write_bufupdate(fdinfo *info)
{
    Wtiktok ^= 1;
    info->sbuf = info->dbuf[Wtiktok];
}

static void
io_fin(int fd)
{
    if (fd != Wcfd) {
	return;
    }
    DEBUG(DLEVEL_WORKER) {
	dbgprintf("%s: finalzation of file IO for fd(%d)\n", __func__, fd);
    }
    pthread_mutex_lock(&Wmtx);
    if (Wcmd != WRK_CMD_DONE) {
	DEBUG(DLEVEL_WORKER) {
	    dbgprintf("%s: still worker is working\n", __func__);
	}
	pthread_cond_wait(&Wcnd, &Wmtx);
    }
    pthread_mutex_unlock(&Wmtx);
    // dbgprintf("%s: worker is idle\n", __func__);
    Wcfd = 0;
}

void
worker_init(int is_worker_enable)
{
    int	cc;

    if (is_worker_enable == 0) return;
    atexit(worker_fin);
    pthread_mutex_init(&Wmtx, NULL);
    pthread_cond_init(&Wcnd, NULL);
    pthread_barrier_init(&Wsync, 0, 2);
    cc = pthread_create(&Wthid, NULL, worker,  0);
    if (cc != 0) {
	perror("worker_init"); exit(-1);
    }
    /* wait for worker ready */
    DEBUG(DLEVEL_WORKER) {
	dbgprintf("%s: wait for worker ready\n", __func__);
    }
    pthread_barrier_wait(&Wsync);
    DEBUG(DLEVEL_WORKER) {
	dbgprintf("%s: DONE.\n", __func__);
    }
}

void
worker_fin()
{
    void	*retval;
    
    pthread_mutex_lock(&Wmtx);
    if (Wcmd != WRK_CMD_DONE) {
	DEBUG(DLEVEL_WORKER) {
	    dbgprintf("%s: still worker is working\n", __func__);
	}
	pthread_cond_wait(&Wcnd, &Wmtx);
    }
    Wcmd = WRK_CMD_FIN;
    pthread_mutex_unlock(&Wmtx);
    pthread_cond_signal(&Wcnd);
    pthread_join(Wthid, &retval);
    DEBUG(DLEVEL_WORKER) {
	dbgprintf("%s: FIN (%lx).\n", __func__, retval);
    }
}

#ifdef STATISTICS
#if 0
#include <math.h>
static void
stv_time(uint64_t *data, uint64_t excld, float *mean, float *stv)
{
    int	i, ent = 0;
    uint64_t	sum = 0;
    double	mn, fsum = 0;
    
    for (i = 0; i < Nprocs; i++) {
	if (data[i] > 0) { sum += data[i]; ent++; }
    }
    mn = (double) sum/ent;
    *mean = mn;
    for (i = 0; i < Nprocs; i++) {
	if (data[i] != excld) {
	    fsum += ((double) data[i] - mn) * ((double) data[i] - mn);
	}
    }
    *stv = (float) (sqrt(fsum/(float)ent) / (double)Wtmhz);
}
#endif

static void
stat_show(fdinfo *info)
{
    int	i;
    //uint64_t	*data = malloc(sizeof(uint64_t)*Fwrdr);
    uint64_t	tm_tot[TIMER_MAX], tm_max[TIMER_MAX], tm_min[TIMER_MAX], io_sz[TIMER_MAX];

    if (_inf.tmr == 0) return;

    if (Cfwrdr) {
	if (_inf.tmr == 2) {
	    fprintf(stderr, "@[%d], **************** STATISTICS (%s) Per Forwarder **************************\n",
		    Myrank, info->path);
	    fprintf(stderr, "@[%d], name, total time(sec), max time(sec), min time(sec), total data size(MiB)\n",
		    Myrank);
	    for (i = 0; i < TIMER_MAX; i++) {
		dbgprintf("@[%d], %11s, %12.9f, %12.9f, %12.9f, %12.9f\n", Myrank,
			  timer_str[i], TIMER_SECOND(info->io_time_tot[i]),
			  TIMER_SECOND(info->io_time_max[i]), TIMER_SECOND(info->io_time_min[i]),
			  SIZE_MiB(info->io_sz[i]));
	    }
	}
	memset(tm_tot, 0, sizeof(tm_tot)); memset(tm_max, 0, sizeof(tm_max));
	memset(tm_min, 0, sizeof(tm_min)); memset(io_sz, 0, sizeof(io_sz));
	MPI_Reduce(info->io_time_tot, tm_tot, TIMER_MAX, MPI_LONG_LONG, MPI_MAX, 0, Cfwcomm);
	MPI_Reduce(info->io_time_max, tm_max, TIMER_MAX, MPI_LONG_LONG, MPI_MAX, 0, Cfwcomm);
	MPI_Reduce(info->io_time_min, tm_min, TIMER_MAX, MPI_LONG_LONG, MPI_MAX, 0, Cfwcomm);
	MPI_Reduce(info->io_sz, io_sz, TIMER_MAX, MPI_LONG_LONG, MPI_MAX, 0, Cfwcomm);
    }
    if (Myrank == 0) { /* rank 0 must be a forwarder */
	fprintf(stderr, "@, ************ STATISTICS (%s) Maximum values  of all forwarders *************\n", info->path);
	fprintf(stderr, "@, name, total time(sec), max time(sec), min time(sec), total data size(MiB)\n");
	for (i = 0; i < TIMER_MAX; i++) {
	    dbgprintf("@, %11s, %12.9f, %12.9f, %12.9f, %12.9f\n",
		      timer_str[i], TIMER_SECOND(tm_tot[i]),
		      TIMER_SECOND(tm_max[i]), TIMER_SECOND(tm_min[i]), SIZE_MiB(io_sz[i]));
	}
    }
#if 0
    MPI_Gather(info->io_time_tot[i], 1, MPI_LON_LONG, data, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    dbgprintf("@, **************** STATISTICS [%d] ***********************\n", Color);
    dbgprintf("@, name, total time(sec), max time(sec), min time(sec), total data size(MiB)\n");
    for (i = 0; i < TIMER_MAX; i++) {
	dbgprintf("@, %11s, %12.9f, %12.9f, %12.9f, %12.9f\n",
		  timer_str[i], TIMER_SECOND(info->io_time_tot[i]),
		  TIMER_SECOND(info->io_time_max[i]), TIMER_SECOND(info->io_time_min[i]),
		  SIZE_MiB(info->io_sz[i]));
    }
#endif
}
#endif /* STATISTICS */
