BEGIN	{ first = 1; }
/PJM_LLIO_STRIPE_COUNT=/ { 
	    split($1, STRIPE, "=");
	    if (first) {
		print "sripe count, nodes, forwarder, MiB/s, MiB/s, MiB/s, ", ARGV[1];
		first = 0;
	    }
	}
/IOMIDDLE_FORWARDER/ { forwarder= $3; }
/nodes/ { nodes = $3; }
/Summary/ { effective = 1;}
/LD_PRELOAD/ { io_middle = 1; }
/write/ {
	    if (effective == 1) {
		printf "%d, %d, %d, %d, %d, %d\n", STRIPE[2], nodes, forwarder, $2, $3, $4;
		effective = 0; nodes = 0; forwarder = 0; io_middle = 0;
	    }
	}
