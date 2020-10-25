#include "testlib.h"
#include <mpi.h>

off64_t	strsize = DEFAULT_STRSIZE;
int	strcnt;
size_t	len = DEFAULT_LENGTH;
size_t	bufsiz;
int	nprocs, myrank, dflag, vflag, rwflag, tflag;
int	verbose;

char	fname[1024];

void
test_parse_args(int argc, char **argv)
{
    int	opt;
    rwflag = DO_WRITE;
    while ((opt = getopt(argc, argv, "drtvwVWc:f:l:s:")) != -1) {
	switch(opt) {
	case 'd': /* debug mode */
	    setenv("IOMIDDLE_DEBUG", "1", 1);
	    dflag = 1;
	    break;
	case 'V': /* verbose mode */
	    verbose = 1;
	    break;
	case 'W': /* write and read, default write */
	    rwflag = DO_WRITE|DO_READ;
	    break;
	case 'w': /* write, default write */
	    rwflag = DO_WRITE;
	    break;
	case 'r': /* read, default write */
	    rwflag = DO_READ;
	    break;
	case 't': /* truncate or not */
	    tflag = 1;
	    break;
	case 'v': /* verify or not */
	    vflag = 1;
	    break;
	case 'c': /* stripe count == nprocs */
	    strcnt = atoi(optarg);
	    break;
	case 'f': /* file name */
	    strcpy(fname, optarg);
	    break;
	case 'l': /* count of write/read */
	    len = atoll(optarg);
	    break;
	case 's': /* stripe size */
	    strsize = atoi(optarg);
	    break;
	}
    }
}
