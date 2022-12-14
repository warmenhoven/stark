#include <stdint.h>
#include <time.h>
#include "list.h"

typedef struct {
	int64_t			val;
	unsigned long	sig;
} value;

typedef struct {
	char	*id;
	time_t	time;
	long	ns;
	value	value;
} price;

typedef struct {
	char	*space;
	char	*id;
	char	*name;
	int		fraction;
	price	*quote;
} commodity;

typedef enum {
	ASSET = 1,
	BANK,
	CASH,
	CREDIT,
	CURRENCY,
	EQUITY,
	EXPENSE,
	INCOME,
	LIABILITY,
	MUTUAL,
	PAYABLE,
	RECEIVABLE,
	STOCK
} act_type;

typedef struct _acct {
	/* data */
	struct _acct	*parent;
	list			*subs;
	char			*name;
	char			*description;
	char			*id;
	act_type		type;
	value			quantity;
	commodity		*commodity;
	list			*transactions;
	int				placeholder;

	/* stuff we don't care about (yet?), except for file */
	int				scu;
	int				nonstdscu;
	char			*oldsrc;
	int				has_placeholder;
	int				has_notes;
	char			*notes;
	int				tax_related;
	unsigned long	last_reconcile;
	int				reconcile_mon;
	int				reconcile_day;
	int				last_num;

	/* display */
	int				expanded;
	int				selected;

	/* conversion */
	int				sqlid;
} account;

typedef struct {
	/* data */
	char			*id;
	char			*account;
	char			*memo;
	time_t			recdate;
	long			ns;
	value			value;
	value			quantity;
	char			*action;
	unsigned char	recstate;
	char			pad[3];
	account			*acctptr;

	/* display */
	int				selected;

	/* conversion */
	int				sqlid;
} split;

typedef struct {
	/* data */
	char	*id;
	int		num;
	time_t	posted;
	long	pns;
	time_t	entered;
	long	ens;
	char	*description;
	list	*splits;

	/* display */
	int		expanded;
	int		selected;

	/* conversion */
	int		sqlid;
} transaction;

extern list *commodities;
extern list *transactions;
extern list *accounts;
extern char *book_guid;

extern account *find_account(char *);

extern void value_add(value *, value *);
extern void value_multiply(value *, value *, value *);
extern double value_to_double(value *);

extern void gnucash_init(char *);
extern void free_all(void);

extern void display_run(char *);

extern int trans_cmp_func(const void *, const void *);
extern void build_trans_list(list *, list **, int);
extern void write_file(const char *);

/* vim:set ts=4 sw=4 noet ai tw=80: */
