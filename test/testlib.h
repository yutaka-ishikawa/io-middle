#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define DEFAULT_STRSIZE	47008
#define DEFAULT_LENGTH	1000
#define DO_READ		1
#define DO_WRITE	2
#define VERBOSE	if (verbose)

extern off64_t	strsize;
extern int	strcnt;
extern size_t	len;
extern size_t	bufsiz;
extern int	nprocs, myrank, dflag, rwflag, vflag;
extern int	rwflag;
extern int	verbose;
extern char	fname[1024];

extern void test_parse_args(int, char **argc);
