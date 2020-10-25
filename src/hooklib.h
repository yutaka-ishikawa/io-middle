/*
 *	written by Yutaka Ishikawa, yutaka.ishikawa@riken.jp
 *	Copyright 2018, RIKEN
 *	  2018/04/30
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

#define EXTERN_PTR_DECL(name,ret,args)		\
    extern ret (*__real_ ## name) args;	\
    extern ret (*_hijacked_ ## name) args

EXTERN_PTR_DECL(creat, int, (const char* path, mode_t mode));
EXTERN_PTR_DECL(creat64, int, (const char* path, mode_t mode));
EXTERN_PTR_DECL(open, int, (const char *path, int flags, ...));
EXTERN_PTR_DECL(open64, int, (const char *path, int flags, ...));
EXTERN_PTR_DECL(close, int, (int fd));
EXTERN_PTR_DECL(write, ssize_t, (int fd, const void *buf, size_t count));
EXTERN_PTR_DECL(read, ssize_t, (int fd, void *buf, size_t count));
EXTERN_PTR_DECL(lseek, off_t, (int fd, off_t offset, int whence));
EXTERN_PTR_DECL(lseek64, off64_t, (int fd, off64_t offset, int whence));
EXTERN_PTR_DECL(pread, ssize_t, (int fd, void *buf, size_t count, off_t offset));
EXTERN_PTR_DECL(pread64, ssize_t, (int fd, void *buf, size_t count, off64_t offset));
EXTERN_PTR_DECL(pwrite, ssize_t, (int fd, const void *buf, size_t count, off_t offset));
EXTERN_PTR_DECL(pwrite64, ssize_t, (int fd, const void *buf, size_t count, off64_t offset
));
EXTERN_PTR_DECL(readv, ssize_t, (int fd, const struct iovec *iov, int iovcnt));
EXTERN_PTR_DECL(writev, ssize_t, (int fd, const struct iovec *iov, int iovcnt));
EXTERN_PTR_DECL(__fxstat, int, (int vers, int fd, struct stat *buf));
EXTERN_PTR_DECL(__fxstat64, int, (int vers, int fd, struct stat64 *buf));
EXTERN_PTR_DECL(__lxstat, int, (int vers, const char* path, struct stat *buf));
EXTERN_PTR_DECL(__lxstat64, int, (int vers, const char* path, struct stat64 *buf));
EXTERN_PTR_DECL(__xstat, int, (int vers, const char* path, struct stat *buf));
EXTERN_PTR_DECL(__xstat64, int, (int vers, const char* path, struct stat64 *buf));
EXTERN_PTR_DECL(mmap, void*, (void *addr, size_t length, int prot, int flags, int fd, off_t offset));
EXTERN_PTR_DECL(mmap64, void*, (void *addr, size_t length, int prot, int flags, int fd, off64_t offset));
EXTERN_PTR_DECL(fopen, FILE*, (const char *path, const char *mode));
EXTERN_PTR_DECL(fopen64, FILE*, (const char *path, const char *mode));
EXTERN_PTR_DECL(fclose, int, (FILE *fp));
EXTERN_PTR_DECL(fread, size_t, (void *ptr, size_t size, size_t nmemb, FILE *stream));
EXTERN_PTR_DECL(fwrite, size_t, (const void *ptr, size_t size, size_t nmemb, FILE *stream));
EXTERN_PTR_DECL(fseek, int, (FILE *stream, long offset, int whence));
EXTERN_PTR_DECL(fsync, int, (int fd));
EXTERN_PTR_DECL(fdatasync, int, (int fd));
EXTERN_PTR_DECL(aio_read, int, (struct aiocb *aiocbp));
EXTERN_PTR_DECL(aio_read64, int, (struct aiocb64 *aiocbp));
EXTERN_PTR_DECL(aio_write, int, (struct aiocb *aiocbp));
EXTERN_PTR_DECL(aio_write64, int, (struct aiocb64 *aiocbp));
