#include <stdlib.h>
#include "list.h"

list *
list_new(void *data)
{
	list *l = malloc(sizeof(list));
	l->prev = NULL;
	l->next = NULL;
	l->data = data;
	return l;
}

unsigned int
list_length(list *l)
{
	unsigned int c = 0;

	while (l) {
		l = l->next;
		c++;
	}

	return c;
}

list *
list_nth(list *l, int n)
{
	while (l && n) {
		l = l->next;
		n--;
	}
	if (l) return l;
	return NULL;
}

list *
list_find(list *l, void *data)
{
	while (l) {
		if (l->data == data)
			return l;
		l = l->next;
	}
	return NULL;
}

list *
list_append(list *l, void *data)
{
	list *s = l;

	if (!s) return list_new(data);

	while (s->next) s = s->next;
	s->next = list_new(data);
	s->next->prev = s;

	return l;
}

list *
list_insert_sorted(list *l, void *data, cmpfunc func)
{
	list *s = l, *t;

	/* search for the one it should go before */
	while (s && func(data, s->data) > 0) s = s->next;

	if (!s)
		return list_append(l, data);

	if (!s->prev)
		return list_prepend(l, data);

	t = s->prev;
	t->next = list_new(data);
	t->next->prev = t;
	t->next->next = s;
	s->prev = t->next;

	return l;
}

list *
list_prepend(list *l, void *data)
{
	list *s = list_new(data);
	s->next = l;
	s->next->prev = s;
	return s;
}

list *
list_remove(list *l, void *data)
{
	list *s = l, *p = NULL;

	if (!s) return NULL;
	if (s->data == data) {
		p = s->next;
		free(s);
		if (p) p->prev = NULL;
		return p;
	}
	while (s->next) {
		p = s;
		s = s->next;
		if (s->data == data) {
			p->next = s->next;
			free(s);
			if (p->next) p->next->prev = p;
			return l;
		}
	}
	return l;
}

void
list_free(list *l)
{
	while (l) {
		list *s = l;
		l = l->next;
		free(s);
	}
}

/* vim:set ts=4 sw=4 noet ai tw=80: */
