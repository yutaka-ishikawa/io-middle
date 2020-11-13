BEGIN	{ first = 0; }
/PJM_LLIO_STRIPE_COUNT=/ {
	    split($1, STR_COUNT, "=");
	    first += 1;
	    if (first == 2) {
		print "nodes, sripe count, forwarder, MiB/s, MiB/s, MiB/s, stripe size =", STR_SIZE[2], ", file =", ARGV[1];
		first += 1;
	    }
	}
/PJM_LLIO_STRIPE_SIZE=/ {
	    split($1, STR_SIZE, "=");
	    first += 1;
	    if (first == 2) {
		print "nodes, sripe count, forwarder, MiB/s, MiB/s, MiB/s, stripe size =", STR_SIZE[2], ", file =", ARGV[1];
		first += 1;
	    }
	}
/IOMIDDLE_FORWARDER/ { forwarder= $3; }
/nodes/ { nodes = $3; }
/Summary/ { effective = 1;}
/LD_PRELOAD/ { io_middle = 1; }
/write/ {
	    if (effective == 1) {
		printf "%d, %d, %d, %d, %d, %d\n", nodes, STR_COUNT[2], forwarder, $2, $3, $4;
		effective = 0; nodes = 0; forwarder = 0; io_middle = 0;
	    }
	}
