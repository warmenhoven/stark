#include <stdlib.h>
#include "tree.h"

tree *
tree_new(void *data)
{
	tree *t = calloc(sizeof (tree), 1);
	t->data = data;
	return t;
}

tree *
tree_find(tree *l, void *data, tree_cmpfunc func)
{
	tree *s = l;

	while (s) {
		int ret = func(s->data, data);

		if (!ret)
			return s;
		else if (ret < 0)
			s = s->prev;
		else
			s = s->next;
	}

	return NULL;
}

tree *
tree_insert(tree *l, void *data, tree_cmpfunc func)
{
	tree *s = l;

	if (!l)
		return tree_new(data);

	while (s) {
		int ret = func(s->data, data);

		if (!ret) {
			if (!s->prev) {
				s->prev = tree_new(data);
				break;
			}
			if (!s->next) {
				s->next = tree_new(data);
				break;
			}
			s = s->prev;
		} else if (ret < 0) {
			if (!s->prev) {
				s->prev = tree_new(data);
				break;
			}
			s = s->prev;
		} else {
			if (!s->next) {
				s->next = tree_new(data);
				break;
			}
			s = s->next;
		}
	}

	return l;
}

void
tree_free(tree *t)
{
	if (!t)
		return NULL;
	tree_free(t->prev);
	tree_free(t->next);
	free(t);
}

/* vim:set ts=4 sw=4 noet ai tw=80: */
