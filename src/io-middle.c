#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#define __USE_GNU
#include <dlfcn.h>
#include <mpi.h>
#include "hooklib.h"
#include "io_middle.h"

#define MODE_UNKNOWN	0
#define MODE_READ	1
#define MODE_WRITE	2

struct ioinfo {
    int		debug;
    int		nprocs;
    int		myrank;
    size_t	mybufcount;
    struct {
	int	first:1,
		mode:2,
		strsize:61;
	int	iofd;
	size_t	bufsize;
	int	bufcount;
	off64_t	offset;
	char	*ubuf;
	char	*sbuf;
    } fd[1024];		/* FIXE ME 1024 */
};

struct ioinfo _ioinfo;

#define DEBUG	if (_ioinfo.debug)

static void
init_buf(int fd)
{
    _ioinfo[fd].bufsize
	= _ioinfo.strsize * _ioinfo.nprocs * _ioinfo.mybufcount;
    _ioinfo[fd].ubuf = malloc(_ioinfo.bufsize);
    _ioinfo[fd].sbuf = malloc(_ioinfo.bufsize);
    if (_ioinfo[fd].ubuf == NULL || _ioinfo[fd].sbuf == NULL) {
	fprintf(stderr, "Cannot allocate IO middleware buffer\n");
	exit(-1);
    }
    memset(_ioinfo[fd].ubuf, 0, _ioinfo[fd].bufsize);
    memset(_ioinfo[fd].sbuf, 0, _ioinfo[fd].bufsize);
}

static inline void
mode_check(int fd, int mode)
{
    if (_ioinfo[fd].mode == UNKNOWN) {
	_ioinfo[fd].mode = mode;
    } else if (_ioinfo.fd[mode] != mode) {
	fprintf(stderr, "");
    }
}

static inline void
stripe_check(int fd, size_t count)
{
    if (!_ioinfo.fd[fd].first) {
	_ioinfo.strsize = count;
	_ioinfo.fd[fd].first = 1;
	init_buf(fd);
    } else {
	if (_ioinfo.fd[fd].strsize != count) {
	    fprintf(stderr, "stripe size is changed: new(ld) old(%ld)\n",
		    count, _ioinfo.strsize); fflush(stderr);
	    abort();
	}
    }
}

int
_iomiddle_open(const char *path, int flags, ...)
{
    int	fd;

    DEBUG {
	fprintf(stderr, "%s\n", __func__);
    }
    if (flags & O_CREAT) {
	int mode;
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, int);
        va_end(arg);
	fd = _ioinfo.fd[fd].iofd = __real_open(path, flags, mode);
    } else {
	fd = _ioinfo.fd[fd].iofd = __real_open(path, flags);
    }
    _ioinfo.fd[fd].offset = 0;
    _ioinfo.fd[fd].bufcount = 0;
    _ioinfo.fd[fd].iofd = fd;
    return fd;
}

int
_iomiddle_close(int fd)
{
    int	rc;
    DEBUG { fprintf(stderr, "%s\n", __func__); }
    if (_ioinfo.fd[fd] != fd) {
	fprintf(stderr, "close: something wrong fd=%d\n", fd);
	abort();
    }
    rc = __real_close(fd);
    _ioinfo.first = 0;
    return rc;
}

ssize_t
_iomiddle_write(int fd, const void *buf, size_t len)
{
    ssize_t	rc;
    DEBUG {
	int	sz;
	char	dbuf[128];
	sz = snprintf(dbuf, 128, "%s\n", __func__);
	__real_write(0, dbuf, sz);
    }
    mode_check(fd, MODE_WRITE);
    stripe_check(fd, len);
    memcpy(_ioinfo.ubuf + _ioinfo.offset, buf, len);
    MPI_Allather(_ioinfo.fd[fd].ubuf, _ioinfo.fd[fd].strsize, MPI_BYTE,
		 _ioinfo.fd[fd].sbuf, MPI_BYTE, MPI_COMM_WORLD);
    _ioinfo.fd[fd].offset += len; _ioinfo.fd[fd].bufcount++;
    if (_ioinfo.fd[fd].bufcount == _ioinfo.mybufcount) {
	rc = __real_write(_ioinfo.fd[fd].iofd,
			  _ioinfo.fd[fd].ubuf, _ioinfo.fd[fd].bufsize);
	_ioinfo.fd[fd].bufcount = 0;
	_ioinfo.fd[fd].offset = 0;
    }
    rc = len;
    return rc;
}

ssize_t
_iomiddle_read(int fd, void *buf, size_t len)
{
    int	rc;
    DEBUG { fprintf(stderr, "%s\n", __func__); }
    mode_check(fd, MODE_READ);
    stripe_check(fd, len);
    if (_ioinfo.fd[fd].offset == 0) {
	__real_read(_ioinfo.fd[fd].iofd,
		    _ioinfo.fd[fd].sbuf, _ioinfo.fd[fd].bufsize);
	MPI_Scatter(_ioinfo.fd[fd].sbuf, _iioinfo.fd[fd].strsize, MPI_BYRTE,
		    _ioinfo.fd[fd].ubuf, MPI_BYTE, MPI_COMM_WORLD);
    }
    memcpy(buf, _ioinfo.fd[fd].ubuf + _ioinfo.fd[fd].offset, len);
    _ioinfo.fd[fd].offset += len; _ioinfo.fd[fd].bufcount++;
    if (_ioinfo.fd[fd].bufount == _ioinfo.mybufcount) {
	_ioinfo.fd[fd].bufcount = 0;
	_ioinfo.fd[fd].offset = 0;
    }
    
    return rc;
}

off64_t
_iomiddle_lseek64(int fd, off64_t offset, int whence)
{
    int	rc = 0;
    off64_t	strsize;
    DEBUG { fprintf(stderr, "%s\n", __func__); }
    strsize = offset - _ioinfo.offset;
    if (strsize != _ioinfo.offset) {
	fprintf(stderr, "lseek64: stripe size is changed: new(ld) old(%ld)\n",
		count, _ioinfo.strsize); fflush(stderr);
	abort();
    }
    if (offset != _ioinfo.offset) {
	fprintf(stderr, "lseek64: offset is not assumed one: offset(ld) assumed(%ld)\n",
		offset, _ioinfo.offset); fflush(stderr);
	abort();
    }
    return rc;
}

void
_myhijack_init()
{
    char	*cp;
    cp = getenv("IOMIDDLE_DEBUG");
    if (cp && atoi(cp) == 1) {
	_ioinfo.debug = 1;
    }
    cp = getenv("IO_MIDDLE_BUFCOUNT");
    if (cp) {
	_ioinfo.bufcount = atoi(cp);
    } else {
	_ioinfo.bufcount = 1;
    }
    MPI_Comm_size(MPI_COMM_WORLD, &_ioinfo.nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &_ioinfo.myrank);
    DEBUG {
	fprintf(stderr, "IO_MIDDLE_BUFCOUNT = %ld\n", _ioinfo.bufcount);
	fprintf(stderr, "nprocs = %d\n", _ioinfo.nprocs);
	fprintf(stderr, "myrank = %d\n", _ioinfo.myrank);
    }
    
    _hijacked_open = _iomiddle_open;
    _hijacked_close = _iomiddle_close;
    _hijacked_write = _iomiddle_write;
    _hijacked_read = _iomiddle_read;
    _hijacked_lseek64 = _iomiddle_lseek64;
}
