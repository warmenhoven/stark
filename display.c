#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"

static enum {
	ACCT_LIST,
	ACCT_DETAIL
} display_mode;

static int skip_acct = 0;
static list *disp_acct = NULL;
static account *curr_acct = NULL;

static int skip_trans = 0;
static list *disp_trans = NULL;
static transaction *curr_trans = NULL;

static int
build_exp_accts(list *accts, list **l)
{
	int i = 0;

	while (accts) {
		account *a = accts->data;
		accts = accts->next;

		*l = list_append(*l, a);
		i++;

		if (a->expanded)
			i += build_exp_accts(a->subs, l);
	}

	return i;
}

static float
get_value(account *a)
{
	float total = 0;
	list *l = a->subs;

	if (a->commodity && a->commodity->quote)
			total = a->quantity * a->commodity->quote->value;
	else if (!a->commodity)
		total += a->quantity;

	while (l) {
		total += get_value(l->data);
		l = l->next;
	}

	return total;
}

static void
draw_acct(account *a, int line)
{
	account *dfind = a;
	char value[1024];
	int sibling = 0;
	int depth = 0;
	int dlen = 0;
	int i;

	move(line, 0);

	if (a->selected)
		attron(A_REVERSE);

	while (dfind->parent) {
		dfind = dfind->parent;
		depth++;
	}

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
		dlen += 2;
	}

	if (a->parent) {
		list *l = list_find(a->parent->subs, a);
		if (l->next)
			sibling = 1;
	}

	if (a->subs) {
		if (a->expanded)
			addch('-');
		else
			addch('+');
	} else if (sibling) {
		addch(ACS_LTEE);
	} else {
		addch(ACS_LLCORNER);
	}
	addch(' ');
	dlen += 2;
	addstr(a->name);
	dlen += strlen(a->name);

	for (; dlen < 40; dlen++)
		addch(' ');

	if (a->commodity) {
		char quant[1024];
		dlen += sprintf(quant, "%.3f %s", a->quantity, a->commodity->id);
		addstr(quant);
	}

	for (; dlen < 65; dlen++)
		addch(' ');

	addch('$');
	dlen++;

	dlen += sprintf(value, "%.2f", get_value(a));
	for (; dlen < 80; dlen++)
		addch(' ');
	addstr(value);

	/* can't use clrtoeol() because we want A_REVERSE to eol */
	for (; dlen < COLS; dlen++)
		addch(' ');

	if (a->selected)
		attroff(A_REVERSE);
}

static void
draw_accounts()
{
	list *exp = NULL, *l;
	int i;

	build_exp_accts(accounts, &exp);
	l = list_nth(exp, skip_acct);

	for (i = 0; l && i < LINES; i++) {
		disp_acct = list_append(disp_acct, l->data);
		draw_acct(l->data, i);
		l = l->next;
	}

	list_free(exp);
}

static void
draw_trans_header()
{
	int i, j = 0;

	list *l = curr_acct->transactions;
	while (l && l->next) l = l->next;

	move(0, 0);
	addstr("   DATE   ");
	addch(ACS_VLINE);
	addstr(" NUM  ");
	addch(ACS_VLINE);
	addstr(" DESCRIPTION            ");
	addch(ACS_VLINE);
	addstr(" TRANSFER               ");
	addch(ACS_VLINE);
	addstr(" R ");
	addch(ACS_VLINE);
	addstr("    INC     ");
	addch(ACS_VLINE);
	addstr("    DEC     ");
	addch(ACS_VLINE);
	addstr(" BALANCE");
	clrtoeol();

	move(1, 0);
	for (i = 0; i < 10; i++)
		addch(ACS_HLINE);
	addch(ACS_BTEE);
	j += i + 1;
	for (i = 0; i < 6; i++)
		addch(ACS_HLINE);
	addch(ACS_BTEE);
	j += i + 1;
	for (i = 0; i < 24; i++)
		addch(ACS_HLINE);
	addch(ACS_BTEE);
	j += i + 1;
	for (i = 0; i < 24; i++)
		addch(ACS_HLINE);
	addch(ACS_BTEE);
	j += i + 1;
	for (i = 0; i < 3; i++)
		addch(ACS_HLINE);
	addch(ACS_BTEE);
	j += i + 1;
	for (i = 0; i < 12; i++)
		addch(ACS_HLINE);
	addch(ACS_BTEE);
	j += i + 1;
	for (i = 0; i < 12; i++)
		addch(ACS_HLINE);
	addch(ACS_BTEE);
	j += i + 1;
	for (; j < COLS; j++)
		addch(ACS_HLINE);

	for (i = 0; i < COLS; i++)
		addch(ACS_HLINE);
}

static float
trans_balance(transaction *t, account *a)
{
	float balance = 0;
	list *l = t->splits;

	while (l) {
		split *s = l->data;
		l = l->next;

		if (strcmp(s->account, a->id))
			continue;

		balance += s->quantity;
	}

	return balance;
}

static void
draw_trans(transaction *t, int line, float total)
{
	float balance = 0;
	char tmpstr[11];
	int i;

	if (t->selected)
		attron(A_REVERSE);

	move(line, 0);
	for (i = 0; i < COLS; i++)
		addch(' ');

	strftime(tmpstr, 11, "%m/%d/%Y", localtime(&t->posted));
	mvaddstr(line, 0, tmpstr);

	if (t->num) {
		sprintf(tmpstr, "%d", t->num);
		mvaddstr(line, 12, tmpstr);
	}

	mvaddstr(line, 19, t->description);
	mvaddstr(line, 41, "   ");

	if (list_length(t->splits) > 2) {
		mvaddstr(line, 44, "- Split Transaction -");
	} else if (!t->splits->next) {
		mvaddstr(line, 44, "                     ");
	} else {
		account *a;
		split *s = t->splits->data;
		split *o = t->splits->data;

		if (!strcmp(s->account, curr_acct->id))
			s = t->splits->next->data;
		else
			o = t->splits->next->data;

		a = find_account(s->account, accounts);
		if (!a)
			mvaddstr(line, 44, "                     ");
		else
			mvaddstr(line, 44, a->name);

		mvaddstr(line, 66, "   ");
		mvaddch(line, 69, o->recstate);
		mvaddstr(line, 70, "   ");
	}

	balance = trans_balance(t, curr_acct);
	if (balance > 0)
		sprintf(tmpstr, "$%.02f", balance);
	else
		sprintf(tmpstr, "$%.02f", 0 - balance);
	if (balance > 0) {
		mvaddstr(line, 73, tmpstr);
		for (i = 84; i < 96; i++)
			mvaddch(line, i, ' ');
	} else {
		for (i = 73; i < 86; i++)
			mvaddch(line, i, ' ');
		mvaddstr(line, 86, tmpstr);
	}
	mvaddstr(line, 96, "   ");

	sprintf(tmpstr, "%.02f", balance + total);
	mvaddstr(line, 99, tmpstr);

	if (t->selected)
		attroff(A_REVERSE);
}

static void
draw_transactions()
{
	list *l = curr_acct->transactions;
	int i = skip_trans;
	float balance = 0;
	int line = 3;

	list_free(disp_trans);
	disp_trans = NULL;

	while (i--) {
		transaction *t = l->data;
		balance += trans_balance(t, curr_acct);
		l = l->next;
	}

	while (l && line < LINES) {
		transaction *t = l->data;
		draw_trans(t, line++, balance);
		disp_trans = list_append(disp_trans, t);
		balance += trans_balance(t, curr_acct);
		l = l->next;
	}
}

static void
redraw_screen()
{
	switch (display_mode) {
	case ACCT_LIST:
		list_free(disp_acct);
		disp_acct = NULL;
		draw_accounts();
		break;
	case ACCT_DETAIL:
		draw_trans_header();
		draw_transactions();
		break;
	}

	refresh();
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
		close_all(l->data);
		l = l->next;
	}
}

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
recalc_skip_acct()
{
	list *exp = NULL;
	list *l;
	int len;
	int i;

	len = build_exp_accts(accounts, &exp);

	l = list_find(exp, curr_acct);
	i = list_length(l->next);

	list_free(exp);

	if ((len - skip_acct) - i < LINES) {
		/* len - skip_acct is how many accounts are displayed from the first one
		 * that's not skipped. (len - skip_acct) - i is how many accounts there
		 * are between the first displayed and the current account. if that
		 * value is less than the number of lines, then the current account is
		 * still displayed and we don't need to adjust the skip_acct. */
		return;
	}

	skip_acct = len - LINES - i;
}

static void
init_trans()
{
	list *l = curr_acct->transactions;
	int i = 0;

	curr_trans = NULL;

	while (l) {
		transaction *t = l->data;
		l = l->next;
		if (l) {
			t->selected = 0;
		} else {
			t->selected = 1;
			curr_trans = t;
		}
		i++;
	}

	/* the header is three lines long */
	if (i >= LINES - 3)
		skip_trans = i - (LINES - 3);
	else
		skip_trans = 0;
}

static int
list_handle_key(int c)
{
	account *ta;
	list *l;

	switch (c) {
	case 'q':
		return 1;

	case 13:	/* ^M */
		display_mode = ACCT_DETAIL;
		clear();
		init_trans();
		redraw_screen();
		break;

	/* MOTION */
	case 'h':
	case KEY_LEFT:
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
					skip_acct--;
			} while (a != curr_acct->parent);
			curr_acct->selected = 0;
			curr_acct = a;
			curr_acct->selected = 1;
			redraw_screen();
		}
		break;
	case KEY_DOWN:
	case 'j':
	case 14:	/* ^N */
		l = list_find(disp_acct, curr_acct);
		if (l && l->next) {
			curr_acct->selected = 0;
			curr_acct = l->next->data;
			curr_acct->selected = 1;
			redraw_screen();
		} else {
			account *a = select_next_acct(curr_acct);
			if (a != curr_acct) {
				skip_acct++;
				curr_acct->selected = 0;
				curr_acct = a;
				curr_acct->selected = 1;
				mvaddch(LINES - 1, COLS - 1, '\n');
				redraw_screen();
			}
		}
		break;
	case KEY_UP:
	case 'k':
	case 16:	/* ^P */
		l = list_find(disp_acct, curr_acct);
		if (l && l->prev) {
			curr_acct->selected = 0;
			curr_acct = l->prev->data;
			curr_acct->selected = 1;
			redraw_screen();
		} else {
			account *a = select_prev_acct(curr_acct);
			if (a != curr_acct) {
				skip_acct--;
				curr_acct->selected = 0;
				curr_acct = a;
				curr_acct->selected = 1;
				redraw_screen();
			}
		}
		break;
	case 1:		/* ^A */
	case KEY_HOME:
		curr_acct->selected = 0;
		curr_acct = accounts->data;
		curr_acct->selected = 1;
		skip_acct = 0;
		redraw_screen();
		break;
	case 5:		/* ^E */
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
					skip_acct++;
			} while (a != ta);
		}

		curr_acct = ta;
		curr_acct->selected = 1;
		redraw_screen();
		break;

	/* EXPANSION */
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
	case KEY_RIGHT:
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

	default:
		break;
	}

	return 0;
}

static int
detail_handle_key(int c)
{
	list *l;

	if (!curr_trans && c != 'q') {
		return 0;
	}

	switch (c) {
	case 'q':
		display_mode = ACCT_LIST;
		clear();
		redraw_screen();
		break;

	/* MOTION */
	case KEY_DOWN:
	case 'j':
	case 14:	/* ^N */
		l = list_find(disp_trans, curr_trans);
		if (l->next) {
			curr_trans->selected = 0;
			curr_trans = l->next->data;
			curr_trans->selected = 1;
			redraw_screen();
		} else {
			l = list_find(curr_acct->transactions, curr_trans);
			if (l->next) {
				skip_trans++;
				curr_trans->selected = 0;
				curr_trans = l->next->data;
				curr_trans->selected = 1;
				redraw_screen();
			}
		}
		break;
	case KEY_UP:
	case 'k':
	case 16:	/* ^P */
		l = list_find(disp_trans, curr_trans);
		if (l && l->prev) {
			curr_trans->selected = 0;
			curr_trans = l->prev->data;
			curr_trans->selected = 1;
			redraw_screen();
		} else {
			l = list_find(curr_acct->transactions, curr_trans);
			if (l->prev) {
				skip_trans--;
				curr_trans->selected = 0;
				curr_trans = l->prev->data;
				curr_trans->selected = 1;
				redraw_screen();
			}
		}
		break;
	case KEY_HOME:
	case 1:		/* ^A */
		curr_trans->selected = 0;
		curr_trans = curr_acct->transactions->data;
		curr_trans->selected = 1;
		skip_trans = 0;
		redraw_screen();
		break;
	case KEY_END:
	case 5:		/* ^E */
		init_trans();
		redraw_screen();
		break;

	default:
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
		if (c == KEY_RESIZE) {
			endwin();
			initscr();
			recalc_skip_acct();
			clear();
			redraw_screen();
			continue;
		} else if (c == 12) {	/* ^L */
			clear();
			redraw_screen();
			continue;
		}

		switch (display_mode) {
		case ACCT_LIST:
			if (list_handle_key(c)) {
				clear();
				refresh();
				endwin();
				return;
			}
			break;
		case ACCT_DETAIL:
			if (detail_handle_key(c)) {
				clear();
				refresh();
				endwin();
				return;
			}
			break;
		}
	}
}

/* vim:set ts=4 sw=4 noet ai tw=80: */
