#include <time.h>
#include "list.h"

typedef struct {
	time_t	time;
	float	value;
} price;

typedef struct {
	char	*space;
	char	*id;
	char	*name;
	price	*quote;
} commodity;

typedef enum {
	ASSET = 1,
	BANK,
	CASH,
	CREDIT,
	EQUITY,
	EXPENSE,
	INCOME,
	LIABILITY,
	MUTUAL,
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
	float			quantity;
	commodity		*commodity;
	list			*transactions;

	/* display */
	int				expanded;
	int				selected;
} account;

typedef struct {
	char	*account;
	char	*memo;
	char	recstate;
	time_t	recdate;
	float	value;
	float	quantity;
	char	*action;
} split;

typedef struct {
	/* data */
	int		num;
	time_t	posted;
	time_t	entered;
	char	*description;
	list	*splits;

	/* display */
	int		selected;
} transaction;

extern list *commodities;
extern list *accounts;
extern list *transactions;

extern account *find_account(char *, list *);

extern void gnucash_init(char *);

extern void display_run();

/* vim:set ts=4 sw=4 noet ai tw=80: */
