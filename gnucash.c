#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif
#include <expat.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "main.h"
#include "tree.h"
#include "xml.h"

static XML_Parser parser;

list *commodities = NULL;
list *accounts = NULL;
char *book_guid = NULL;

static tree *acct_tree = NULL;

static void
bail(const char *f, ...)
{
	va_list ap;

	va_start(ap, f);
	vfprintf(stderr, f, ap);
	va_end(ap);

	exit(1);
}

static time_t
gnucash_get_time(void *data)
{
	struct tm t;
	time_t ret;

	char sign;
	int h, m, s;

	char *pos;

	char *str = xml_get_data(data);

	pos = strptime(str, "%Y-%m-%d %H:%M:%S", &t);

	if (!pos)
		return 0;

	if (sscanf(pos, " %c%2d%2d", &sign, &h, &m) < 3)
		return 0;

	ret = timegm(&t);

	s = 60 * (60 * h + m);
	if (sign == '-')
		ret += s;
	else
		ret -= s;

	return ret;
}

static void
gnucash_get_value(char *s, value *v)
{
	v->val = strtol(s, NULL, 0);
	s = strchr(s, '/');
	s++;
	v->sig = strlen(s) - 1;
}

static commodity *
find_commodity(char *space, char *id)
{
	list *l = commodities;

	while (l) {
		commodity *c = l->data;
		l = l->next;

		if (!strcmp(c->space, space) && !strcmp(c->id, id))
			return c;
	}

	return NULL;
}

static void
gnucash_add_commodity(void *com)
{
	commodity *c;
	void *data;

	c = calloc(sizeof (commodity), 1);
	if (!c)
		bail("can't calloc!\n");

	data = xml_get_child(com, "cmdty:space");
	if (!data || !xml_get_data(data))
		bail("Commodity without space?\n");
	c->space = strdup(xml_get_data(data));

	data = xml_get_child(com, "cmdty:id");
	if (!data || !xml_get_data(data))
		bail("Commodity without id?\n");
	c->id = strdup(xml_get_data(data));

	data = xml_get_child(com, "cmdty:name");
	if (!data || !xml_get_data(data))
		bail("Commodity without name?\n");
	c->name = strdup(xml_get_data(data));

	data = xml_get_child(com, "cmdty:fraction");
	if (!data || !xml_get_data(data))
		bail("Commodity without fraction?\n");
	c->fraction = strtol(xml_get_data(data), NULL, 0);

	/* there's also a fraction element but we don't care */

	commodities = list_append(commodities, c);
}

static void
gnucash_add_price(void *pr)
{
	void *data, *sp, *id;
	commodity *c = NULL;
	price *p;

	p = calloc(sizeof (price), 1);
	if (!p)
		bail("can't calloc!\n");

	data = xml_get_child(pr, "price:commodity");
	if (!data)
		bail("No commodity information for price?\n");
	sp = xml_get_child(data, "cmdty:space");
	id = xml_get_child(data, "cmdty:id");

	c = find_commodity(xml_get_data(sp), xml_get_data(id));
	if (c) {
		if (c->quote) {
			void *tm = xml_get_child(pr, "price:time");
			p->time = gnucash_get_time(xml_get_child(tm, "ts:date"));
			if (c->quote->time >= p->time) {
				/* eh. silently ignore. */
				free(p);
				return;
			} else {
				free(c->quote);
				c->quote = p;
			}
		} else {
			c->quote = p;
		}
	} else {
		/* eh. silently ignore. */
		free(p);
		return;
	}

	data = xml_get_child(pr, "price:time");
	p->time = gnucash_get_time(xml_get_child(data, "ts:date"));
	data = xml_get_child(data, "ts:ns");
	if (data && xml_get_data(data))
		p->ns = strtol(xml_get_data(data), NULL, 0);

	data = xml_get_child(pr, "price:value");
	gnucash_get_value(xml_get_data(data), &p->value);

	data = xml_get_child(pr, "price:id");
	p->id = strdup(xml_get_data(data));
}

static void
gnucash_parse_pricedb(void *pricedb)
{
	list *children = xml_get_children(pricedb);

	while (children) {
		void *child = children->data;
		children = children->next;

		if (strcmp(xml_name(child), "price"))
			bail("Non-price (%s) in pricedb?\n", xml_name(child));

		gnucash_add_price(child);
	}
}

static act_type
gnucash_get_type(char *t)
{
	if (!strcmp(t, "ASSET"))
		return ASSET;
	if (!strcmp(t, "BANK"))
		return BANK;
	if (!strcmp(t, "CASH"))
		return CASH;
	if (!strcmp(t, "CREDIT"))
		return CREDIT;
	if (!strcmp(t, "EQUITY"))
		return EQUITY;
	if (!strcmp(t, "EXPENSE"))
		return EXPENSE;
	if (!strcmp(t, "INCOME"))
		return INCOME;
	if (!strcmp(t, "LIABILITY"))
		return LIABILITY;
	if (!strcmp(t, "MUTUAL"))
		return MUTUAL;
	if (!strcmp(t, "STOCK"))
		return STOCK;

	bail("Unknown account type %s\n", t);
	return 0;
}

static int
gnucash_acct_cmp(const void *one, const void *two)
{
	const account *a = one, *b = two;
	return memcmp(a->id, b->id, 32);
}

account *
find_account(char *guid)
{
	account a;
	tree *t;

	a.id = guid;

	t = tree_find(acct_tree, &a, gnucash_acct_cmp);
	if (t)
		return t->data;
	return NULL;
}

static void
get_reconcile_info(account *a, void *val)
{
	list *l;

	if (!xml_get_attrib(val, "type") ||
		strcmp(xml_get_attrib(val, "type"), "frame"))
		return;

	l = xml_get_children(val);
	while (l) {
		void *slot = l->data;
		void *k, *v;
		l = l->next;

		if (strcmp(xml_name(slot), "slot"))
			continue;

		k = xml_get_child(slot, "slot:key");
		v = xml_get_child(slot, "slot:value");

		if (!k || !xml_get_data(k) || !v)
			continue;

		if (!strcmp(xml_get_data(k), "last-date")) {
			if (!xml_get_data(v))
				continue;
			a->last_reconcile = strtoul(xml_get_data(v), NULL, 0);
		} else if (!strcmp(xml_get_data(k), "last-interval")) {
			list *s;

			if (!xml_get_attrib(v, "type") ||
				strcmp(xml_get_attrib(v, "type"), "frame"))
				continue;

			s = xml_get_children(v);
			while (s) {
				slot = s->data;
				s = s->next;

				if (strcmp(xml_name(slot), "slot"))
					continue;

				k = xml_get_child(slot, "slot:key");
				v = xml_get_child(slot, "slot:value");

				if (!k || !xml_get_data(k) || !v)
					continue;

				if (!strcmp(xml_get_data(k), "days")) {
					if (!xml_get_data(v))
						continue;
					a->reconcile_day = strtol(xml_get_data(v), NULL, 0);
				} else if (!strcmp(xml_get_data(k), "months")) {
					if (!xml_get_data(v))
						continue;
					a->reconcile_mon = strtol(xml_get_data(v), NULL, 0);
				}
			}
		}
	}
}

static void
gnucash_add_account(void *acc)
{
	account *a;
	void *data;

	a = calloc(sizeof (account), 1);
	if (!a)
		bail("can't calloc!\n");

	data = xml_get_child(acc, "act:name");
	if (!data || !xml_get_data(data))
		bail("Account with no name?\n");
	a->name = strdup(xml_get_data(data));

	data = xml_get_child(acc, "act:id");
	if (!data || !xml_get_data(data))
		bail("Account with no id?\n");
	a->id = strdup(xml_get_data(data));

	data = xml_get_child(acc, "act:type");
	if (!data || !xml_get_data(data))
		bail("Account with no type?\n");
	a->type = gnucash_get_type(xml_get_data(data));

	data = xml_get_child(acc, "act:description");
	if (data && xml_get_data(data))
		a->description = strdup(xml_get_data(data));

	data = xml_get_child(acc, "act:commodity-scu");
	if (data && xml_get_data(data))
		a->scu = strtol(xml_get_data(data), NULL, 0);

	data = xml_get_child(acc, "act:non-standard-scu");
	if (data)
		a->nonstdscu = 1;

	data = xml_get_child(acc, "act:parent");
	if (data) {
		a->parent = find_account(xml_get_data(data));
		if (!a->parent)
			bail("Couldn't find parent %s for %s\n", xml_get_data(data),
				 a->name);
		a->parent->subs = list_append(a->parent->subs, a);
	} else {
		accounts = list_append(accounts, a);
	}

	acct_tree = tree_insert(acct_tree, a, gnucash_acct_cmp);

	data = xml_get_child(acc, "act:commodity");
	if (data) {
		void *sp = xml_get_child(data, "cmdty:space");
		void *id = xml_get_child(data, "cmdty:id");
		commodity *c;

		if (!sp || !id)
			bail("Commodity used by account %s is bogus!\n", a->id);

		if ((c = find_commodity(xml_get_data(sp), xml_get_data(id))) != NULL)
			a->commodity = c;
	}

	data = xml_get_child(acc, "act:slots");
	if (data) {
		list *l = xml_get_children(data);
		while (l) {
			void *key, *val;
			data = l->data;
			l = l->next;
			key = xml_get_child(data, "slot:key");
			val = xml_get_child(data, "slot:value");
			if (!key || !xml_get_data(key) || !val)
				continue;
			if (!strcmp(xml_get_data(key), "placeholder")) {
				a->has_placeholder = 1;
				if (!xml_get_data(val))
					continue;
				if (!strcmp(xml_get_data(val), "true"))
					a->placeholder = 1;
			} else if (!strcmp(xml_get_data(key), "old-price-source")) {
				if (!xml_get_data(val))
					continue;
				a->oldsrc = strdup(xml_get_data(val));
			} else if (!strcmp(xml_get_data(key), "notes")) {
				a->has_notes = 1;
				if (!xml_get_data(val))
					continue;
				a->notes = strdup(xml_get_data(val));
			} else if (!strcmp(xml_get_data(key), "tax-related")) {
				a->tax_related = 1;
			} else if (!strcmp(xml_get_data(key), "reconcile-info")) {
				get_reconcile_info(a, val);
			} else if (!strcmp(xml_get_data(key), "last-num")) {
				if (!xml_get_data(val))
					continue;
				a->last_num = strtol(xml_get_data(val), NULL, 0);
			}
		}
	}
	/* we should probably handle other things as well... */
}

static int
gnucash_trans_cmp(const void *one, const void *two)
{
	const transaction *a = one, *b = two;

	if (a->posted != b->posted)
		return a->posted - b->posted;
	if (a->num != b->num)
		return a->num - b->num;
	return a->entered - b->entered;
}

static void
gnucash_add_split(transaction *t, void *sp)
{
	void *data;
	split *s;

	s = calloc(sizeof (split), 1);
	if (!s)
		bail("can't calloc!\n");

	t->splits = list_append(t->splits, s);

	data = xml_get_child(sp, "split:id");
	if (!data || !xml_get_data(data))
		bail("Split without guid?\n");
	s->id = strdup(xml_get_data(data));

	data = xml_get_child(sp, "split:memo");
	if (data && xml_get_data(data))
		s->memo = strdup(xml_get_data(data));

	data = xml_get_child(sp, "split:action");
	if (data && xml_get_data(data))
		s->action = strdup(xml_get_data(data));

	data = xml_get_child(sp, "split:reconciled-state");
	if (!data || !xml_get_data(data))
		bail("Split without reconciled state?\n");
	s->recstate = *xml_get_data(data);

	data = xml_get_child(sp, "split:reconcile-date");
	if (data && xml_get_data(data)) {
		if (!xml_get_child(data, "ts:date"))
			bail("reconcile-date without date?\n");
		s->recdate = gnucash_get_time(xml_get_child(data, "ts:date"));
		data = xml_get_child(data, "ts:ns");
		if (data && xml_get_data(data))
			s->ns = strtol(xml_get_data(data), NULL, 0);
	}

	data = xml_get_child(sp, "split:value");
	if (!data || !xml_get_data(data))
		bail("Split without reconciled value?\n");
	gnucash_get_value(xml_get_data(data), &s->value);

	data = xml_get_child(sp, "split:quantity");
	if (!data || !xml_get_data(data))
		bail("Split without reconciled quantity?\n");
	gnucash_get_value(xml_get_data(data), &s->quantity);

	data = xml_get_child(sp, "split:account");
	if (data && xml_get_data(data)) {
		account *a = find_account(xml_get_data(data));
		if (!a)
			bail("Split for non-account (%s)?\n", xml_get_data(data));
		if (!list_find(a->transactions, t)) {
			a->transactions =
				list_insert_sorted(a->transactions, t, gnucash_trans_cmp);
			value_add(&a->quantity, &s->quantity);
		}
		s->account = strdup(xml_get_data(data));
	} else {
		bail("Split without reconciled account?\n");
	}
}

static void
gnucash_add_transaction(void *trans)
{
	transaction *t;
	void *data;

	t = calloc(sizeof (transaction), 1);
	if (!t)
		bail("can't calloc!\n");

	data = xml_get_child(trans, "trn:id");
	if (!data || !xml_get_data(data))
		bail("Transaction without id?\n");
	t->id = strdup(xml_get_data(data));

	data = xml_get_child(trans, "trn:num");
	if (data && xml_get_data(data))
		t->num = strtol(xml_get_data(data), NULL, 0);

	data = xml_get_child(trans, "trn:date-posted");
	if (!data || !xml_get_data(data))
		bail("Transaction %s without date-posted?\n", t->id);
	if (!xml_get_child(data, "ts:date"))
		bail("Date-Posted (%s) without date?\n", t->id);
	t->posted = gnucash_get_time(xml_get_child(data, "ts:date"));
	data = xml_get_child(data, "ts:ns");
	if (data && xml_get_data(data))
		t->pns = strtol(xml_get_data(data), NULL, 0);

	data = xml_get_child(trans, "trn:date-entered");
	if (!data || !xml_get_data(data))
		bail("Transaction %s without date-entered?\n", t->id);
	if (!xml_get_child(data, "ts:date"))
		bail("Date-Entered (%s) without date?\n", t->id);
	t->entered = gnucash_get_time(xml_get_child(data, "ts:date"));
	data = xml_get_child(data, "ts:ns");
	if (data && xml_get_data(data))
		t->ens = strtol(xml_get_data(data), NULL, 0);

	data = xml_get_child(trans, "trn:description");
	if (data && xml_get_data(data))
		t->description = strdup(xml_get_data(data));

	data = xml_get_child(trans, "trn:splits");
	if (data) {
		list *splits = xml_get_children(data);
		if (!splits)
			bail("Transaction without splits?\n");
		while (splits) {
			if (strcmp(xml_name(splits->data), "trn:split"))
				bail("non-split (%s) in splits?\n", xml_name(splits->data));
			gnucash_add_split(t, splits->data);
			splits = splits->next;
		}
	} else {
		bail("Transaction without splits?\n");
	}
}

static void
gnucash_parse_book(void *book)
{
	list *children;

	if (strcmp(xml_get_attrib(book, "version"), "2.0.0"))
		bail("I don't understand book version %s\n",
			 xml_get_attrib(book, "version"));

	children = xml_get_children(book);
	if (!children)
		bail("No children in book?\n");

	while (children) {
		void *child = children->data;
		children = children->next;

		if (!strcmp(xml_name(child), "book:id")) {
			book_guid = strdup(xml_get_data(child));
		} else if (!strcmp(xml_name(child), "gnc:count-data")) {
			/* we don't really need to do anything here either. i guess we could
			 * use this for validation purposes, but eh. */
		} else if (!strcmp(xml_name(child), "gnc:commodity")) {
			if (strcmp(xml_get_attrib(child, "version"), "2.0.0"))
				bail("I don't understand commodity version %s\n",
					 xml_get_attrib(book, "version"));
			gnucash_add_commodity(child);
		} else if (!strcmp(xml_name(child), "gnc:pricedb")) {
			if (strcmp(xml_get_attrib(child, "version"), "1"))
				bail("I don't understand pricedb version %s\n",
					 xml_get_attrib(book, "version"));
			gnucash_parse_pricedb(child);
		} else if (!strcmp(xml_name(child), "gnc:account")) {
			if (strcmp(xml_get_attrib(child, "version"), "2.0.0"))
				bail("I don't understand account version %s\n",
					 xml_get_attrib(book, "version"));
			gnucash_add_account(child);
		} else if (!strcmp(xml_name(child), "gnc:transaction")) {
			if (strcmp(xml_get_attrib(child, "version"), "2.0.0"))
				bail("I don't understand transaction version %s\n",
					 xml_get_attrib(book, "version"));
			gnucash_add_transaction(child);
		} else {
			bail("I don't understand %s\n", xml_name(child));
		}
	}
}

static void
gnucash_process(void *top)
{
	list *child;

	if (strcmp(xml_name(top), "gnc-v2"))
		bail("I don't understand gnucash data (%s)\n", xml_name(top));

	child = xml_get_children(top);
	if (!child)
		bail("Empty gnucash data?\n");

	while (child) {
		if (!strcmp(xml_name(child->data), "gnc:count-data")) {
			int count;

			if (strcmp(xml_get_attrib(child->data, "cd:type"), "book"))
				bail("Didn't find book count\n");

			count = strtol(xml_get_data(child->data), NULL, 0);

			if (count != 1)
				bail("More than 1 book in file, can't deal!\n");
		} else if (!strcmp(xml_name(child->data), "gnc:book")) {
			gnucash_parse_book(child->data);
		} else {
			bail("Unrecognized data (%s)\n", xml_name(child->data));
		}

		child = child->next;
	}
}

static void
gnucash_start(void *data, const char *el, const char **attr)
{
	void **curr = (void **)data;
	int i;

	if (*curr)
		*curr = xml_child(*curr, el);
	else
		*curr = xml_new(el);

	for (i = 0; attr[i]; i +=2 )
		xml_attrib(*curr, attr[i], attr[i + 1]);
}

static void
gnucash_end(void *data, const char *el)
{
	void **curr = (void **)data;
	void *parent;

	if (!*curr)
		return;

	if (!(parent = xml_parent(*curr))) {
		gnucash_process(*curr);
		xml_free(*curr);
		*curr = NULL;
	} else if (!strcmp(xml_name(*curr), el)) {
		*curr = parent;
	}
}

static void
gnucash_chardata(void *data, const char *s, int len)
{
	void **curr = (void **)data;
	xml_data(*curr, s, len);
}

void
gnucash_init(char *filename)
{
	FILE *f = NULL;
	char line[4096];
	void *curr = NULL;

	if (!(parser = XML_ParserCreate(NULL)))
		bail("Unable to create parser\n");

	XML_SetUserData(parser, &curr);
	XML_SetElementHandler(parser, gnucash_start, gnucash_end);
	XML_SetCharacterDataHandler(parser, gnucash_chardata);

	if (!(f = fopen(filename, "r")))
		bail("Unable to open %s\n", filename);

	while (fgets(line, sizeof (line), f))
		if (!XML_Parse(parser, line, strlen(line), 0))
			bail("Unable to parse %s\n", filename);

	fclose(f);
	XML_ParserFree(parser);
}

/* vim:set ts=4 sw=4 noet ai tw=80: */
