#ifndef list_h
#define list_h

typedef struct _list {
	struct _list *prev;
	struct _list *next;
	void *data;
} list;

/* returns > 1 if a > b */
typedef int (*list_cmpfunc)(const void *, const void *);

extern list *list_new(void *);
extern list *list_copy(list *);
extern unsigned int list_length(list *);
extern list *list_nth(list *, int);
extern list *list_find(list *, void *);
extern list *list_append(list *, void *);
extern list *list_prepend(list *, void *);
extern list *list_insert_sorted(list *, void *, list_cmpfunc);
extern list *list_remove(list *, void *);
extern void list_free(list *);

#endif
