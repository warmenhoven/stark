#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "main.h"

static unsigned long
power_ten(unsigned long y)
{
	unsigned long ret = 1;
	while (y--)
		ret *= 10;
	return ret;
}

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

int
trans_cmp_func(const void *one, const void *two)
{
	const transaction *a = one, *b = two;

	if (a->posted != b->posted)
		return a->posted - b->posted;
	if (a->num != b->num)
		return a->num - b->num;
	return a->entered - b->entered;
}

void
build_trans_list(list *l, list **t, int sorted)
{
	while (l) {
		account *a = l->data;
		list *s = a->transactions;
		while (s) {
			if (!list_find(*t, s->data)) {
				if (sorted)
					*t = list_insert_sorted(*t, s->data, trans_cmp_func);
				else
					*t = list_append(*t, s->data);
			}
			s = s->next;
		}
		build_trans_list(a->subs, t, sorted);
		l = l->next;
	}
}

static void
write_counts(FILE *f, int trans_count)
{
	fprintf(f, "<gnc:count-data cd:type=\"commodity\">%d</gnc:count-data>\n",
			list_length(commodities));

	fprintf(f, "<gnc:count-data cd:type=\"account\">%d</gnc:count-data>\n",
			1 + num_accounts(accounts));

	fprintf(f, "<gnc:count-data cd:type=\"transaction\">%d</gnc:count-data>\n",
			trans_count);
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

	len = strlen(str) * 6 + 1;

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
		} else if (*a == '"') {
			*b++ = '&';
			*b++ = 'q';
			*b++ = 'u';
			*b++ = 'o';
			*b++ = 't';
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
print_date(FILE *f, time_t t)
{
	char tmstr[64];
	struct tm *stm;
	time_t tz;

	stm = localtime(&t);
	strftime(tmstr, sizeof (tmstr), "%Y-%m-%d %H:%M:%S", stm);
	tz = __timezone - (stm->tm_isdst ? 3600 : 0);

	fprintf(f, "<ts:date>%s %c%02ld%02ld</ts:date>\n", tmstr,
			tz > 0 ? '-' : '+',
			tz > 0 ? tz / 3600 : -tz / 3600,
			tz > 0 ? (tz / 60) % 60 : (-tz / 60) % 60);
}

static void
write_price(FILE *f, commodity *c)
{
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
	fprintf(f, "      "); print_date(f, c->quote->time);
	if (c->quote->ns)
		fprintf(f, "      <ts:ns>%ld</ts:ns>\n", c->quote->ns);
	fprintf(f, "    </price:time>\n");
	fprintf(f, "    <price:source>Finance::Quote</price:source>\n");
	fprintf(f, "    <price:type>last</price:type>\n");
	fprintf(f, "    <price:value>%lld/%lu</price:value>\n",
			c->quote->value.val, power_ten(c->quote->value.sig));
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
	case CURRENCY:
		return "CURRENCY";
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
	case PAYABLE:
		return "PAYABLE";
	case RECEIVABLE:
		return "RECEIVABLE";
	case STOCK:
		return "STOCK";
	}

	return NULL;
}

/* XXX this isn't right at all yet */
static void
print_slots(FILE *f, account *a)
{
	if (!a->oldsrc &&
		!a->has_placeholder &&
		!a->has_notes &&
		!a->tax_related &&
		!a->last_reconcile &&
		!a->last_num)
		return;

	fprintf(f, "  <act:slots>\n");

	if (a->oldsrc) {
		fprintf(f, "    <slot>\n");
		fprintf(f, "      <slot:key>old-price-source</slot:key>\n");
		fprintf(f, "      <slot:value type=\"string\">%s</slot:value>\n",
				a->oldsrc);
		fprintf(f, "    </slot>\n");
	}

	if (a->has_placeholder) {
		fprintf(f, "    <slot>\n");
		fprintf(f, "      <slot:key>placeholder</slot:key>\n");
		fprintf(f, "      <slot:value type=\"string\">%s</slot:value>\n",
				a->placeholder ? "true" : "false");
		fprintf(f, "    </slot>\n");
	}

	if (a->has_notes) {
		fprintf(f, "    <slot>\n");
		fprintf(f, "      <slot:key>notes</slot:key>\n");
		fprintf(f, "      <slot:value type=\"string\">%s</slot:value>\n",
				a->notes ? a->notes : "");
		fprintf(f, "    </slot>\n");
	}

	if (a->last_num) {
		fprintf(f, "    <slot>\n");
		fprintf(f, "      <slot:key>last-num</slot:key>\n");
		fprintf(f, "      <slot:value type=\"string\">%d</slot:value>\n",
				a->last_num);
		fprintf(f, "    </slot>\n");
	}

	if (a->tax_related) {
		fprintf(f, "    <slot>\n");
		fprintf(f, "      <slot:key>tax-related</slot:key>\n");
		fprintf(f, "      <slot:value type=\"integer\">1</slot:value>\n");
		fprintf(f, "    </slot>\n");
	}

	if (a->last_reconcile) {
		fprintf(f, "    <slot>\n");
		fprintf(f, "      <slot:key>reconcile-info</slot:key>\n");
		fprintf(f, "      <slot:value type=\"frame\">\n");
		fprintf(f, "        <slot>\n");
		fprintf(f, "          <slot:key>last-date</slot:key>\n");
		fprintf(f, "          <slot:value type=\"integer\">%lu</slot:value>\n",
				a->last_reconcile);
		fprintf(f, "        </slot>\n");
		if (a->reconcile_mon || a->reconcile_day) {
			fprintf(f, "        <slot>\n");
			fprintf(f, "          <slot:key>last-interval</slot:key>\n");
			fprintf(f, "          <slot:value type=\"frame\">\n");
			fprintf(f, "            <slot>\n");
			fprintf(f, "              <slot:key>days</slot:key>\n");
			fprintf(f, "              <slot:value type=\"integer\">"
					   "%d</slot:value>\n", a->reconcile_day);
			fprintf(f, "            </slot>\n");
			fprintf(f, "            <slot>\n");
			fprintf(f, "              <slot:key>months</slot:key>\n");
			fprintf(f, "              <slot:value type=\"integer\">"
					   "%d</slot:value>\n", a->reconcile_mon);
			fprintf(f, "            </slot>\n");
			fprintf(f, "          </slot:value>\n");
			fprintf(f, "        </slot>\n");
		}
		fprintf(f, "      </slot:value>\n");
		fprintf(f, "    </slot>\n");
	}

	fprintf(f, "  </act:slots>\n");
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
		fprintf(f, "  <act:commodity-scu>%d</act:commodity-scu>\n", a->scu);
		if (a->nonstdscu)
			fprintf(f, "  <act:non-standard-scu/>\n");
		if (a->description)
			fprintf(f, "  <act:description>%s</act:description>\n",
					xml_str(a->description));
		print_slots(f, a);
		if (a->parent)
			fprintf(f, "  <act:parent type=\"guid\">%s</act:parent>\n",
					a->parent->id);
		fprintf(f, "</gnc:account>\n");

		write_accounts(f, a->subs);
	}
}

static void
write_split(FILE *f, split *s)
{
	account *a = find_account(s->account);
	if (!a)
		return;
	fprintf(f, "    <trn:split>\n");
	fprintf(f, "      <split:id type=\"guid\">%s</split:id>\n", s->id);
	if (s->memo)
		fprintf(f, "      <split:memo>%s</split:memo>\n", xml_str(s->memo));
	if (s->action)
		fprintf(f, "      <split:action>%s</split:action>\n",
				xml_str(s->action));
	fprintf(f, "      <split:reconciled-state>%c</split:reconciled-state>\n",
			s->recstate);
	if (s->recdate) {
		fprintf(f, "      <split:reconcile-date>\n");
		fprintf(f, "        "); print_date(f, s->recdate);
		if (s->ns)
			fprintf(f, "        <ts:ns>%ld</ts:ns>\n", s->ns);
		fprintf(f, "      </split:reconcile-date>\n");
	}
	fprintf(f, "      <split:value>%lld/%lu</split:value>\n",
			s->value.val, power_ten(s->value.sig));
	fprintf(f, "      <split:quantity>%lld/%lu</split:quantity>\n",
			s->quantity.val, power_ten(s->quantity.sig));
	fprintf(f, "      <split:account type=\"guid\">%s</split:account>\n",
			s->account);
	fprintf(f, "    </trn:split>\n");
}

static void
write_transactions(FILE *f, list *l)
{
	while (l) {
		transaction *t = l->data;
		list *splits;
		l = l->next;

		fprintf(f, "<gnc:transaction version=\"2.0.0\">\n");
		fprintf(f, "  <trn:id type=\"guid\">%s</trn:id>\n", t->id);
		fprintf(f, "  <trn:currency>\n");
		fprintf(f, "    <cmdty:space>ISO4217</cmdty:space>\n");
		fprintf(f, "    <cmdty:id>USD</cmdty:id>\n");
		fprintf(f, "  </trn:currency>\n");
		if (t->num)
			fprintf(f, "  <trn:num>%d</trn:num>\n", t->num);
		fprintf(f, "  <trn:date-posted>\n");
		fprintf(f, "    "); print_date(f, t->posted);
		if (t->pns)
			fprintf(f, "    <ts:ns>%ld</ts:ns>\n", t->pns);
		fprintf(f, "  </trn:date-posted>\n");
		fprintf(f, "  <trn:date-entered>\n");
		fprintf(f, "    "); print_date(f, t->entered);
		if (t->ens)
			fprintf(f, "    <ts:ns>%ld</ts:ns>\n", t->ens);
		fprintf(f, "  </trn:date-entered>\n");
		fprintf(f, "  <trn:description>%s</trn:description>\n",
				xml_str(t->description));
		fprintf(f, "  <trn:splits>\n");
		splits = t->splits;
		while (splits) {
			write_split(f, splits->data);
			splits = splits->next;
		}
		fprintf(f, "  </trn:splits>\n");
		fprintf(f, "</gnc:transaction>\n");
	}
}

static void
write_footer(FILE *f)
{
	fprintf(f, "</gnc:book>\n");
	fprintf(f, "</gnc-v2>\n");
	fprintf(f, "\n");
	fprintf(f, "<!-- Local variables: -->\n");
	fprintf(f, "<!-- mode: xml        -->\n");
	fprintf(f, "<!-- End:             -->\n");
}

void
write_file(const char *filename)
{
	FILE *f;
	list *l = NULL;

	f = fopen(filename, "w");
	if (!f)
		return;

	build_trans_list(accounts, &l, 0);

	write_header(f);
	write_counts(f, list_length(l));
	write_commodities(f);
	write_pricedb(f);
	write_accounts(f, accounts);
	write_transactions(f, l);
	write_footer(f);
	list_free(l);

	fclose(f);
}

/* vim:set ts=4 sw=4 noet ai tw=80: */
