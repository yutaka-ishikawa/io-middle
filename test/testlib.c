#include "testlib.h"
#include <mpi.h>

off64_t	strsize = DEFAULT_STRSIZE;
int	strcnt;
size_t	len = DEFAULT_LENGTH;
size_t	bufsiz;
int	nprocs, myrank, dflag, rwflag, vflag;
int	verbose;

char	fname[1024];

void
test_parse_args(int argc, char **argv)
{
    int	opt;
    rwflag = DO_WRITE;
    while ((opt = getopt(argc, argv, "rvdVWf:s:c:l:")) != -1) {
	switch(opt) {
	case 'V':
	    verbose = 1;
	    break;
	case 'W':
	    rwflag = DO_WRITE|DO_READ;
	    break;
	case 'r':
	    rwflag = DO_READ;
	    break;
	case 'v':
	    vflag = 1;
	    break;
	case 'f': /* file name */
	    strcpy(fname, optarg);
	    break;
	case 's': /* stripe size */
	    strsize = atoi(optarg);
	    break;
	case 'c': /* stripe count == nprocs */
	    strcnt = atoi(optarg);
	    break;
	case 'd':
	    setenv("IOMIDDLE_DEBUG", "1", 1);
	    dflag = 1;
	    break;
	case 'l':
	    len = atoll(optarg);
	    break;
	}
    }
}
