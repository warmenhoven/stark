#include <stdlib.h>
#include "tree.h"

static int inserts = 0;

tree *
tree_new(void *data)
{
	tree *t = calloc(sizeof (tree), 1);
	if (!t)
		return NULL;
	t->data = data;
	t->red = 1;
	return t;
}

tree *
tree_find(tree *t, void *data, tree_cmpfunc func)
{
	tree *s = t;

	while (s) {
		int ret = func(s->data, data);

		if (!ret)
			return s;
		else if (ret < 0)
			s = s->left;
		else
			s = s->right;
	}

	return NULL;
}

tree *
tree_insert(tree *t, void *data, tree_cmpfunc func)
{
	tree *s = t;
	tree *child, *parent, *grandparent;

	/* case 1: new node is root */
	if (!t) {
		s = tree_new(data);
		s->red = 0;
		inserts++;
		return s;
	}

	while (s) {
		int ret = func(s->data, data);

		if (!ret) {
			if (!s->left) {
				s->left = tree_new(data);
				s->left->parent = s;
				s = s->left;
				break;
			}
			if (!s->right) {
				s->right = tree_new(data);
				s->right->parent = s;
				s = s->right;
				break;
			}
			s = s->left;
		} else if (ret < 0) {
			if (!s->left) {
				s->left = tree_new(data);
				s->left->parent = s;
				s = s->left;
				break;
			}
			s = s->left;
		} else {
			if (!s->right) {
				s->right = tree_new(data);
				s->right->parent = s;
				s = s->right;
				break;
			}
			s = s->right;
		}
	}

	/* case 2: new node's parent is black */
	if (!s->parent->red) {
		inserts++;
		return t;
	}

	/* case 3: uncle is red */
	child = s;
	parent = child->parent;
	while (parent && parent->red) {
		tree *uncle;

		grandparent = parent->parent;
		if (grandparent->left == parent)
			uncle = grandparent->right;
		else
			uncle = grandparent->left;

		if (!uncle)
			break;

		if (uncle->red) {
			parent->red = 0;
			uncle->red = 0;
			grandparent->red = 1;

			child = grandparent;
			parent = child->parent;
		} else {
			break;
		}
	}
	if (!parent || !parent->red) {
		inserts++;
		grandparent->red = 0;
		return t;
	}

	grandparent = parent->parent;

	/* case 4: uncle is black, child and parent on opposite sides */
	if (grandparent->left == parent && parent->right == child) {
		grandparent->left = child;
		child->parent = grandparent;

		parent->right = child->left;
		if (parent->right)
			parent->right->parent = parent;

		child->left = parent;
		parent->parent = child;

		child = parent;
		parent = child->parent;
	} else if (grandparent->right == parent && parent->left == child) {
		grandparent->right = child;
		child->parent = grandparent;

		parent->left = child->right;
		if (parent->left)
			parent->left->parent = parent;

		child->right = parent;
		parent->parent = child;

		child = parent;
		parent = child->parent;
	}

	/* case 5: uncle is black, child and parent on same side */
	if (grandparent->left == parent && parent->left == child) {
		parent->parent = grandparent->parent;

		grandparent->left = parent->right;
		if (grandparent->left)
			grandparent->left->parent = grandparent;

		parent->right = grandparent;
		grandparent->parent = parent;

		grandparent->red = 1;
		parent->red = 0;

		if (parent->parent) {
			if (parent->parent->left == grandparent)
				parent->parent->left = parent;
			else
				parent->parent->right = parent;
		} else {
			inserts++;
			return parent;
		}
	} else if (grandparent->right == parent && parent->right == child) {
		parent->parent = grandparent->parent;

		grandparent->right = parent->left;
		if (grandparent->right)
			grandparent->right->parent = grandparent;

		parent->left = grandparent;
		grandparent->parent = parent;

		grandparent->red = 1;
		parent->red = 0;

		if (parent->parent) {
			if (parent->parent->left == grandparent)
				parent->parent->left = parent;
			else
				parent->parent->right = parent;
		} else {
			inserts++;
			return parent;
		}
	}

	inserts++;
	return t;
}

void
tree_free(tree *t)
{
	if (!t)
		return;
	tree_free(t->left);
	tree_free(t->right);
	free(t);
}

/* vim:set ts=4 sw=4 noet ai tw=80: */
