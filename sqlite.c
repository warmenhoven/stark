#include "main.h"
#include <sqlite3.h>
#include <stdio.h>

static sqlite3 *sqldb = NULL;

static void
sql_acct_list(list *accts)
{
	for (; accts; accts = accts->next) {
		account *a = accts->data;
		char query[16 * 1024];

		snprintf(query, sizeof (query),
				 "INSERT INTO accounts "
				 "(name, class, tax, def_commodity, reconcile) "
				 "VALUES "
				 "('%s', '%d', '%d', '%s', '%lu')",
				 a->name,
				 a->type,
				 a->tax_related,
				 a->commodity ? a->commodity->id : "USD",
				 a->last_reconcile);

		sqlite3_exec(sqldb, query, NULL, NULL, NULL);
		a->sqlid = sqlite3_last_insert_rowid(sqldb);
		//printf("added account \"%s\" as %d\n", a->name, a->sqlid);

		if (a->parent && a->parent->sqlid) {
			snprintf(query, sizeof (query),
					 "UPDATE accounts "
					 "SET parent = '%d' "
					 "WHERE id = '%d'",
					 a->parent->sqlid,
					 a->sqlid);
			sqlite3_exec(sqldb, query, NULL, NULL, NULL);
		}

		if (a->description) {
			snprintf(query, sizeof (query),
					 "UPDATE accounts "
					 "SET description = '%s' "
					 "WHERE id = '%d'",
					 a->description,
					 a->sqlid);
			sqlite3_exec(sqldb, query, NULL, NULL, NULL);
		}

		if (a->notes) {
			snprintf(query, sizeof (query),
					 "UPDATE accounts "
					 "SET notes = '%s' "
					 "WHERE id = '%d'",
					 a->notes,
					 a->sqlid);
			sqlite3_exec(sqldb, query, NULL, NULL, NULL);
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
				 "INSERT INTO transactions "
				 "(payee) "
				 "VALUES "
				 "('%s')",
				 t->description);
		sqlite3_exec(sqldb, query, NULL, NULL, NULL);
		t->sqlid = sqlite3_last_insert_rowid(sqldb);

		scount = list_length(t->splits);
		for (splits = t->splits; splits; splits = splits->next) {
			split *s = splits->data;

			snprintf(query, sizeof (query),
					 "INSERT INTO splits "
					 "(trans, post, payee, reconciled, commodity, quantity, account) "
					 "VALUES "
					 "('%d', '%lu', '%s', '%d', '%s', '%.05f', '%d')",
					 t->sqlid,
					 t->posted,
					 t->description,
					 s->recstate ? 2 : 0,
					 s->acctptr->commodity ? s->acctptr->commodity->id : "USD",
					 value_to_double(&s->quantity),
					 s->acctptr->sqlid
					);
			sqlite3_exec(sqldb, query, NULL, NULL, NULL);
			s->sqlid = sqlite3_last_insert_rowid(sqldb);

			if (t->num) {
				snprintf(query, sizeof (query),
						 "UPDATE splits "
						 "SET num = '%d' "
						 "WHERE id = '%d'",
						 t->num,
						 s->sqlid);
				sqlite3_exec(sqldb, query, NULL, NULL, NULL);
			}

			if (s->memo) {
				snprintf(query, sizeof (query),
						 "UPDATE splits "
						 "SET memo = '%s' "
						 "WHERE id = '%d'",
						 s->memo,
						 s->sqlid);
				sqlite3_exec(sqldb, query, NULL, NULL, NULL);
			}

			if (scount == 2) {
				split *o;
				if (splits->next) {
					o = splits->next->data;
				} else {
					o = splits->prev->data;
				}
				snprintf(query, sizeof (query),
						 "UPDATE splits "
						 "SET coding = '%d' "
						 "WHERE id = '%d'",
						 o->acctptr->sqlid,
						 s->sqlid);
				sqlite3_exec(sqldb, query, NULL, NULL, NULL);
			}
		}
		//printf("added transaction with %d splits as %d\n", scount, t->sqlid);
	}
}

int
convert(char *gnucash_file, char *sqlite_file)
{
	printf("opening gnucash file %s\n", gnucash_file);
	gnucash_init(gnucash_file);

	printf("opening sqlite file %s\n", sqlite_file);
	sqlite3_open(sqlite_file, &sqldb);
	if (sqldb == NULL) {
		fprintf(stderr, "unable to open %s\n", sqlite_file);
		return 1;
	}

	printf("building account table for %d accounts\n", list_length(accounts));
	sql_acct_list(accounts);
	printf("building transaction and split tables for %d transactions\n", list_length(transactions));
	sql_trans_list(transactions);
	printf("finished!\n");

	return 0;
}

/* vim:set ts=4 sw=4 noet ai tw=80: */
