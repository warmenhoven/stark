#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif
#include <assert.h>
#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"

static enum {
	ACCT_LIST,
	REGISTER,
	JOURNAL
} display_mode;

static int skip_acct = 0;
static list *disp_acct = NULL;
static account *curr_acct = NULL;

static list *trans_list = NULL;
static int skip_trans = 0;
static list *disp_trans = NULL;
static transaction *curr_trans = NULL;
#define REG_HDR 4

static account *journal_parent = NULL;
static unsigned int journal_pre = 0, journal_post = 0;

static int
build_expd_accts(list *accts, list **l)
{
	int i = 0;

	while (accts) {
		account *a = accts->data;
		accts = accts->next;

		*l = list_append(*l, a);
		i++;

		if (a->expanded)
			i += build_expd_accts(a->subs, l);
	}

	return i;
}

void
value_add(value *a, value *b)
{
	if (a->sig > b->sig) {
		value tmp;
		tmp.val = b->val;
		tmp.sig = b->sig;
		while (tmp.sig < a->sig) {
			tmp.val *= 10;
			tmp.sig++;
		}
		a->val += tmp.val;
	} else {
		while (a->sig < b->sig) {
			a->val *= 10;
			a->sig++;
		}
		a->val += b->val;
	}
}

void
value_multiply(value *r, value *a, value *b)
{
	value tmp;
	tmp.val = a->val;
	tmp.sig = a->sig;
	while (tmp.sig && (tmp.val % 10) == 0) {
		tmp.val /= 10;
		tmp.sig--;
	}
	r->val = b->val;
	r->sig = b->sig;
	while (r->sig && (r->val % 10) == 0) {
		r->val /= 10;
		r->sig--;
	}
	r->val *= tmp.val;
	r->sig += tmp.sig;
}

static void
get_value(account *a, value *total)
{
	list *l = a->subs;
	value curr;

	curr.val = 0;
	curr.sig = 0;

	if (a->commodity && a->commodity->quote)
		value_multiply(&curr, &a->quantity, &a->commodity->quote->value);
	else if (!a->commodity)
		value_add(&curr, &a->quantity);

	while (l) {
		get_value(l->data, &curr);
		l = l->next;
	}

	value_add(total, &curr);
}

double
value_to_double(value *v)
{
	double ret = v->val;
	int s = v->sig;
	while (s--)
		ret /= 10;
	return ret;
}

static void
draw_acct(account *a, int line)
{
	account *dfind = a;
	char valstr[1024];
	int sibling = 0;
	int depth = 0;
	int dlen = 0;
	value total;
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

		assert(x);

		if (x->next)
			addch(ACS_VLINE);
		else
			addch(' ');
		addch(' ');
		dlen += 2;
	}

	if (a->parent) {
		list *l = list_find(a->parent->subs, a);
		assert(l);
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
		dlen += sprintf(quant, "%.3f %s", value_to_double(&a->quantity),
						a->commodity->id);
		addstr(quant);
	}

	for (; dlen < 65; dlen++)
		addch(' ');

	addch('$');
	dlen++;

	total.val = 0;
	total.sig = 0;
	get_value(a, &total);
	dlen += sprintf(valstr, "%.2f", value_to_double(&total));
	for (; dlen < 80; dlen++)
		addch(' ');
	addstr(valstr);

	/* can't use clrtoeol() because we want A_REVERSE to eol */
	for (; dlen < COLS; dlen++)
		addch(' ');

	if (a->selected)
		attroff(A_REVERSE);
}

static void
draw_accounts(void)
{
	list *expd = NULL, *l;
	int i;

	build_expd_accts(accounts, &expd);
	l = list_nth(expd, skip_acct);

	for (i = 0; l && i < LINES; i++) {
		disp_acct = list_append(disp_acct, l->data);
		draw_acct(l->data, i);
		l = l->next;
	}

	list_free(expd);
}

static char *
full_acct_name(account *a)
{
	account *b = a;
	int sz = 0, cur = 0;;
	char *x;
	while (b) {
		sz += strlen(b->name) + 1;
		b = b->parent;
	}
	cur = sz;
	x = malloc(sz);
	x[sz - 1] = 0;
	while (a) {
		cur -= strlen(a->name) + 1;
		memcpy(x + cur, a->name, strlen(a->name));
		if (a->parent)
			x[cur - 1] = ':';
		a = a->parent;
	}

	return x;
}

static void
draw_trans_header(void)
{
	int i, j = 0;
	char *x;

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
	if (display_mode != JOURNAL)
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

	if (display_mode != JOURNAL || journal_parent) {
		x = full_acct_name(curr_acct);
		mvaddstr(2, 0, x);
		if (display_mode == JOURNAL)
			mvaddstr(2, strlen(x) + 1, "and sub-accounts");
		free(x);
	} else {
		mvaddstr(2, 0, "General Ledger");
	}

	move(3, 0);
	for (i = 0; i < COLS; i++)
		addch(ACS_HLINE);
}

static double
trans_balance(transaction *t, account *a)
{
	value balance;
	list *l = t->splits;

	balance.val = 0;
	balance.sig = 0;

	while (l) {
		split *s = l->data;
		l = l->next;

		if (strcmp(s->account, a->id))
			continue;

		value_add(&balance, &s->quantity);
	}

	return value_to_double(&balance);
}

static void
draw_trans(transaction *t, int line, double total)
{
	double balance = 0;
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

	if (display_mode != JOURNAL) {
		if (list_length(t->splits) > 2) {
			list *l;
			mvaddstr(line, 44, "- Split Transaction -");
			l = t->splits;
			while (l) {
				split *s = l->data;
				l = l->next;
				if (!strcmp(s->account, curr_acct->id)) {
					mvaddch(line, 69, s->recstate);
					break;
				}
			}
		} else {
			account *a;
			split *s = t->splits->data;
			split *o = t->splits->data;

			assert(t->splits->next);

			if (!strcmp(s->account, curr_acct->id))
				s = t->splits->next->data;
			else
				o = t->splits->next->data;

			a = find_account(s->account);
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
	} else {
		for (i = 44; i < COLS; i++)
			mvaddch(line, i, ' ');
	}

	if (t->selected)
		attroff(A_REVERSE);
}

static void
draw_split(split *s, int line)
{
	char tmpstr[11];
	account *a;
	int i;

	if (s->selected)
		attron(A_REVERSE);

	move(line, 0);
	for (i = 0; i < COLS; i++)
		addch(' ');

	if (s->action)
		mvaddstr(line, 12, s->action);

	if (s->memo)
		mvaddstr(line, 19, s->memo);
	mvaddstr(line, 41, "   ");

	a = find_account(s->account);
	if (!a)
		mvaddstr(line, 44, "                     ");
	else
		mvaddstr(line, 44, a->name);

	mvaddstr(line, 66, "   ");
	mvaddch(line, 69, s->recstate);
	mvaddstr(line, 70, "   ");

	if (s->quantity.val > 0)
		sprintf(tmpstr, "$%.02f", value_to_double(&s->quantity));
	else
		sprintf(tmpstr, "$%.02f", 0 - value_to_double(&s->quantity));
	if (s->quantity.val > 0) {
		mvaddstr(line, 73, tmpstr);
		for (i = 84; i < 96; i++)
			mvaddch(line, i, ' ');
	} else {
		for (i = 73; i < 86; i++)
			mvaddch(line, i, ' ');
		mvaddstr(line, 86, tmpstr);
	}
	mvaddstr(line, 96, "   ");

	if (s->selected)
		attroff(A_REVERSE);
}

static void
draw_transactions(void)
{
	list *l = trans_list, *x = NULL;
	int i = skip_trans;
	double balance = 0;
	int line = REG_HDR;

	list_free(disp_trans);
	disp_trans = NULL;

	assert(i >= 0);
	while (i--) {
		transaction *t = l->data;
		balance += trans_balance(t, curr_acct);
		if (display_mode == JOURNAL) {
			x = t->splits;
			while (i && x) {
				x = x->next;
				i--;
			}
		}
		l = l->next;
	}

	journal_pre = 0;
	while (x && line < LINES) {
		draw_split(x->data, line++);
		x = x->next;
		journal_pre++;
	}

	while (l && line < LINES) {
		transaction *t = l->data;
		draw_trans(t, line++, balance);
		if (display_mode == JOURNAL || t->expanded) {
			list *s = t->splits;
			journal_post = 0;
			while (s && line < LINES) {
				draw_split(s->data, line++);
				s = s->next;
				journal_post++;
			}
		}
		disp_trans = list_append(disp_trans, t);
		balance += trans_balance(t, curr_acct);
		l = l->next;
	}
}

static void
redraw_screen(void)
{
	switch (display_mode) {
	case ACCT_LIST:
		list_free(disp_acct);
		disp_acct = NULL;
		draw_accounts();
		break;
	case REGISTER:
	case JOURNAL:
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
		assert(l);
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
	assert(l);
	if (l->next) {
		return l->next->data;
	}

	/* we have no next sibling. try and get our parent's next sibling, etc. */
	p = a->parent;
	while (p) {
		/* our parent is top-level */
		if (!p->parent) {
			l = list_find(accounts, p);
			assert(l);
			if (l->next) {
				/* return the next top-level account */
				return l->next->data;
			} else {
				/* we got to the end of the list! break. */
				break;
			}
		}

		l = list_find(p->parent->subs, p);
		assert(l);
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
		l = list_find(accounts, a);
		assert(l);
		if (l->prev) {
			return get_last_expanded(l->prev->data);
		} else {
			return a;
		}
	}

	l = list_find(a->parent->subs, a);
	assert(l);
	if (l->prev) {
		return get_last_expanded(l->prev->data);
	} else {
		return a->parent;
	}
}

static void
recalc_skip_acct(void)
{
	list *expd = NULL;
	list *l;
	int len;
	int i;

	len = build_expd_accts(accounts, &expd);

	l = list_find(expd, curr_acct);
	assert(l);
	i = list_length(l->next);

	list_free(expd);

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
recalc_skip_trans(void)
{
	list *l;
	int len;
	int i;

	if (display_mode == ACCT_LIST)
		return;

	len = list_length(trans_list);

	l = list_find(trans_list, curr_trans);
	assert(l);
	i = list_length(l->next);

	if (display_mode == JOURNAL) {
		l = trans_list;
		while (l) {
			transaction *t = l->data;
			l = l->next;
			len += list_length(t->splits);
			if (t == curr_trans)
				break;
		}
	} else if (curr_trans->expanded) {
		len += list_length(curr_trans->splits);
	}

	if ((len - skip_trans) - i < (LINES - REG_HDR))
		return;

	skip_trans = len - (LINES - REG_HDR) - i;
}

static void
init_trans_list(void)
{
	if (display_mode == JOURNAL) {
		if (journal_parent) {
			trans_list = list_copy(journal_parent->transactions);
			build_trans_list(journal_parent->subs, &trans_list, 1);
		} else {
			build_trans_list(accounts, &trans_list, 1);
		}
	} else {
		trans_list = curr_acct->transactions;
	}
}

static void
init_trans(void)
{
	list *l;
	int i = 0;

	init_trans_list();

	l = trans_list;

	if (curr_trans) {
		curr_trans->selected = 0;
		curr_trans->expanded = 0;
	}
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
		if (display_mode == JOURNAL)
			i += list_length(t->splits);
	}

	if (i >= LINES - REG_HDR)
		skip_trans = i - (LINES - REG_HDR);
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

	case 10:	/* ^J */
	case 13:	/* ^M */
		if (curr_acct->placeholder)
			break;
		display_mode = REGISTER;
		clear();
		init_trans();
		redraw_screen();
		break;

	case 'G':
	case 'S':
		journal_parent = (c == 'S') ? curr_acct : NULL;
		display_mode = JOURNAL;
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
		assert(l);
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
static void
expand_transaction(void)
{
	int prelen, len, postlen;
	list *l;

	len = list_length(disp_trans);
	l = list_find(disp_trans, curr_trans);
	assert(l);
	postlen = list_length(l->next);
	prelen = len - postlen;

	len = list_length(curr_trans->splits);

	if (len + prelen >= LINES - REG_HDR) {
		/* more splits than space below */
		skip_trans += (len + prelen - (LINES - REG_HDR));
	}

	curr_trans->expanded = 1;
}

static void
unexpand_transaction(void)
{
	int len;
	list *l, *s;
	if (!curr_trans->expanded)
		return;
	len = list_length(disp_trans);
	l = list_find(disp_trans, curr_trans);
	assert(l);
	move(REG_HDR + len - list_length(l), 0);
	clrtobot();
	curr_trans->expanded = 0;

	s = curr_trans->splits;
	l = list_find(trans_list, curr_trans);
	assert(l);
	l = l->next;
	while (s && l) {
		disp_trans = list_append(disp_trans, l->data);
		s = s->next;
		l = l->next;
	}
}

static void
jump_split(split *s)
{
	int len, slen = 0, nlen;
	account *a;
	list *l;

	a = find_account(s->account);
	if (!a)
		return;

	curr_acct->selected = 0;
	curr_acct = a;
	curr_acct->selected = 1;

	trans_list = curr_acct->transactions;

	l = list_find(trans_list, curr_trans);
	assert(l);
	nlen = list_length(l->next);
	len = list_length(trans_list) - nlen;
	if (curr_trans->expanded)
		slen = list_length(curr_trans->splits);

	if (len + slen >= LINES - REG_HDR) {
		if (nlen < (LINES - REG_HDR) / 2)
			skip_trans = len + slen - (LINES - REG_HDR) + nlen;
		else
			skip_trans = len + slen - ((LINES - REG_HDR) / 2);
	} else {
		skip_trans = 0;
	}

	clear();
	redraw_screen();
}

static int
register_handle_key(int c)
{
	account *a;
	list *l;
	int expanded = 0;

	if (!curr_trans && c != 'q') {
		return 0;
	}

	switch (c) {
	case 'q':
		if (display_mode == JOURNAL)
			list_free(trans_list);
		trans_list = NULL;
		display_mode = ACCT_LIST;
		if (curr_trans && curr_trans->expanded) {
			curr_trans->expanded = 0;
			if (curr_trans->selected)
				curr_trans->selected = 0;
			else
				for (l = curr_trans->splits; l; l = l->next)
					((split *)l->data)->selected = 0;
		}

		a = curr_acct;
		while ((a = a->parent) != NULL)
			a->expanded = 1;
		recalc_skip_acct();

		clear();
		redraw_screen();
		break;

	case 10:	/* ^J */
	case 13:	/* ^M */
	case ' ':
		if (display_mode == JOURNAL)
			break;
		if (!curr_trans->selected)
			break;
		if (curr_trans->expanded) {
			unexpand_transaction();
		} else {
			expand_transaction();
		}
		redraw_screen();
		break;

	case 'J':
		if (display_mode == JOURNAL)
			break;
		l = curr_trans->splits;
		while (l) {
			split *s = l->data;
			l = l->next;
			if (!curr_trans->selected && !s->selected)
				continue;
			if (curr_trans->selected && !strcmp(s->account, curr_acct->id))
				continue;
			jump_split(s);
			break;
		}
		break;

	/* MOTION */
	case KEY_DOWN:
	case 'j':
		if (curr_trans->expanded) {
			list *splits = curr_trans->splits;
			if (curr_trans->selected) {
				split *s = splits->data;
				s->selected = 1;
				curr_trans->selected = 0;
				redraw_screen();
				break;
			} else {
				split *s = NULL;
				while (splits) {
					s = splits->data;
					splits = splits->next;
					if (!s->selected)
						continue;
					s->selected = 0;
					if (splits) {
						s = splits->data;
						s->selected = 1;
						break;
					}
				}
				if (splits) {
					redraw_screen();
					break;
				}
				l = list_find(trans_list, curr_trans);
				assert(l);
				if (!l->next) {
					assert(s);
					s->selected = 1;
					break;
				}
			}
			expanded = 1;
		}
		/* fallthrough */
	case 14:	/* ^N */
		if (curr_trans->expanded)
			expanded = 1;
		l = list_find(disp_trans, curr_trans);
		assert(l);
		if (l->next) {
			unexpand_transaction();
			curr_trans->selected = 0;
			curr_trans = l->next->data;
			curr_trans->selected = 1;
			if (expanded)
				expand_transaction();
			if (display_mode == JOURNAL && !l->next->next &&
				journal_post != list_length(curr_trans->splits)) {
				skip_trans += list_length(curr_trans->splits);
				skip_trans -= journal_post;
				redraw_screen();
				break;
			}
			redraw_screen();
		} else {
			l = list_find(trans_list, curr_trans);
			assert(l);
			if (l->next) {
				unexpand_transaction();
				curr_trans->selected = 0;
				curr_trans = l->next->data;
				curr_trans->selected = 1;
				if (expanded) {
					expand_transaction();
				} else {
					skip_trans++;
					if (display_mode == JOURNAL)
						skip_trans += list_length(curr_trans->splits);
				}
				redraw_screen();
			}
		}
		break;
	case KEY_UP:
	case 'k':
		if (curr_trans->expanded && !curr_trans->selected) {
			list *splits = curr_trans->splits;
			split *s = splits->data;
			if (s->selected) {
				s->selected = 0;
				curr_trans->selected = 1;
			} else {
				for (splits = splits->next; splits; splits = splits->next) {
					s = splits->data;
					if (!s->selected)
						continue;
					s->selected = 0;
					assert(splits->prev);
					s = splits->prev->data;
					s->selected = 1;
				}
			}
			redraw_screen();
			break;
		} else if (curr_trans->expanded) {
			l = list_find(trans_list, curr_trans);
			assert(l);
			if (!l->prev)
				break;
			expanded = 1;
		}
		/* fallthrough */
	case 16:	/* ^P */
		if (curr_trans->expanded)
			expanded = 1;
		unexpand_transaction();
		l = list_find(disp_trans, curr_trans);
		if (l && l->prev) {
			curr_trans->selected = 0;
			curr_trans = l->prev->data;
			curr_trans->selected = 1;
			if (expanded)
				expand_transaction();
			redraw_screen();
		} else {
			l = list_find(trans_list, curr_trans);
			assert(l);
			if (l->prev) {
				skip_trans--;
				curr_trans->selected = 0;
				curr_trans = l->prev->data;
				curr_trans->selected = 1;
				if (display_mode == JOURNAL) {
					skip_trans -= list_length(curr_trans->splits);
					skip_trans += journal_pre;
				}
				if (expanded)
					curr_trans->expanded = 1;
				redraw_screen();
			} else if (expanded) {
				expand_transaction();
				redraw_screen();
			}
		}
		break;
	case KEY_HOME:
	case 1:		/* ^A */
		if (curr_trans->expanded) {
			list *splits = curr_trans->splits;
			while (splits) {
				split *s = splits->data;
				if (s->selected) {
					s->selected = 0;
					break;
				}
				splits = splits->next;
			}
			if (curr_trans != trans_list->data) {
				unexpand_transaction();
			}
			expanded = 1;
		}
		curr_trans->selected = 0;
		curr_trans = trans_list->data;
		curr_trans->selected = 1;
		skip_trans = 0;
		if (expanded)
			curr_trans->expanded = 1;
		redraw_screen();
		break;
	case KEY_END:
	case 5:		/* ^E */
		if (curr_trans->expanded) {
			l = list_find(trans_list, curr_trans);
			assert(l);
			if (!l->next) {
				/* we're the last transaction */
				list *splits = curr_trans->splits;
				while (splits) {
					split *s = splits->data;
					splits = splits->next;
					if (splits) {
						s->selected = 0;
					} else {
						s->selected = 1;
					}
				}
				curr_trans->selected = 0;
				redraw_screen();
				break;
			}
			expanded = 1;
		}
		unexpand_transaction();
		init_trans();
		if (expanded) {
			curr_trans->expanded = 1;
			skip_trans += list_length(curr_trans->splits);
		}
		redraw_screen();
		break;

	default:
		break;
	}

	return 0;
}

void
display_run(char *filename)
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
			curs_set(0);
			recalc_skip_acct();
			recalc_skip_trans();
			clear();
			redraw_screen();
			continue;
		} else if (c == 12) {	/* ^L */
			clear();
			redraw_screen();
			continue;
		} else if (c == 17) {	/* ^Q */
			clear();
			refresh();
			endwin();
			list_free(disp_acct);
			list_free(disp_trans);
			return;
		} else if (c == 18) {	/* ^R */
			char *acctid, *transid = NULL, *journalid = NULL;

			acctid = strdup(curr_acct->id);
			if (display_mode != ACCT_LIST)
				transid = strdup(curr_trans->id);
			if (display_mode == JOURNAL) {
				if (journal_parent)
					journalid = strdup(journal_parent->id);
				list_free(trans_list);
			}
			curr_trans = NULL;
			journal_parent = NULL;
			trans_list = NULL;

			clear();
			refresh();
			endwin();

			free_all();

			gnucash_init(filename);

			if (!accounts) {
				fprintf(stderr, "no accounts!\n");
				return;
			}

			curr_acct = find_account(acctid);
			free(acctid);
			if (!curr_acct) {
				curr_acct = accounts->data;
				display_mode = REGISTER;
				if (transid)
					free(transid);
				if (journalid)
					free(journalid);
			} else {
				account *p = curr_acct->parent;
				curr_acct->selected = 1;
				while (p) {
					p->expanded = 1;
					p = p->parent;
				}
				recalc_skip_acct();
				if (journalid) {
					journal_parent = find_account(journalid);
					free(journalid);
					if (!journal_parent) {
						display_mode = ACCT_LIST;
						initscr();
						curs_set(0);
						redraw_screen();
						continue;
					}
				}
				init_trans_list();
				if (transid) {
					list *l = trans_list;
					while (l) {
						transaction *t = l->data;
						if (strcasecmp(t->id, transid)) {
							l = l->next;
							continue;
						}
						curr_trans = t;
						break;
					}
					free(transid);
				}
				if (!curr_trans) {
					init_trans();
				} else {
					curr_trans->selected = 1;
					recalc_skip_trans();
				}
			}

			initscr();
			curs_set(0);
			redraw_screen();
		} else if (c == 19) {	/* ^S */
			write_file(filename);
			continue;
		}


		switch (display_mode) {
		case ACCT_LIST:
			if (list_handle_key(c)) {
				clear();
				refresh();
				endwin();
				list_free(disp_acct);
				list_free(disp_trans);
				return;
			}
			break;
		case REGISTER:
		case JOURNAL:
			if (register_handle_key(c)) {
				clear();
				refresh();
				endwin();
				list_free(disp_acct);
				list_free(disp_trans);
				return;
			}
			break;
		}
	}
}

/* vim:set ts=4 sw=4 noet ai tw=80: */
