#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "main.h"

static void
write_header(FILE *f)
{
	fprintf(f, "<?xml version=\"1.0\"?>\n");
	fprintf(f, "<gnc-v2>\n");
	fprintf(f, "<gnc:count-data cd:type=\"book\">1</gnc:count-data>\n");
	fprintf(f, "<gnc:book version=\"2.0.0\">\n");
	fprintf(f, "<book:id type=\"guid\">%s</book:id>\n", book_guid);
}

static int
num_accounts(list *l)
{
	int ret = 0;

	while (l) {
		account *a = l->data;
		l = l->next;
		ret += num_accounts(a->subs);
		ret++;
	}

	return ret;
}

static void
build_trans_list(list *l, list **t)
{
	while (l) {
		account *a = l->data;
		list *s = a->transactions;
		while (s) {
			if (!list_find(*t, s->data))
				*t = list_append(*t, s->data);
			s = s->next;
		}
		build_trans_list(a->subs, t);
		l = l->next;
	}
}

static void
write_counts(FILE *f)
{
	list *l = NULL;

	fprintf(f, "<gnc:count-data cd:type=\"commodity\">%d</gnc:count-data>\n",
			list_length(commodities));

	fprintf(f, "<gnc:count-data cd:type=\"account\">%d</gnc:count-data>\n",
			1 + num_accounts(accounts));

	build_trans_list(accounts, &l);
	fprintf(f, "<gnc:count-data cd:type=\"transaction\">%d</gnc:count-data>\n",
			list_length(l));
	list_free(l);
}

static char *
xml_str(char *str)
{
	static char *ret = NULL;
	char *a, *b;
	int len;

	if (ret) {
		free(ret);
		ret = NULL;
	}

	len = strlen(str) * 5 + 1;

	ret = malloc(len);
	if (!ret)
		return NULL;

	a = str;
	b = ret;

	while (*a) {
		if (*a == '<') {
			*b++ = '&';
			*b++ = 'l';
			*b++ = 't';
			*b++ = ';';
		} else if (*a == '>') {
			*b++ = '&';
			*b++ = 'g';
			*b++ = 't';
			*b++ = ';';
		} else if (*a == '&') {
			*b++ = '&';
			*b++ = 'a';
			*b++ = 'm';
			*b++ = 'p';
			*b++ = ';';
		} else {
			*b++ = *a;
		}
		*a++;
	}

	*b = 0;

	return ret;
}

static void
write_commodities(FILE *f)
{
	list *l = commodities;
	while (l) {
		commodity *c = l->data;
		l = l->next;

		fprintf(f, "<gnc:commodity version=\"2.0.0\">\n");
		fprintf(f, "  <cmdty:space>%s</cmdty:space>\n", xml_str(c->space));
		fprintf(f, "  <cmdty:id>%s</cmdty:id>\n", xml_str(c->id));
		fprintf(f, "  <cmdty:name>%s</cmdty:name>\n", xml_str(c->name));
		fprintf(f, "  <cmdty:fraction>%d</cmdty:fraction>\n", c->fraction);
		fprintf(f, "</gnc:commodity>\n");
	}
}

static void
write_price(FILE *f, commodity *c)
{
	char tmstr[64];
	struct tm *stm;
	time_t tz;

	fprintf(f, "  <price>\n");
	fprintf(f, "    <price:id type=\"guid\">%s</price:id>\n", c->quote->id);
	fprintf(f, "    <price:commodity>\n");
	fprintf(f, "      <cmdty:space>%s</cmdty:space>\n", xml_str(c->space));
	fprintf(f, "      <cmdty:id>%s</cmdty:id>\n", xml_str(c->id));
	fprintf(f, "    </price:commodity>\n");
	fprintf(f, "    <price:currency>\n");
	fprintf(f, "      <cmdty:space>ISO4217</cmdty:space>\n");
	fprintf(f, "      <cmdty:id>USD</cmdty:id>\n");
	fprintf(f, "    </price:currency>\n");
	fprintf(f, "    <price:time>\n");
	tz = c->quote->time + __timezone - (stm->tm_isdst ? 3600 : 0);
	stm = localtime(&tz);
	tz -= c->quote->time;
	strftime(tmstr, sizeof (tmstr), "%Y-%m-%d %H:%M:%S", stm);
	fprintf(f, "      <ts:date>%s %c%02ld%02ld</ts:date>\n", tmstr,
			tz > 0 ? '-' : '+',
			tz > 0 ? tz / 3600 : -tz / 3600,
			tz > 0 ? (tz / 60) % 60 : (-tz / 60) % 60);
	fprintf(f, "    </price:time>\n");
	fprintf(f, "    <price:source>Finance::Quote</price:source>\n");
	fprintf(f, "    <price:type>last</price:type>\n");
	fprintf(f, "    <price:value>%d/10000000</price:value>\n",
			(int)((c->quote->value + 0.009) * 100) * 100000);
	fprintf(f, "  </price>\n");
}

static void
write_pricedb(FILE *f)
{
	list *l = commodities;

	fprintf(f, "<gnc:pricedb version=\"1\">\n");

	while (l) {
		commodity *c = l->data;
		l = l->next;
		if (c->quote)
			write_price(f, c);
	}

	fprintf(f, "</gnc:pricedb>\n");
}

static const char *
account_type_string(act_type t)
{
	switch (t) {
	case ASSET:
		return "ASSET";
	case BANK:
		return "BANK";
	case CASH:
		return "CASH";
	case CREDIT:
		return "CREDIT";
	case EQUITY:
		return "EQUITY";
	case EXPENSE:
		return "EXPENSE";
	case INCOME:
		return "INCOME";
	case LIABILITY:
		return "LIABILITY";
	case MUTUAL:
		return "MUTUAL";
	case STOCK:
		return "STOCK";
	}

	return NULL;
}

static void
write_accounts(FILE *f, list *l)
{
	while (l) {
		account *a = l->data;
		l = l->next;

		fprintf(f, "<gnc:account version=\"2.0.0\">\n");
		fprintf(f, "  <act:name>%s</act:name>\n", xml_str(a->name));
		fprintf(f, "  <act:id type=\"guid\">%s</act:id>\n", a->id);
		fprintf(f, "  <act:type>%s</act:type>\n", account_type_string(a->type));
		if (a->commodity) {
			fprintf(f, "  <act:commodity>\n");
			fprintf(f, "    <cmdty:space>%s</cmdty:space>\n",
					xml_str(a->commodity->space));
			fprintf(f, "    <cmdty:id>%s</cmdty:id>\n",
					xml_str(a->commodity->id));
			fprintf(f, "  </act:commodity>\n");
		} else {
			fprintf(f, "  <act:commodity>\n");
			fprintf(f, "    <cmdty:space>ISO4217</cmdty:space>\n");
			fprintf(f, "    <cmdty:id>USD</cmdty:id>\n");
			fprintf(f, "  </act:commodity>\n");
		}
		fprintf(f, "  <act:commodity-scu>100</act:commodity-scu>\n");
		if (a->description)
			fprintf(f, "  <act:description>%s</act:description>\n",
					xml_str(a->description));
		fprintf(f, "  <act:slots>\n");
		fprintf(f, "    <slot>\n");
		fprintf(f, "      <slot:key>placeholder</slot:key>\n");
		fprintf(f, "      <slot:value type=\"string\">%s</slot:value>\n",
				a->placeholder ? "true" : "false");
		fprintf(f, "    </slot>\n");
		fprintf(f, "    <slot>\n");
		fprintf(f, "      <slot:key>notes</slot:key>\n");
		fprintf(f, "      <slot:value type=\"string\"></slot:value>\n");
		fprintf(f, "    </slot>\n");
		fprintf(f, "  </act:slots>\n");
		if (a->parent)
			fprintf(f, "  <act:parent type=\"guid\">%s</act:parent>\n",
					a->parent->id);
		fprintf(f, "</gnc:account>\n");

		write_accounts(f, a->subs);
	}
}

void
write_file(const char *filename)
{
	FILE *f;

	f = fopen(filename, "w");
	if (!f)
		return;

	write_header(f);
	write_counts(f);
	write_commodities(f);
	write_pricedb(f);
	write_accounts(f, accounts);

	fclose(f);
}

/* vim:set ts=4 sw=4 noet ai tw=80: */
