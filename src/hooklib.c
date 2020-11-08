/*
 *	2020/10/23 for IO500 Benchmark
 *	2018/08/30 fget* / fput* are added
 *	2018/04/30 Created
 *	written by Yutaka Ishikawa, yutaka.ishikawa@riken.jp
 */
#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <aio.h>
#define __USE_GNU
#include <dlfcn.h>

#define PTR_DECL(name, ret, args)	\
    ret (*__real_ ## name) args = NULL;	\
    ret (*_hijacked_ ## name) args = NULL;
    
#define HIJACK(type, func)				\
    {							\
	if (_hijack_init == 0) {			\
	    _hijack_init = 1;				\
	    _myhijack_init();				\
	}						\
	if ((void*) __real_ ## func == (void*) NULL) {	\
	    __real_ ## func = type dlsym(RTLD_NEXT, #func);	\
	}						\
    }

#define HIJACK_DO(type, ret, func, args)		\
    {							\
	HIJACK(type, func);				\
	if (_hijacked_ ## func) {			\
	    ret = _hijacked_ ## func args;		\
	} else {					\
	    ret = __real_ ## func args;			\
	}						\
    }

static int _hijack_init = 0;

extern void _myhijack_init();

PTR_DECL(creat, int, (const char* path, mode_t mode));
PTR_DECL(open, int, (const char *path, int flags, ...));
PTR_DECL(close, int, (int fd));
PTR_DECL(write, ssize_t, (int fd, const void *buf, size_t count));
PTR_DECL(read, ssize_t, (int fd, void *buf, size_t count));
PTR_DECL(lseek, off_t, (int fd, off_t offset, int whence));
PTR_DECL(lseek64, off64_t, (int fd, off64_t offset, int whence));

#if 0
PTR_DECL(creat64, int, (const char* path, mode_t mode));
PTR_DECL(open64, int, (const char *path, int flags, ...));
PTR_DECL(pread, ssize_t, (int fd, void *buf, size_t count, off_t offset));
PTR_DECL(pread64, ssize_t, (int fd, void *buf, size_t count, off64_t offset));
PTR_DECL(pwrite, ssize_t, (int fd, const void *buf, size_t count, off_t offset));
PTR_DECL(pwrite64, ssize_t, (int fd, const void *buf, size_t count, off64_t offset
));
PTR_DECL(readv, ssize_t, (int fd, const struct iovec *iov, int iovcnt));
PTR_DECL(writev, ssize_t, (int fd, const struct iovec *iov, int iovcnt));
PTR_DECL(__fxstat, int, (int vers, int fd, struct stat *buf));
PTR_DECL(__fxstat64, int, (int vers, int fd, struct stat64 *buf));
PTR_DECL(__lxstat, int, (int vers, const char* path, struct stat *buf));
PTR_DECL(__lxstat64, int, (int vers, const char* path, struct stat64 *buf));
PTR_DECL(__xstat, int, (int vers, const char* path, struct stat *buf));
PTR_DECL(__xstat64, int, (int vers, const char* path, struct stat64 *buf));
PTR_DECL(mmap, void*, (void *addr, size_t length, int prot, int flags, int fd, off_t offset));
PTR_DECL(mmap64, void*, (void *addr, size_t length, int prot, int flags, int fd, off64_t offset));
PTR_DECL(fopen, FILE*, (const char *path, const char *mode));
PTR_DECL(fopen64, FILE*, (const char *path, const char *mode));
PTR_DECL(fclose, int, (FILE *fp));
PTR_DECL(fread, size_t, (void *ptr, size_t size, size_t nmemb, FILE *stream));
PTR_DECL(fwrite, size_t, (const void *ptr, size_t size, size_t nmemb, FILE *stream));
PTR_DECL(fgetc, int, (FILE *stream));
PTR_DECL(fgets, char*, (char *s, int size, FILE *stream));
PTR_DECL(getc, int, (FILE *stream));
PTR_DECL(getchar, int, (void));
PTR_DECL(ungetc, int, (int c, FILE *stream));
PTR_DECL(fputc, int, (int c, FILE *stream));
PTR_DECL(fputs, int, (const char *s, FILE *stream));
PTR_DECL(putc, int, (int c, FILE *stream));
PTR_DECL(puchar, int, (int c));
PTR_DECL(puts, int, (const char *s));
PTR_DECL(fseek, int, (FILE *stream, long offset, int whence));
PTR_DECL(fsync, int, (int fd));
PTR_DECL(fdatasync, int, (int fd));
PTR_DECL(aio_read, int, (struct aiocb *aiocbp));
PTR_DECL(aio_read64, int, (struct aiocb64 *aiocbp));
PTR_DECL(aio_write, int, (struct aiocb *aiocbp));
PTR_DECL(aio_write64, int, (struct aiocb64 *aiocbp));
#endif /**/

int
creat(const char* path, mode_t mode)
{
    int	ret;
    HIJACK_DO((int (*)(const char*, mode_t)), ret, creat, (path, mode));
    return ret;
}

int
open(const char *path, int flags, ...)
{
    int		ret;
    mode_t	mode;

    HIJACK((int (*)(const char *, int, ...)), open);
    /* go through if no hijacked */
    if (flags & O_CREAT) {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, int);
        va_end(arg);
	if (_hijacked_open) {
	    ret = _hijacked_open(path, flags, mode);
	} else {
	    ret = __real_open(path, flags, mode);
	}
    } else {
	if (_hijacked_open) {
	    ret = _hijacked_open(path, flags);
	} else {
	    ret = __real_open(path, flags);
	}
    }
    return ret;
}

int
close(int fd)
{
    int	ret;

    // fprintf(stderr, "CLOSE %d 0x%p %ld\n", fd);
    HIJACK_DO((int (*)(int)), ret, close, (fd));
    return ret;
}

ssize_t write(int fd, const void *buf, size_t count)
{
    ssize_t	ret;

    // fprintf(stderr, "WRITE %d 0x%p %ld\n", fd, buf, count);
    HIJACK_DO((ssize_t (*)(int, const void*, size_t)), ret, write, (fd, buf, count));
    return ret;
}

ssize_t read(int fd, void *buf, size_t count)
{
    ssize_t	ret;

    // fprintf(stderr, "READ %d 0x%p %ld\n", fd, buf, count);
    HIJACK_DO((ssize_t (*)(int, void*, size_t)), ret, read, (fd, buf, count));
    return ret;
}

off_t
lseek(int fd, off_t offset, int whence)
{
    ssize_t	ret;
    HIJACK_DO((ssize_t (*)(int, off_t, int)), ret, lseek, (fd, offset, whence));
    return ret;
}

off64_t
lseek64(int fd, off64_t offset, int whence)
{
    ssize_t	ret;
    HIJACK_DO((off64_t (*)(int, off64_t, int)), ret, lseek64, (fd, offset, whence));
    return ret;
}
