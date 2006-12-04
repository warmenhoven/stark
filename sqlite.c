#include "main.h"
#include <sqlite.h>

static sqlite *sqldb = NULL;

static void
sql_acct_list(list *accts)
{
	for (; accts; accts = accts->next) {
		account *a = accts->data;
		char query[16 * 1024];

		snprintf(query, sizeof (query),
			 "insert into accounts "
			 "(name, class, tax, def_commodity, reconcile_date) "
			 "values "
			 "('%s', '%d', '%d', '%s', '%lu')",
			 a->name,
			 a->type,
			 a->tax_related,
			 a->commodity ? a->commodity->id : "USD",
			 a->last_reconcile);

		sqlite_exec(sqldb, query, NULL, NULL, NULL);
		a->sqlid = sqlite_last_insert_rowid(sqldb);
		printf("added account \"%s\" as %d\n", a->name, a->sqlid);

		if (a->parent && a->parent->sqlid) {
			snprintf(query, sizeof (query),
				 "update accounts "
				 "set parent = '%d' "
				 "where id = '%d'",
				 a->parent->sqlid,
				 a->sqlid);
			sqlite_exec(sqldb, query, NULL, NULL, NULL);
		}

		if (a->description) {
			snprintf(query, sizeof (query),
				 "update accounts "
				 "set description = '%s' "
				 "where id = '%d'",
				 a->description,
				 a->sqlid);
			sqlite_exec(sqldb, query, NULL, NULL, NULL);
		}

		if (a->notes) {
			snprintf(query, sizeof (query),
				 "update accounts "
				 "set notes = '%s' "
				 "where id = '%d'",
				 a->notes,
				 a->sqlid);
			sqlite_exec(sqldb, query, NULL, NULL, NULL);
		}

		sql_acct_list(a->subs);
	}
}

static void
sql_trans_list(list *xacts)
{
	for (; xacts; xacts = xacts->next) {
		transaction *t = xacts->data;
		list *splits;
		char query[16 * 1024];
		int scount = 0;

		snprintf(query, sizeof (query),
			 "insert into transactions "
			 "(description, post_date) "
			 "values "
			 "('%s', '%d')",
			 t->description,
			 t->posted);
		sqlite_exec(sqldb, query, NULL, NULL, NULL);
		t->sqlid = sqlite_last_insert_rowid(sqldb);

		for (splits = t->splits; splits; splits = splits->next) {
			split *s = splits->data;

			snprintf(query, sizeof (query),
				 "insert into splits "
				 "(account, trans, commodity, quantity, reconciled) "
				 "values "
				 "('%d', '%d', '%s', '%.05f', '%d')",
				 s->acctptr->sqlid,
				 t->sqlid,
				 s->acctptr->commodity ? s->acctptr->commodity->id : "USD",
				 value_to_double(&s->quantity),
				 s->recstate ? 1 : 0);
			sqlite_exec(sqldb, query, NULL, NULL, NULL);
			s->sqlid = sqlite_last_insert_rowid(sqldb);

			if (s->memo) {
				snprintf(query, sizeof (query),
					 "update accounts "
					 "set memo = '%s' "
					 "where id = '%d'",
					 s->memo,
					 s->sqlid);
				sqlite_exec(sqldb, query, NULL, NULL, NULL);
			}
			scount++;
		}
		printf("added transaction with %d splits as %d\n", scount, t->sqlid);
	}
}

int
convert(char *gnucash_file, char *sqlite_file)
{
	gnucash_init(gnucash_file);

	sqldb = sqlite_open(sqlite_file, 0666, NULL);

	sql_acct_list(accounts);
	sql_trans_list(transactions);

	return 0;
}
