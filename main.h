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
	struct _acct	*parent;
	list			*subs;
	char			*name;
	char			*description;
	char			*id;
	act_type		type;
	float			quantity;
	commodity		*commodity;
	list			*transactions;
} account;

typedef struct {
	char	*memo;
	char	recstate;
	time_t	recdate;
	float	value;
	float	quantity;
	char	*action;
} split;

typedef struct {
	int		num;
	time_t	posted;
	time_t	entered;
	char	*description;
	list	*splits;
} transaction;

extern list *commodities;
extern list *accounts;
extern list *transactions;

extern void gnucash_init(char *);

extern void display_init();
extern void display_run();
extern void display_end();

/* vim:set ts=4 sw=4 noet ai tw=80: */
