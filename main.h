#include <time.h>
#include "list.h"

typedef struct {
	char	*id;
	time_t	time;
	float	value;
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
	int				placeholder;

	/* display */
	int				expanded;
	int				selected;
} account;

typedef struct {
	/* data */
	char	*account;
	char	*memo;
	time_t	recdate;
	float	value;
	float	quantity;
	char	*action;
	char	recstate;
	char	pad[3];

	/* display */
	int		selected;
} split;

typedef struct {
	/* data */
	char	*id;
	int		num;
	time_t	posted;
	time_t	entered;
	char	*description;
	list	*splits;

	/* display */
	int		expanded;
	int		selected;
} transaction;

extern list *commodities;
extern list *accounts;
extern char *book_guid;

extern account *find_account(char *);

extern void gnucash_init(char *);

extern void display_run(void);

extern void write_file(const char *);

/* vim:set ts=4 sw=4 noet ai tw=80: */
