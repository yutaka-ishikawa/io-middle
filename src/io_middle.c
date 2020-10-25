/*
 * Assumptions:
 *	1) A file descriptor is always used for read or write operations,
 *	   not combination of read and write.
 *	2) lseek is always issued for the next stripe.
 * Captured system calls:
 *	creat, open, close, read, lseek64, write
 */
#include "io_middle.h"
#include <mpi.h>
#include <limits.h>

#define Myrank 	(_inf.rank)
#define Nprocs 	(_inf.nprocs)

static struct ioinfo _inf;
static char	care_path[PATH_MAX];

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

static void
buf_init(int fd, int strsize)
{
    int	strcnt = Nprocs;
    _inf.fdinfo[fd].notfirst = 1;
    _inf.fdinfo[fd].strsize = strsize;
    _inf.fdinfo[fd].strcnt = strcnt;
    _inf.fdinfo[fd].filoff = _inf.fdinfo[fd].strsize*Myrank;
    _inf.fdinfo[fd].filblklen = strsize * strcnt;
    _inf.mybufcount = strcnt;
    _inf.fdinfo[fd].bufsize = _inf.fdinfo[fd].filblklen;
    _inf.fdinfo[fd].ubuf = malloc(_inf.fdinfo[fd].bufsize);
    _inf.fdinfo[fd].sbuf = malloc(_inf.fdinfo[fd].bufsize);
    IOMIDDLE_IFERROR(
	(_inf.fdinfo[fd].ubuf == NULL || _inf.fdinfo[fd].sbuf == NULL),
	"%s", "Cannot allocate IO middleware buffer\n");
    memset(_inf.fdinfo[fd].ubuf, 0, _inf.fdinfo[fd].bufsize);
    memset(_inf.fdinfo[fd].sbuf, 0, _inf.fdinfo[fd].bufsize);
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
    if (_inf.fdinfo[fd].mode == MODE_UNKNOWN) {
	_inf.fdinfo[fd].mode = mode;
    }
    IOMIDDLE_IFERROR((_inf.fdinfo[fd].mode != mode), "%s",
		     "read and write issued\n");
    return 0;
}

/*
 *  Stripe size is checked or determined
 */
static inline int
stripe_check_init(int fd, size_t len, int lseek)
{
    int	rc = 0;
    int	strsize;
    if (!_inf.fdinfo[fd].notfirst) {
	if (lseek) {
	    if (Myrank == 0) {
		IOMIDDLE_IFERROR((len != 0), "%s",
			 "lseek is issued before write/read on rank 0\n");
		/* OK */
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
			  "stripe size(%ld), requested size(%d)\n",
			  len, _inf.fdinfo[fd].strsize);
		rc = -1;
	    }
	}
    }
ext:
    return rc;
}

size_t
buf_flush(fdinfo *info)
{
    size_t	cc = -1ULL;
    int	i;
    size_t	strsize = info->strsize;
    /*
     * ubuf : user data buffer, contining stripe * bufcount
     * sbuf : system data buffer
     * ubuf --> sbuf per stripe
     */
    for (i = 0; i < info->bufcount; i++) {
	int off = 0;
	DEBUG(DLEVEL_BUFMGR) {
	    data_show("ubuf", (int*) info->ubuf + off, 5, off);
	}
	MPI_CALL(
	    MPI_Gather(info->ubuf + off, strsize, MPI_BYTE,
		       info->sbuf, strsize, MPI_BYTE,
		       i, MPI_COMM_WORLD));
	    off += strsize;
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
    if (Myrank < info->bufcount) {
	off_t	filpos = info->filcurb * info->filblklen;
	size_t	wsize = strsize * info->bufcount;

	DEBUG(DLEVEL_BUFMGR) {
	    dbgprintf("writing size(%ld) filpos(%ld) "
		      "curblk#(%d) tailblk#(%d)\n",
		      info->bufsize, filpos,  info->filcurb, info->filtail);
	    /* showing one block */
	    for (i = 0; i < info->bufsize; i += strsize) {
		data_show("sbuf", (int*) (info->sbuf + i), 5, i);
	    }
	}
	cc = pwrite(info->iofd, info->sbuf, wsize, filpos);
	if (cc < wsize) { cc = -1ULL; goto ext; }
    } else {
	DEBUG(DLEVEL_BUFMGR) {
	    dbgprintf("No needs to write\n", Myrank);
	}
    }
ext:
    info->filcurb += info->strcnt;
    info->filtail += info->strcnt;
    if(info->filcurb != info->filtail) {
	dbgprintf("%s: Something Wrong ???? filcurb(%d) filtail(%d)\n",
		  __func__, info->filcurb, info->filtail);
    }
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
	fd =  __real_creat(path, mode);
	return fd;
    }
    DEBUG(DLEVEL_HIJACKED|DLEVEL_CONFIRM) {
	fprintf(stderr, "%s DO-CARE path=%s\n", __func__, path);
    }
    fd =  __real_creat(path, mode);
    if (fd >= 0) {
	_inf.fdinfo[fd].iofd = fd;
	_inf.fdinfo[fd].bufpos = 0;
	_inf.fdinfo[fd].filpos = 0;
	_inf.fdinfo[fd].bufcount = 0;
	_inf.fdinfo[fd].iofd = fd;
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

    DEBUG(DLEVEL_ALL) {
	fprintf(stderr, "[%d] %s path=%s\n", Myrank, __func__, path);
    }
    if (flags & O_CREAT) {
	int mode;
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, int);
        va_end(arg);
	fd =  __real_open(path, flags, mode);
    } else {
	fd = __real_open(path, flags);
    }
    if (fd < 0) goto err;
    if (is_dont_care_path(path)) {
	_inf.fdinfo[fd].notfirst = 1;
	_inf.fdinfo[fd].dntcare = 1;
    } else {
	DEBUG(DLEVEL_HIJACKED|DLEVEL_CONFIRM) {
	    fprintf(stderr, "[%d] DO-CARE file fd(%d) path(%s)\n",
		    Myrank, fd, path);
	}
	if (Myrank < 0) {
	    MPI_Comm_size(MPI_COMM_WORLD, &Nprocs);
	    MPI_Comm_rank(MPI_COMM_WORLD, &Myrank);
	}
	_inf.fdinfo[fd].iofd = fd;
	_inf.fdinfo[fd].bufpos = 0;
	_inf.fdinfo[fd].filpos = 0;
	_inf.fdinfo[fd].bufcount = 0;
	_inf.fdinfo[fd].iofd = fd;
	_inf.fdinfo[fd].dntcare = 0;
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
    int	rc;

    if (_inf.fdinfo[fd].dntcare || _inf.fdinfo[fd].iofd == 0) {
	rc = __real_close(fd);
	return rc;
    }
    DEBUG(DLEVEL_HIJACKED) { fprintf(stderr, "%s DO-CARE fd(%d)\n", __func__, fd); }
    if (_inf.fdinfo[fd].bufcount > 0) {
	DEBUG(DLEVEL_BUFMGR) {
	    dbgprintf("%s: bufcount(%d)\n", __func__, _inf.fdinfo[fd].bufcount);
	}
	rc = buf_flush(&_inf.fdinfo[fd]);
    }
    rc = __real_close(fd);
    _inf.fdinfo[fd].attrall = 0;
    _inf.fdinfo[fd].iofd = -1;
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
    info->filpos += len; /* filpos will be discarded ? */
    DEBUG(DLEVEL_BUFMGR) {
	dbgprintf("bufcount(%d) mybufcount(%d) len(%ld) "
		  "info->strsize(%d)\n",
		  info->bufcount, _inf.mybufcount, len, info->strsize);
    }
    if (info->bufcount == _inf.mybufcount) {
	rc = buf_flush(info);
	if (rc == -1) goto ext;
    }
 ext:
    if (rc == info->filblklen) {
	rc = len;
    }
    return rc;
}

/*
 * read system call
 */
ssize_t
_iomiddle_read(int fd, void *buf, size_t len)
{
    size_t	rc = len;
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
    if (info->bufpos == 0) {
	int	i, off = 0;
	size_t	cc;

	cc = __real_read(info->iofd, info->sbuf, info->bufsize);
	/* Though read opertaion returns error, other processes may success.
	 * Thus error is checked after Scatter */
	for (i = 0; i < Nprocs; i++) {
	    MPI_CALL(
		MPI_Scatter(info->sbuf, info->strsize, MPI_BYTE,
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
    info->bufpos += len; info->bufcount++;
    info->filpos += len;
    if (info->bufcount == _inf.mybufcount) {
	info->bufcount = 0;
	info->bufpos = 0;
    }
ext:
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
			 "lseek64: offset is not expected in this rank. "
			 "response rank=%d offset=%lx\n",
			 expct_rank, reqfilpos);
	/* checking if this file position is the next */
	IOMIDDLE_IFERROR((blks != _inf.fdinfo[fd].filtail),
			 "[%d] lseek64: offset is out of block area: %ld "
			 " (blks(%d) tailblks(%d))\n",
			 Myrank, reqfilpos, blks, _inf.fdinfo[fd].filtail);
    }
    return rc;
}

#include <sys/time.h>
#include <sys/resource.h>

/*
 * _myhijack_init:
 *  This function is invoked when one of the hijacked system call
 *  is invoked by the user.
 *	See lib/hooklib.c
 */
void
_myhijack_init()
{
    int	i;
    char	*cp;
    struct rlimit rlim;

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
    DEBUG(DLEVEL_CONFIRM) {
	printf("IOMIDDLE_CARE_PATH = %s\n", care_path);
    }
    DEBUG(DLEVEL_ALL) {
	printf("%s: init is called. debug(%d)\n", __func__, _inf.debug);
    }
    cp = getenv("IO_MIDDLE_BUFCOUNT");
    if (cp) {
	_inf.mybufcount = atoi(cp);
    } else {
	_inf.mybufcount = 1;
    }
    getrlimit(RLIMIT_NOFILE, &rlim);
    Myrank = -1;
    _inf.fdlimit = rlim.rlim_cur;
    _inf.fdinfo = malloc(sizeof(fdinfo)*_inf.fdlimit);
    IOMIDDLE_IFERROR((_inf.fdinfo == 0), "%s",
		     "Canoot allocate working memory\n");
    /* Checking don't care fd's */
    for (i = 0; i < 3; i++) {
	_inf.fdinfo[i].notfirst = 1;
	_inf.fdinfo[i].dntcare = 1;
    }
    /* here are hijacked system call regisration */
    _hijacked_creat = _iomiddle_creat;
    _hijacked_open = _iomiddle_open;
    _hijacked_close = _iomiddle_close;
    _hijacked_read = _iomiddle_read;
    _hijacked_lseek64 = _iomiddle_lseek64;
    _hijacked_write = _iomiddle_write;
}

