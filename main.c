#include <stdio.h>
#include <stdlib.h>
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
	if (argc != 2)
		usage(argv[0]);

	display_init();

	gnucash_init(argv[1]);

	display_run();

	display_end();

	return 0;
}

/* vim:set ts=4 sw=4 noet ai tw=80: */
