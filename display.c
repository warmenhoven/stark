#include <curses.h>
#include "main.h"

void
display_init()
{
	initscr();
	keypad(stdscr, TRUE);
	nonl();
	raw();
	noecho();

}

/* vim:set ts=4 sw=4 noet ai tw=80: */
