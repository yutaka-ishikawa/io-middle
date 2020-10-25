#include "testlib.h"

int
main(int argc, char **argv)
{
    char	*fname;
    unsigned int *data;
    FILE	*fp;
    off64_t	fpos;
    size_t	sz, tot = 0;
    int		lc, sc, i;
    char	marker;
    int		errs = 0;

    test_parse_args(argc, argv);

    if (strcnt == 0) {
	fprintf(stderr, "stripe count (nprocs) must be speficied "
		"using option -c\n"
		" Using default value 4\n");
	strcnt = 4;
    }
    printf(" stripe size: %ld\n"
	   "stripe count: %d\n"
	   "      length: %ld\n",
	   strsize, strcnt, len);
    data = malloc(strsize);
    if (data == NULL) {
	fprintf(stderr, "Cannot allocate buffer memory. size = %ld",
		strsize);
	exit(-1);
    }

    fname = "tdata";
    if ((fp = fopen(fname, "r")) == NULL) {
	fprintf(stderr, "Cannot open file %s\n", fname);
	exit(-1);
    }
    fpos = 0;
    for(lc = 0; lc < len; lc++) {
	for (sc = 0; sc < strcnt; sc++) {
	    sz = fread(data, 1, strsize, fp);
	    tot += sz;
	    if (sz != strsize) {
		fprintf(stderr,
			"File truncated: sz(%ld), "
			"must be %ld = len(%ld) strsize(%ld) * count(%d)\n",
			tot, len*strsize*strcnt, len, strsize, strcnt);
		goto err;
	    }
	    marker = sc;
	    for (i = 0; i < strsize/sizeof(unsigned int); i++) {
		if (data[i] != marker + i) {
		    fprintf(stderr, "data on position(%ld) must be %d, but %d\n",
			    fpos + i, marker + i, data[i]);
		    errs++;
		    if (errs > 20) goto err;
		}
	    }
	    fpos += strsize;
	}
    }
    printf("total read size = %ld\n", fpos);
    printf("Success\n");
err:
    fclose(fp);
}
