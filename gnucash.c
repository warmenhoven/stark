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
#include "xml.h"

static XML_Parser parser;

list *commodities = NULL;
list *accounts = NULL;
list *transactions = NULL;

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

	ret = mktime(&t);

	s = 60 * h + m;
	if (sign == '-')
		ret += s;
	else
		ret -= s;

	return ret;
}

static float
gnucash_get_value(char *v)
{
	long n, d;
	n = strtol(v, NULL, 0);
	v = strchr(v, '/');
	v++;
	d = strtoul(v, NULL, 0);
	return ((float)n/(float)d);
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
	list *children = xml_get_children(com);
	commodity *c;

	if (!children)
		bail("No children in commodity?\n");

	c = calloc(sizeof (commodity), 1);
	if (!c)
		bail("can't calloc!\n");

	while (children) {
		void *data = children->data;
		children = children->next;

		if (!strcmp(xml_name(data), "cmdty:space"))
			c->space = strdup(xml_get_data(data));
		else if (!strcmp(xml_name(data), "cmdty:id"))
			c->id = strdup(xml_get_data(data));
		else if (!strcmp(xml_name(data), "cmdty:name"))
			c->name = strdup(xml_get_data(data));
	}

	commodities = list_append(commodities, c);
}

static void
gnucash_add_price(void *pr)
{
	list *children = xml_get_children(pr);
	commodity *c = NULL;
	price *p;

	if (!children)
		bail("No children in price?\n");

	p = calloc(sizeof (price), 1);
	if (!p)
		bail("can't calloc!\n");

	while (children) {
		void *data = children->data;
		children = children->next;

		if (!strcmp(xml_name(data), "price:id")) {
			/* we can skip this */
		} else if (!strcmp(xml_name(data), "price:commodity")) {
			void *sp = xml_get_child(data, "cmdty:space");
			void *id = xml_get_child(data, "cmdty:id");
			c = find_commodity(xml_get_data(sp), xml_get_data(id));
			if (c) {
				if (c->quote) {
					void *tm = xml_get_child(pr, "price:time");
					p->time = gnucash_get_time(xml_get_child(tm, "ts:date"));
					if (c->quote->time >= p->time) {
						/* eh. silently ignore.
						fprintf(stderr, "duplicate quote %s\n", c->id);
						*/
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
				/* eh. silently ignore.
				fprintf(stderr, "couldn't find commodity %s\n", xml_get_data(id));
				*/
				free(p);
				return;
			}
		} else if (!strcmp(xml_name(data), "price:time")) {
			p->time = gnucash_get_time(xml_get_child(data, "ts:date"));
		} else if (!strcmp(xml_name(data), "price:value")) {
			p->value = gnucash_get_value(xml_get_data(data));
		}
	}

	/*
	printf("%s had price %.02f at %s", c->name, p->value, ctime(&p->time));
	*/
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

account *
find_account(char *guid, list *l)
{
	while (l) {
		account *a = l->data;
		l = l->next;

		if (!strcmp(guid, a->id))
			return a;

		a = find_account(guid, a->subs);
		if (a)
			return a;
	}

	return NULL;
}

static void
gnucash_add_account(void *acc)
{
	list *children = xml_get_children(acc);
	account *a;

	if (!children)
		bail("No children in account?\n");

	a = calloc(sizeof (account), 1);
	if (!a)
		bail("can't calloc!\n");

	while (children) {
		void *data = children->data;
		children = children->next;

		if (!strcmp(xml_name(data), "act:name")) {
			a->name = strdup(xml_get_data(data));
		} else if (!strcmp(xml_name(data), "act:id")) {
			a->id = strdup(xml_get_data(data));
		} else if (!strcmp(xml_name(data), "act:type")) {
			a->type = gnucash_get_type(xml_get_data(data));
		} else if (!strcmp(xml_name(data), "act:commodity")) {
			void *sp = xml_get_child(data, "cmdty:space");
			void *id = xml_get_child(data, "cmdty:id");
			commodity *c = find_commodity(xml_get_data(sp), xml_get_data(id));
			if (c) {
				a->commodity = c;
			} else {
				/*
				printf("Unable to find commodity %s/%s, will assume USD\n",
						xml_get_data(sp), xml_get_data(id));
				*/
			}
		} else if (!strcmp(xml_name(data), "act:commodity-scu")) {
			/* we don't care about this */
		} else if (!strcmp(xml_name(data), "act:non-standard-scu")) {
			/* why does gnucash do it this way? */
		} else if (!strcmp(xml_name(data), "act:description")) {
			a->description = strdup(xml_get_data(data));
		} else if (!strcmp(xml_name(data), "act:slots")) {
			/* XXX: this will probably become necessary later */
		} else if (!strcmp(xml_name(data), "act:parent")) {
			a->parent = find_account(xml_get_data(data), accounts);
			if (!a->parent)
				bail("Couldn't find parent %s for %s\n", xml_get_data(data),
					 a->name);
			a->parent->subs = list_append(a->parent->subs, a);
		} else {
			bail("Unknown data %s in account\n", xml_name(data));
		}
	}

	if (!a->parent)
		accounts = list_append(accounts, a);
}

static void
gnucash_print_accounts(list *l, char *prefix)
{
	while (l) {
		char newprefix[256];
		account *a = l->data;
		l = l->next;

		printf("%s-%s", prefix, a->name);
		if (a->commodity)
			printf(" (%s)", a->commodity->name);
		else
			printf(" (USD)");
		printf(" (%u transactions)", list_length(a->transactions));
		printf(" (%.03f %s)", a->quantity,
				a->commodity ? a->commodity->id : "USD");
		printf("\n");
		snprintf(newprefix, sizeof(newprefix), "%s |", prefix);
		gnucash_print_accounts(a->subs, newprefix);
	}
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
	list *children = xml_get_children(sp);
	split *s;

	if (!children)
		bail("No children in splits?\n");

	s = calloc(sizeof (split), 1);
	if (!s)
		bail("can't calloc!\n");

	while (children) {
		void *data = children->data;
		children = children->next;

		if (!strcmp(xml_name(data), "split:id")) {
			/* honestly, does everything need a guid */
		} else if (!strcmp(xml_name(data), "split:memo")) {
			s->memo = strdup(xml_get_data(data));
		} else if (!strcmp(xml_name(data), "split:reconciled-state")) {
			s->recstate = *xml_get_data(data);
		} else if (!strcmp(xml_name(data), "split:reconcile-date")) {
			s->recdate = gnucash_get_time(xml_get_child(data, "ts:date"));
		} else if (!strcmp(xml_name(data), "split:value")) {
			s->value = gnucash_get_value(xml_get_data(data));
		} else if (!strcmp(xml_name(data), "split:quantity")) {
			s->quantity = gnucash_get_value(xml_get_data(data));
		} else if (!strcmp(xml_name(data), "split:account")) {
			account *a = find_account(xml_get_data(data), accounts);
			if (!a)
				bail("Split for non-account (%s)?\n", xml_get_data(data));
			if (!list_find(a->transactions, t)) {
				a->transactions =
					list_insert_sorted(a->transactions, t, gnucash_trans_cmp);
				a->quantity += s->quantity;
			}
			s->account = strdup(xml_get_data(data));
		} else if (!strcmp(xml_name(data), "split:action")) {
			s->action = strdup(xml_get_data(data));
		} else {
			bail("Unknown data %s in split\n", xml_name(data));
		}
	}

	t->splits = list_append(t->splits, s);
}

static void
gnucash_add_transaction(void *trans)
{
	list *children = xml_get_children(trans);
	transaction *t;

	if (!children)
		bail("No children in transaction?\n");

	t = calloc(sizeof (transaction), 1);
	if (!t)
		bail("can't calloc!\n");

	while (children) {
		void *data = children->data;
		children = children->next;

		if (!strcmp(xml_name(data), "trn:id")) {
			/* is this really necessary? */
		} else if (!strcmp(xml_name(data), "trn:currency")) {
		} else if (!strcmp(xml_name(data), "trn:num")) {
			t->num = strtol(xml_get_data(data), NULL, 0);
		} else if (!strcmp(xml_name(data), "trn:date-posted")) {
			t->posted = gnucash_get_time(xml_get_child(data, "ts:date"));
		} else if (!strcmp(xml_name(data), "trn:date-entered")) {
			t->entered = gnucash_get_time(xml_get_child(data, "ts:date"));
		} else if (!strcmp(xml_name(data), "trn:description")) {
			if (xml_get_data(data))
				t->description = strdup(xml_get_data(data));
		} else if (!strcmp(xml_name(data), "trn:splits")) {
			list *splits = xml_get_children(data);
			while (splits) {
				if (strcmp(xml_name(splits->data), "trn:split"))
					bail("non-split (%s) in splits?\n", xml_name(splits->data));
				gnucash_add_split(t, splits->data);
				splits = splits->next;
			}
		} else {
			bail("Unknown data %s in transaction\n", xml_name(data));
		}
	}

	transactions = list_append(transactions, t);
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
			/* we don't really need to do anything here */
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

	/*
	printf("%u transactions\n", list_length(transactions));
	gnucash_print_accounts(accounts, "");
	*/
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
