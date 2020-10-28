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
 *	creat, open, close, read, lseek64, write
 * Shell environment:
 *	IOMIDDLE_CARE_PATH 
 *	   -- file path taken care by this middleware.
 *	     The user must specify this variable.
 *	IOMIDDLE_LANES
 *	   -- Speficfy the number of lanes.
 *	     one lane is (stripsize * stripe count)
 *	IOMIDDLE_WORKER
 *	   -- if specify, asynchronous I/O read/writer is performed 
 *	      by a worker thread.
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

static struct ioinfo _inf;
static char	care_path[PATH_MAX];

static void	worker_init();
static void	worker_fin();
static void	io_init(int flg, int fd);
static void	io_fin(int fd);
static size_t	io_issue(size_t (*cmd)(int, void*, size_t, off64_t),
			 int fd, void *buf, size_t size, off64_t pos);

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

static int
dbgprintf(const char *fmt, ...)
{
    va_list	ap;
    int		rc;

    fprintf(stderr, "[%d] ", Myrank);
    va_start(ap, fmt);
    rc = vfprintf(stderr, fmt, ap);
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
	       _inf.fdinfo[fd].filblklen, _inf.mybufcount);
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
}

static void
buf_init(int fd, int strsize)
{
    int	strcnt = Nprocs;

    _inf.mybufcount = strcnt;
    /* _inf.mybuflanes = is set by shell environment variable */
    _inf.fdinfo[fd].notfirst = 1;
    _inf.fdinfo[fd].frstrwcall = 1;
    _inf.fdinfo[fd].strsize = strsize;
    _inf.fdinfo[fd].strcnt = strcnt;
    _inf.fdinfo[fd].filoff = _inf.fdinfo[fd].strsize*Myrank;
    _inf.fdinfo[fd].filblklen = strsize * strcnt;
    _inf.fdinfo[fd].buflanes = 0;
    _inf.fdinfo[fd].bufsize = _inf.fdinfo[fd].filblklen;
    _inf.fdinfo[fd].ubuf = malloc(_inf.fdinfo[fd].bufsize*_inf.mybuflanes);
    _inf.fdinfo[fd].sbuf = malloc(_inf.fdinfo[fd].bufsize*_inf.mybuflanes);
    IOMIDDLE_IFERROR(
	(_inf.fdinfo[fd].ubuf == NULL || _inf.fdinfo[fd].sbuf == NULL),
	"%s", "Cannot allocate IO middleware buffer\n");
    memset(_inf.fdinfo[fd].ubuf, 0, _inf.fdinfo[fd].bufsize*_inf.mybuflanes);
    memset(_inf.fdinfo[fd].sbuf, 0, _inf.fdinfo[fd].bufsize*_inf.mybuflanes);
    _inf.fdinfo[fd].filcurb = Myrank;
    _inf.fdinfo[fd].filtail = Myrank;
    DEBUG(DLEVEL_BUFMGR) {
	dbgprintf("%s: strsize = %d\n", __func__, _inf.fdinfo[fd].strsize);
    }
}

/*
 * Checking if this file descriptor is controlled by this middleware.
 */
static inline int
dontcare_mode_check(int fd, int mode)
{
    if (_inf.fdinfo[fd].dntcare || _inf.fdinfo[fd].iofd == 0) {
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

    if (!_inf.fdinfo[fd].notfirst) {
	if (lseek) {
	    if (Myrank != 0) {
		strsize = len/Myrank;
	    }
	} else { /* read/write */
	    strsize = len;
	}
	if (strsize != 0) {
	    buf_init(fd, strsize);
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

    //sz = pwrite(fd, buf, size, pos);
    __real_lseek64(fd, pos, SEEK_SET);
    sz = __real_write(fd, buf, size);
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

    sz = __real_read(fd, buf, size);
    return sz;
}

static size_t
buf_flush(fdinfo *info, int cls)
{
    size_t	cc = -1ULL;
    int	i, j;
    int uoff, soff;
    int		strcnt = info->strcnt;
    size_t	strsize = info->strsize;
    size_t	blksize = info->filblklen;
    /*
     * ubuf : user data buffer, contining stripe * bufcount
     * sbuf : system data buffer
     * ubuf --> sbuf per stripe
     */
    /*
     *  from write: 
     *  from close: 
     */
    if (cls) { /* in order to flush remaining data */
	info->buflanes += 1;
    }
    // dbgprintf("%s: %s j(%d) info->buflanes(%d) info->bufcount(%d)\n",
    //__func__, cls == 0 ?"WRITE_FLUSH":"CLOSE_FLUSH", j, info->buflanes, info->bufcount);
    uoff = 0; soff = 0;
    for (j = 0; j < info->buflanes; j++) {
	int	thiscount = (j == info->buflanes - 1) ? info->bufcount: strcnt;
	for (i = 0; i < thiscount; i++) {
	    DEBUG(DLEVEL_BUFMGR) {
		data_show("ubuf", (int*) (info->ubuf + uoff), 5, uoff);
	    }
	    MPI_CALL(
		MPI_Gather(info->ubuf + uoff, strsize, MPI_BYTE,
			   info->sbuf + soff, strsize, MPI_BYTE,
			   i, MPI_COMM_WORLD));
	    uoff += strsize;
	}
	soff += blksize;
    }
    /*
     * bufcount: # of strips has been written to the buffer
     *   (nprocs == bufcount): buffers are fulled over all processes
     *   (nprocs < bufcount):  ranks, smaller than bufcount, only keep data
     * Variables with prefix "fil" are file view
     *   filpos:  pointer
     *   filcurb: current block number
     *		Each block is the maximum contiguous area in file view.
     *		writing/reading to/from file is performed per block.
     *		filcurb is advanced by stripe count, not +1.
     *   filtail: last block buffered in local
     *   filblklen: block length (stripe size * nprocs)
     *		stripe count is equal to nprocs
     *  E.g.
     *	         rank 0	  rank 1     rank 2     rank 3
     *  ubuf   #0#1#2#3   #0#1#2#3   #0#1#2#3   #0#1#2#3
     *         <------------- Exchange ---------------->
     *	sbuf     blk#0	  blk#1	      blk#2	blk#3
     */
    soff = 0;
    for (j = 0; j < info->buflanes; j++) {
	int	thiscount = (j == info->buflanes - 1) ? info->bufcount : strcnt;
	if (Myrank < thiscount) {
	    off_t	filpos = info->filcurb * blksize;
	    size_t	sz;

	    DEBUG(DLEVEL_BUFMGR) {
		dbgprintf("writing size(%ld) filpos(%ld) "
			  "curblk#(%d) tailblk#(%d)\n",
			  info->bufsize, filpos,  info->filcurb, info->filtail);
		/* showing one block */
		for (i = 0; i < info->bufsize; i += strsize) {
		    data_show("sbuf", (int*) (info->sbuf + soff + i), 5, i);
		}
	    }
	    sz = io_issue(WRK_CMD_WRITE, info->iofd, info->sbuf + soff, blksize, filpos);
	    if (info->frstrwcall || sz == blksize) {
		/* incase of the first read/write, return value might be zero */
		cc = strsize;
		info->frstrwcall = 0;
	    } else {
		dbgprintf("%s: cc(%ld) blksize(%ld)\n", __func__, cc, blksize);
		cc = -1ULL;
	    }
	    info->filcurb += strcnt;
	} else {
	    DEBUG(DLEVEL_BUFMGR) {
		dbgprintf("No needs to write\n", Myrank);
	    }
	    cc = strsize;
	}
	soff += blksize;
    }
    if (cls == 0) {
	/* this is called from write system call because the buffer
	   is full. so filcurb and filtail must be the same position */
	if(info->filcurb != info->filtail) {
	    dbgprintf("%s: Something Wrong ???? filcurb(%d) filtail(%d)\n",
		      __func__, info->filcurb, info->filtail);
	}
    }
    info->buflanes = 0;
    info->bufcount = 0;
    info->bufpos = 0;
    return cc;
}

/*
 * creat system call
 */
int
_iomiddle_creat(const char* path, mode_t mode)
{
    int	fd;

    if (is_dont_care_path(path)) {
	fd = __real_creat(path, mode);
	return fd;
    }
    DEBUG(DLEVEL_HIJACKED|DLEVEL_CONFIRM) {
	fprintf(stderr, "%s DO-CARE path=%s\n", __func__, path);
    }
    fd = __real_creat(path, mode);
    if (fd >= 0) {
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
    } else {
	int umode  = info_flagcheck(mode);
	int uflags = info_flagcheck(flags);
	fd = __real_open(path, uflags, umode);
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
	worker_init();
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

    /* file dscriptor 0 is never used */
    if (_inf.fdinfo[fd].dntcare || _inf.fdinfo[fd].iofd == 0) {
	rc = __real_close(fd);
	return rc;
    }
    info =  &_inf.fdinfo[fd];
    DEBUG(DLEVEL_HIJACKED) { fprintf(stderr, "%s DO-CARE fd(%d)\n", __func__, fd); }
    if (_inf.fdinfo[fd].bufcount > 0) {
	size_t	sz;
	DEBUG(DLEVEL_BUFMGR) {
	    dbgprintf("%s: bufcount(%d)\n", __func__, _inf.fdinfo[fd].bufcount);
	}
	if (!(info->buflanes == 0 && info->bufcount == 0)) {
	    sz = buf_flush(info, 1);
	    /* FIXME: how this error propagates ? */
	    rc = (sz == _inf.fdinfo[fd].strsize) ? 0 : -1;
	    if (rc == -1) {
		dbgprintf("%s: ERROR sz = %ld (frstrwcall=%d strsize(%ld))\n",
			  __func__, sz, _inf.fdinfo[fd].frstrwcall, _inf.fdinfo[fd].strsize);
	    }
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
	    rc = __real_close(fd);
	} else {
	    rc = __real_close(fd);
	    MPI_Reduce(&info->filpos, &filpos, 1, MPI_UNSIGNED_LONG_LONG,
		       MPI_MAX, 0, MPI_COMM_WORLD);
	}
    } else {
	rc = __real_close(fd);
    }
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

    IOMIDDLE_IFERROR((len != info->strsize),
		     "write length must be the stripe size. "
		     "len(%ld) stripe size(%d)\n", len, info->strsize);
    memcpy(info->ubuf + info->bufpos, buf, len);
    info->bufpos += len; info->bufcount++;
    info->filpos += len;
    DEBUG(DLEVEL_BUFMGR) {
	dbgprintf("bufcount(%d) mybufcount(%d) len(%ld) "
		  "info->strsize(%d)\n",
		  info->bufcount, _inf.mybufcount, len, info->strsize);
    }
    if (info->bufcount == _inf.mybufcount) {
	info->filtail += info->strcnt;
	info->buflanes++;
	if (info->buflanes == _inf.mybuflanes) {
	    dbgprintf("%s: info->buflanes(%d) info->bufcount(%d)\n", __func__, info->buflanes, info->bufcount);
	    rc = buf_flush(info, 0);
	    if (rc != len) {
		fprintf(stderr, "%s: ERROR here rc(%ld)\n", __func__, rc);
	    }
	} else {
	    info->bufcount = 0;
	}
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
    size_t	cc, rc = len;
    fdinfo *info;

    if (dontcare_mode_check(fd, MODE_READ)) {
	rc = __real_read(fd, buf, len);
	return rc;
    }
    DEBUG(DLEVEL_HIJACKED) {
	fprintf(stderr, "%s DO-CARE fd(%d) len(%ld)\n", __func__, fd, len);
    }
    if (stripe_check_init(fd, len, 0)) {
	DEBUG(DLEVEL_HIJACKED) { info_show(fd, __func__); }
	abort();
    }
    info = &_inf.fdinfo[fd];
    if (info->lanepos == 0) {
	cc = io_issue(WRK_CMD_READ, info->iofd, info->sbuf,
		      info->bufsize*_inf.mybuflanes, 0);
	if (info->frstrwcall) { /* the first-time read/write call */
	    cc = info->bufsize; /* return value is zero any cases */
	    info->frstrwcall = 0;
	}
	/* Though read opertaion returns error, other processes may success.
	 * Thus error is checked after Scatter */
	info->buflanes = _inf.mybuflanes;
    }
    if (info->lanepos < info->buflanes) {
	int	i, off = 0;

	for (i = 0; i < Nprocs; i++) {
	    MPI_CALL(
		MPI_Scatter(info->sbuf + info->lanepos*info->bufsize,
			    info->strsize, MPI_BYTE,
			    info->ubuf + off, info->strsize, MPI_BYTE,
			    i, MPI_COMM_WORLD));
	    off += info->strsize;
	}
	if ((int64_t) cc < 0) {
	    rc = cc;
	    goto ext;
	} else if (cc < _inf.fdinfo[fd].bufsize) {
	    /* truncated ? */
	    rc = cc/Nprocs;
	    goto ext;
	}
    }
    memcpy(buf, info->ubuf + info->bufpos, len);
    info->filpos += len;
    info->lanepos += 1;
ext:
    return rc;
}

static off64_t
lseek_general(int fd, off64_t reqfilpos)
{
    off64_t	rc;

    /* if this call is issued prior to read/write.
     * rqfilpos must be equal to stripe size. */
    if (stripe_check_init(fd, reqfilpos, 1)) {
	DEBUG(DLEVEL_HIJACKED) { info_show(fd, __func__); }
	abort();
    }
    /* file position is now reqfilpos */
    rc = _inf.fdinfo[fd].filpos = reqfilpos;
    /* lseek with 0 offset may be issued by rank 0.
     *  In this case, this request is ignored. */
    /* FIXME:
     * Must check if Rank 0 issues lseek offset = 0 after the first lseek issue */
    if (!(Myrank == 0 && reqfilpos == 0)) {
	int	strsize  = _inf.fdinfo[fd].strsize;
	int	strcnt   = _inf.fdinfo[fd].strcnt;
	int	strnum   = reqfilpos/strsize;
	int	expct_rank = strnum % strcnt;
	int	blks     = (strnum/(strcnt*Nprocs))*Nprocs + Myrank;

	DEBUG(DLEVEL_BUFMGR) {
	    dbgprintf("%s: strnum(%d) blks(%d) strsize(%d)\n",
		      __func__, strnum, blks, strsize);
	}
	/* checking if the file position is algined to this rank */
	IOMIDDLE_IFERROR((expct_rank != Myrank),
			 "lseek_general: offset is not expected in this rank. "
			 "response rank=%d offset=%lx\n",
			 expct_rank, reqfilpos);
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

    if (_inf.fdinfo[fd].dntcare || _inf.fdinfo[fd].iofd == 0) {
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

    if (_inf.fdinfo[fd].dntcare || _inf.fdinfo[fd].iofd == 0) {
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
    int	i;
    char	*cp;
    struct rlimit rlim;
    size_t	sz;

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

    /* worker is created and initialized */
    //worker_init();
    /* here are hijacked system call registration */
    _hijacked_creat = _iomiddle_creat;
    _hijacked_open = _iomiddle_open;
    _hijacked_close = _iomiddle_close;
    _hijacked_read = _iomiddle_read;
    _hijacked_lseek = _iomiddle_lseek;
    _hijacked_lseek64 = _iomiddle_lseek64;
    _hijacked_write = _iomiddle_write;
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
	    dbgprintf("%s: cmd(%s) cfd(%d) cbuf(%p) csize(%ld) cpos(%ld) rbuf(%p)\n",
		      __func__, (Wcmd == WRK_CMD_WRITE) ? "WRITE" : "READ",
		      Wcfd, Wcbuf, Wcsiz, Wcpos, Wcret, Wrbuf);
	}
	if (Wcmd == WRK_CMD_WRITE) {
	    sz = io_write(Wcfd, Wcbuf, Wcsiz, Wcpos);
	    worker_sigdone(sz); /* signal to the client, go ahead */
	} else if (Wcmd == WRK_CMD_READ) {
	    if (!Wnfst) { /* this is the first time to read */
		Wnfst = 1;
		/* read buffer */
		Wrbuf = malloc(sz);
		if(Wrbuf == NULL) {
		    dbgprintf("%s: Cannot allocate buffer\n", __func__);
		    exit(-1);
		}
		sz = io_read(Wcfd, Wcbuf, Wcsiz, Wcpos);
		/* prefetching and its return values is stored in Wrret */
		Wrret = io_read(Wcfd, Wrbuf, Wcsiz, Wcpos);
		worker_sigdone(sz); /* signal to the client, go ahead */
	    } else {
		sz = Wcret;
		if (sz > 0) {
		    memcpy(Wcbuf, Wrbuf, Wcsiz);
		}
		worker_sigdone(sz); /* signal to the client, go ahead */
		/* worker is still working here */
		Wrret = io_read(Wcfd, Wrbuf, Wcsiz, Wcpos);
	    }
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
io_issue(size_t (*cmd)(int, void*, size_t, off64_t),
	 int fd, void *buf, size_t size, off64_t pos)
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
	if (cmd == io_read || cmd == io_write) {
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
	//fprintf(stderr, "%s: Initialize for fd=%d worker(%d)\n", __func__, fd, is_worker_enable);
	dbgprintf("%s: Initialize for fd=%d worker(%d)\n", __func__, fd, is_worker_enable);
	Wcmd = WRK_CMD_DONE;
	Wcfd = fd;
	Wcbuf = 0; Wcsiz = 0; Wcpos = 0; Wrbuf = 0;
	Wenable = is_worker_enable;
    }
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
    dbgprintf("%s: worker is idle\n", __func__);
    Wcfd = 0;
    if (Wrbuf) {
	free(Wrbuf);
    }
    Wrbuf = 0;
}

void
worker_init()
{
    int	cc;

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
