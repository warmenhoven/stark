#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "main.h"

static void
usage(char *name)
{
	printf("Usage: %s <gnucash-data-file>\n", name);
	exit(1);
}

int
main(int argc, char **argv)
{
	struct timeval begin, end;

	if (argc != 2)
		usage(argv[0]);

	printf("opening %s...\n", argv[1]);
	gettimeofday(&begin, NULL);
	gnucash_init(argv[1]);
	gettimeofday(&end, NULL);
	printf("succeeded in %lu.%06lu sec\n",
		   (end.tv_sec - begin.tv_sec),
		   (end.tv_usec - begin.tv_usec));

	display_run(argv[1]);

	return 0;
}

/* vim:set ts=4 sw=4 noet ai tw=80: */
