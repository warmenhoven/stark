#include <curses.h>
#include <signal.h>
#include "main.h"

static void
redraw_screen()
{
}

static void
sigwinch(int sig)
{
	endwin();
	initscr();
	clear();
	ungetch(KEY_RESIZE);
	redraw_screen();
}

void
display_init()
{
	initscr();
	keypad(stdscr, TRUE);
	nonl();
	raw();
	noecho();

	signal(SIGWINCH, sigwinch);
}

void
display_run()
{
	int c;

	redraw_screen();

	while ((c = getch()) != ERR) {
		switch (c) {
		case 'q':
			return;
		case KEY_RESIZE:
			endwin();
			initscr();
			clear();
			redraw_screen();
			break;
		default:
			break;
		}
	}
}

void
display_end()
{
	endwin();
}

/* vim:set ts=4 sw=4 noet ai tw=80: */
