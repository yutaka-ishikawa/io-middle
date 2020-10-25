#include "testlib.h"
#include "utf_tsc.h"
#include <mpi.h>
#include <limits.h>

extern void redirect();

static int	errors = 0;
static void	*bufp;

static uint64_t	timer_hz;
static uint64_t	timer_st[2], timer_et[2];
#define TIMER_SECOND(t)	((double)(t)/(double)(timer_hz))

static void
timer_init()
{
    timer_hz = tick_helz( 0 );
    if (timer_hz == 0) {
	printf("Cannot obtain CPU frequency\n");
	exit(-1);
    }
}

static int
write_stripe(int fd, void *buf, size_t size, off64_t pos)
{
    size_t	rc;
    rc = lseek64(fd, pos, SEEK_SET);
    if (rc != pos) {
	printf("Lseek error: rc(%ld) pos(%ld)\n", rc, pos);
	errors++;
	return -1;
    }
    rc = write(fd, buf, size);
    return rc;
}

static int
read_stripe(int fd, void *buf, size_t size, off64_t pos)
{
    size_t	rc;
    rc = lseek64(fd, pos, SEEK_SET);
    if (rc != pos) {
	printf("Lseek error: rc(%ld) pos(%ld)\n", rc, pos);
	errors++;
	return -1;
    }
    rc = read(fd, buf, size);
    return rc;
}

static int
verify(void *bufp, size_t busiz, int val)
{
    off64_t	pos;
    int	errs = 0;
    for (pos = 0; pos < bufsiz/sizeof(unsigned int); pos++) {
	if (((unsigned *)bufp)[pos] != pos + myrank + val) {
	    printf("\t ERROR pos(%ld) value(%d) expect(%ld)\n",
		   pos, ((unsigned *)bufp)[pos], pos + myrank + val);
	    if (errs++ > 4) break;
	}
    }
    return errs;
}

static void
fillin(void *bufp, size_t busiz, int val)
{
    off64_t	pos;
    for (pos = 0; pos < bufsiz/sizeof(unsigned int); pos++) {
	((unsigned *)bufp)[pos] = pos + myrank + val;
    }
}

static void
do_write(char *fnm, off64_t offset, void *bufp, size_t bufsiz)
{
    int		fd, iter;
    size_t	sz;
    off64_t	pos;
    int		flags;

    flags = O_CREAT|O_WRONLY;
#if 0
    flags |= O_TRUNC;
#endif
    if ((fd = open(fnm, flags, 0644)) < 0) {
	fprintf(stderr, "Cannot open file %s\n", fnm);
	exit(-1);
    }
    fillin(bufp, bufsiz, 0);
    pos = offset;
    for (iter = 0; iter < len; iter++) {
	VERBOSE {
	    printf("[%d] iter=%d pos=%ld strsize(%ld)\n",
		   myrank, iter, pos, strsize);
	}
	sz = write_stripe(fd, bufp, strsize, pos);
	if (sz != strsize) {
	    printf("Write size = %ld, not %ld\n", sz, strsize);
	}
	pos += strsize*nprocs;
    }
    close(fd);
}

static void
do_read(char *fnm, off64_t offset, void *bufp, size_t busiz)
{
    int		fd, iter;
    size_t	sz;
    off64_t	pos;
    
    if ((fd = open(fnm, O_RDONLY)) < 0) {
	fprintf(stderr, "Cannot open file %s\n", fnm);
	exit(-1);
    }
    pos = offset;
    for (iter = 0; iter < len; iter++) {
	VERBOSE {
	    printf("[%d] iter=%d pos=%ld strsize(%ld)\n",
		   myrank, iter, pos, strsize);
	}
	if (vflag) {
	    fillin(bufp, bufsiz, -1);
	}
	sz = read_stripe(fd, bufp, strsize, pos);
	if (sz != strsize) {
	    printf("Write size = %ld, not %ld\n", sz, strsize);
	}
	if (vflag) {
	    errors += verify(bufp, bufsiz, 0);
	}
	pos += strsize*nprocs;
    }
    close(fd);
}

int
main(int argc, char **argv)
{
    char	*fnm;
    off64_t	offset;
    double	tot_fsize;

    if (dflag) {
	printf("MAIN STARTS\n");
    }
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    VERBOSE {
	if (myrank == 0) {
	    printf("nprocs(%d) myrank(%d)\n", nprocs, myrank);
	}
    }
    test_parse_args(argc, argv);

    fnm = "tdata";
    if (fname[0]) fnm = fname;
    offset = strsize * myrank;
    bufsiz = strsize*len;
    bufp = malloc(bufsiz);
    if (bufp == NULL) {
	fprintf(stderr, "Cannot allocate buffer memory "
		"(size = %lf MiB, stripe size = %lf KiB, length = %ld)\n",
		(float)bufsiz/(1024.0*1024.0), (float)strsize/1024.0, len);
	exit(-1);
    }
    timer_init();
    tot_fsize = ((double)(bufsiz*nprocs))/(1024.0*1024.);
    if (myrank == 0) {
	printf("          nprocs: %d\n"
	       "     stripe size: %ld\n"
	       " proc write size: %f kB\n"
	       "total write size: %f MiB\n"
	       "              hz: %ld\n"
	       "           debug: %d\n"
	       "       file name: %s\n",
	       nprocs, strsize,
	       (float)bufsiz/1024.0,
	       tot_fsize, timer_hz, dflag, fnm);
    }
    if (rwflag & DO_WRITE) {
	timer_st[0] = tick_time();
	do_write(fnm, offset, bufp, bufsiz);
	timer_et[0] = tick_time();
    }
    if (rwflag & DO_READ) {
	timer_st[1] = tick_time();
	do_read(fnm, offset, bufp, bufsiz);
	timer_et[1] = tick_time();
    }
    if (myrank == 0) {
	double	bw, eltime;
	if (errors) {
	    printf("\nERROR:  # of errors %d\n", errors);
	} else {
	    printf("SUCCESS\n");
	    if (rwflag & DO_WRITE) {
		eltime = TIMER_SECOND(timer_et[0] - timer_st[0]);
		bw = tot_fsize/eltime;
		printf("\tWrite: \n"
		       "\t   Time: %12.9f second\n"
		       "\t     BW: %12.9f MiB/sec\n",
		       (float) eltime, (float) bw);
	    }
	    if (rwflag & DO_READ) {
		eltime = TIMER_SECOND(timer_et[1] - timer_st[1]);
		bw = tot_fsize/eltime;
		printf("\tRead: \n"
		       "\t   Time: %12.9f second\n"
		       "\t     BW: %12.9f MiB/sec\n",
		       (float) eltime, (float) bw);
	    }
	}
    }
    MPI_Finalize();
    return 0;
}

static FILE	*logfp;
static char	logname[PATH_MAX];

void
redirect()
{
    if (myrank == -1) {
	fprintf(stderr, "Too early for calling %s\n", __func__);
	return;
    }
    if (stderr != logfp) {
        char    *cp1 = getenv("IOMIDDLE_LOG_DIR");
        char    *cp2 = getenv("PJM_JOBID");
        if (cp1) {
	    if (cp2) {
		sprintf(logname, "%s/log-%s-%d", cp1, cp2, myrank);
	    } else {
		sprintf(logname, "%s/log-%d", cp1, myrank);
	    }
        } else {
	    if (cp2) {
		sprintf(logname, "log-%s-%d", cp2, myrank);
	    } else {
		sprintf(logname, "log-%d", myrank);
	    }
        }
        if ((logfp = fopen(logname, "w")) == NULL) {
            /* where we have to print ? /dev/console ? */
            fprintf(stderr, "Cannot create the logfile: %s\n", logname);
        } else {
            fprintf(stderr, "stderr output is now stored in the logfile: %s\n", logname);
            fclose(stderr);
            stderr = logfp;
        }
    }
}
