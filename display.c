#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"

static enum {
	ACCT_LIST,
	ACCT_DETAIL
} display_mode;

static int skip = 0;

static account *curr_acct = NULL;

static list *disp_acct = NULL;

static account *
select_next_acct(account *a)
{
	account *p;
	list *l;

	/* first try and select a child */
	if (a->expanded && a->subs)
		return a->subs->data;

	/* if we can't then try and select our sibling. start by thinking that maybe
	 * we're a top-level account */
	if (!a->parent) {
		l = list_find(accounts, a);
		if (l->next) {
			return l->next->data;
		} else {
			/* we are a top-level account but we don't have a subsequent
			 * sibling. also we don't have any more selectable children. so
			 * we're done! */
			return a;
		}
	}

	/* we're not a top-level account, try and select our next sibling */
	l = list_find(a->parent->subs, a);
	if (l->next) {
		return l->next->data;
	}

	/* we have no next sibling. try and get our parent's next sibling, etc. */
	p = a->parent;
	while (p) {
		/* our parent is top-level */
		if (!p->parent) {
			l = list_find(accounts, p);
			if (l->next) {
				/* return the next top-level account */
				return l->next->data;
			} else {
				/* we got to the end of the list! break. */
				break;
			}
		}

		l = list_find(p->parent->subs, p);
		if (l->next) {
			/* our parent has a sibling, let's move to them */
			return l->next->data;
		} else {
			/* our parent is the last child, move to our parent's parent */
			p = p->parent;
		}
	}

	/* we are the end of the line. return ourselves! */
	return a;
}

static account *
get_last_expanded(account *a)
{
	while (a->expanded && a->subs) {
		list *l = a->subs;
		while (l->next) l = l->next;
		a = l->data;
	}

	return a;
}

static account *
select_prev_acct(account *a)
{
	list *l;

	if (!a->parent) {
		list *l = list_find(accounts, a);
		if (l->prev) {
			return get_last_expanded(l->prev->data);
		} else {
			return a;
		}
	}

	l = list_find(a->parent->subs, a);
	if (l->prev) {
		return get_last_expanded(l->prev->data);
	} else {
		return a->parent;
	}
}

static void
draw_accts(list *l, int depth, int *line, int *s)
{
	int i;

	while (l) {
		account *a = l->data;
		l = l->next;

		if (!*s) {
			move((*line)++, 0);

			if (a->selected)
				attron(A_REVERSE);

			for (i = depth; i > 0; i--) {
				account *parent = a->parent, *child = a;
				list *x;
				int j;

				for (j = 0; j < i; j++) {
					child = parent;
					parent = child->parent;
				}

				if (parent) {
					x = list_find(parent->subs, child);
				} else {
					x = list_find(accounts, child);
				}

				if (x->next)
					addch(ACS_VLINE);
				else
					addch(' ');
				addch(' ');
			}

			if (a->subs) {
				if (a->expanded)
					addch('-');
				else
					addch('+');
			} else if (l) {
				addch(ACS_LTEE);
			} else {
				addch(ACS_LLCORNER);
			}
			addch(' ');
			addstr(a->name);

			for (i = 0; i < COLS - strlen(a->name) - (2 * (depth + 1)); i++)
				addch(' ');

			if (a->selected)
				attroff(A_REVERSE);

			disp_acct = list_append(disp_acct, a);

			if (*line >= LINES)
				return;
		} else {
			(*s)--;
		}

		if (a->expanded) {
			draw_accts(a->subs, depth + 1, line, s);

			if (*line >= LINES)
				return;
		}
	}
}

static void
redraw_screen()
{
	int line = 0, s = skip;

	switch (display_mode) {
	case ACCT_LIST:
		list_free(disp_acct);
		disp_acct = NULL;
		draw_accts(accounts, 0, &line, &s);
		break;
	case ACCT_DETAIL:
		break;
	}

	refresh();
}

static void
display_end()
{
	clear();
	refresh();
	endwin();
}

static void
expand_all(account *a)
{
	list *l = a->subs;

	if (!l)
		return;

	a->expanded = 1;

	while (l) {
		expand_all(l->data);
		l = l->next;
	}
}

static void
close_all(account *a)
{
	list *l = a->subs;

	if (!l)
		return;

	a->expanded = 0;

	while (l) {
		expand_all(l->data);
		l = l->next;
	}
}

static int
list_handle_key(int c)
{
	account *ta;
	list *l;

	switch (c) {
	case 12:	/* ^L */
		clear();
		redraw_screen();
		break;
	case 13:	/* ^M */
		display_mode = ACCT_DETAIL;
		clear();
		redraw_screen();
		break;
	case 'h':
		if (!curr_acct->parent)
			break;

		l = list_find(disp_acct, curr_acct->parent);
		if (l) {
			curr_acct->selected = 0;
			curr_acct = curr_acct->parent;
			curr_acct->selected = 1;
			redraw_screen();
		} else {
			account *a = curr_acct;
			do {
				a = select_prev_acct(a);
				if (!list_find(disp_acct, a))
					skip--;
			} while (a != curr_acct->parent);
			curr_acct->selected = 0;
			curr_acct = a;
			curr_acct->selected = 1;
			redraw_screen();
		}
		break;
	case 'j':
		l = list_find(disp_acct, curr_acct);
		if (l && l->next) {
			curr_acct->selected = 0;
			curr_acct = l->next->data;
			curr_acct->selected = 1;
			redraw_screen();
		} else {
			account *a = select_next_acct(curr_acct);
			if (a != curr_acct) {
				skip++;
				curr_acct->selected = 0;
				curr_acct = a;
				curr_acct->selected = 1;
				mvaddch(LINES - 1, COLS - 1, '\n');
				redraw_screen();
			}
		}
		break;
	case 'k':
		l = list_find(disp_acct, curr_acct);
		if (l && l->prev) {
			curr_acct->selected = 0;
			curr_acct = l->prev->data;
			curr_acct->selected = 1;
			redraw_screen();
		} else {
			account *a = select_prev_acct(curr_acct);
			if (a != curr_acct) {
				skip--;
				curr_acct->selected = 0;
				curr_acct = a;
				curr_acct->selected = 1;
				redraw_screen();
			}
		}
		break;
	case 'q':
		display_end();
		return 1;
	case '-':
		if (curr_acct->subs) {
			int s = 0;
			l = disp_acct;
			while (l->data != curr_acct) {
				l = l->next;
				s++;
			}
			move(s, 0);
			clrtobot();
			curr_acct->expanded = 0;
			redraw_screen();
		}
		break;
	case '+':
		if (curr_acct->subs) {
			curr_acct->expanded = 1;
			/* no need to clear since we're now drawing more */
			redraw_screen();
		}
		break;
	case ' ':
		if (!curr_acct->subs)
			break;

		if (curr_acct->expanded) {
			int s = 0;
			l = disp_acct;
			while (l->data != curr_acct) {
				l = l->next;
				s++;
			}
			move(s, 0);
			clrtobot();
			curr_acct->expanded = 0;
		} else {
			curr_acct->expanded = 1;
			/* no need to clear since we're now drawing more */
		}
		redraw_screen();

		break;
	case '*':
		expand_all(curr_acct);
		redraw_screen();
		break;
	case '/':
		if (curr_acct->subs) {
			int s = 0;
			l = disp_acct;
			while (l->data != curr_acct) {
				l = l->next;
				s++;
			}
			move(s, 0);
			clrtobot();
			close_all(curr_acct);
			redraw_screen();
		}
		break;
	case KEY_HOME:
		curr_acct->selected = 0;
		curr_acct = accounts->data;
		curr_acct->selected = 1;
		skip = 0;
		redraw_screen();
		break;
	case KEY_END:
		curr_acct->selected = 0;

		l = accounts;
		do {
			while (l->next) l = l->next;
			ta = l->data;
			l = ta->subs;
		} while (l && ta->expanded);

		if (!list_find(disp_acct, ta)) {
			account *a = curr_acct;
			do {
				a = select_next_acct(a);
				if (!list_find(disp_acct, a))
					skip++;
			} while (a != ta);
		}

		curr_acct = ta;
		curr_acct->selected = 1;
		redraw_screen();
		break;
	case KEY_RESIZE:
		endwin();
		initscr();
		clear();
		redraw_screen();
		break;
	default:
		break;
	}

	return 0;
}

static int
detail_handle_key(int c)
{
	switch (c) {
	case 'q':
		display_mode = ACCT_LIST;
		clear();
		redraw_screen();
		break;
	}
	return 0;
}

void
display_run()
{
	int c;

	if (!accounts) {
		fprintf(stderr, "no accounts!\n");
		exit(1);
	}

	curr_acct = accounts->data;
	curr_acct->selected = 1;

	initscr();
	keypad(stdscr, TRUE);
	nonl();
	raw();
	noecho();
	notimeout(stdscr, TRUE);
	curs_set(0);

	redraw_screen();

	while ((c = getch())) {
		switch (display_mode) {
		case ACCT_LIST:
			if (list_handle_key(c))
				return;
			break;
		case ACCT_DETAIL:
			if (detail_handle_key(c))
				return;
			break;
		}
	}
}

/* vim:set ts=4 sw=4 noet ai tw=80: */
