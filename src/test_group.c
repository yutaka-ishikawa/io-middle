#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define MPI_CALL(mpicall) do {			\
    int	rc;					\
    rc = mpicall;				\
    if (rc != 0) {				\
	fprintf(stderr, "%s MPI error\n", __func__);\
	abort();				\
    }						\
} while(0);

int nprocs, myrank;

int
main(int argc, char **argv)
{
    /* 384, 4 fowarder (96), 0, 96, 192, 288 */
    MPI_Group	group, Fwgrp;
    MPI_Comm	Fwcomm;
    int	Fwrdr = 4;
    int	fprocs;
    int	ranks[128];
    int	i;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    fprocs = nprocs/Fwrdr;
    for (i = 0; i < Fwrdr; i++) {
	ranks[i] = fprocs*i;
	if (myrank == 0) printf("Fowarder = %d\n", ranks[i]);
    }
    printf("nprocs(%d) Fwrdr(%d) fprocs(%d)\n", nprocs, Fwrdr, fprocs);
    MPI_CALL(MPI_Comm_group(MPI_COMM_WORLD, &group));
    MPI_CALL(MPI_Group_incl(group, Fwrdr, ranks, &Fwgrp));
    MPI_CALL(MPI_Comm_create(MPI_COMM_WORLD, Fwgrp, &Fwcomm));

    printf("Now testing\n");

    MPI_Finalize();
    return 0;
}
