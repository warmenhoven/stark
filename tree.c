#include <assert.h>
#include <stdlib.h>
#include "tree.h"

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
	tree *child, *parent, *grandparent = NULL;

	/* case 1: new node is root */
	if (!t) {
		s = tree_new(data);
		s->red = 0;
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

	assert(s);

	/* case 2: new node's parent is black */
	if (!s->parent->red)
		return t;

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

		if (!uncle) {
			/* uncle is black! */
			break;
		}

		if (uncle->red) {
			parent->red = 0;
			uncle->red = 0;
			grandparent->red = 1;

			child = grandparent;
			parent = child->parent;
			if (parent)
				grandparent = parent->parent;
		} else {
			break;
		}
	}
	if (!grandparent)
		return t;
	if (!parent || !parent->red) {
		if (!grandparent->parent)
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
			return parent;
		}
	}

	return t;
}

#if 0
tree *
tree_root(tree *t)
{
	while (t && t->parent)
		t = t->parent;
	return t;
}

tree *
tree_remove(tree *t, void *data, tree_cmpfunc f)
{
	tree *deleted = tree_find(t, data, f);
	tree *child, *parent, *sibling = NULL;

	if (!deleted)
		return t;

	if (t->data == data && !t->left && !t->right) {
		free(t);
		return NULL;
	}

	if (deleted->left && deleted->right) {
		/* we're not deleting this one, we're replacing it */
		tree *replaced = deleted;
		deleted = deleted->left;
		while (deleted->right)
			deleted = deleted->right;
		if (!deleted->red) {
			deleted = replaced->right;
			while (deleted->left)
				deleted = deleted->left;
		}
		replaced->data = deleted->data;
	}

	/* deleted now points at the node we're actually removing, and we're
	 * guaranteed it only has less than two children */

	child = deleted->left ? deleted->left : deleted->right;
	parent = deleted->parent;
	if (parent)
		sibling = parent->left == deleted ? parent->right : parent->left;

	if (deleted->red) {
		free(deleted);
		if (parent) {
			if (parent->right == sibling)
				parent->left = child;
			else
				parent->right = child;
			if (child)
				child->parent = parent;
			return t;
		} else {
			/* if we have no parent and no child then there is only one tree
			 * element. the checks at the beginning of this function should have
			 * handled that case. so child can't be NULL! */
			assert(child);
			child->parent = NULL;
			return child;
		}
	}

	/* we've taken care of the case where the deleted node is red; from here on
	 * we can assume it's black */

	free(deleted);

	if (child && child->red) {
		child->red = 0;
		if (parent) {
			if (parent->right == sibling)
				parent->left = child;
			else
				parent->right = child;
			child->parent = parent;
			return t;
		} else {
			child->parent = NULL;
			return child;
		}
	}

	/* so now the deleted node and its child (if any) are black */

	if (parent) {
		if (parent->right == sibling)
			parent->left = child;
		else
			parent->right = child;
		if (child)
			child->parent = parent;
	} else {
		/* case 1: child is the new root */
		assert(child);
		child->parent = NULL;
		return child;
	}

	assert(sibling);
	/* case 3: parent, sibling, and nephews are all black */
	while (!parent->red && !sibling->red &&
		   (!sibling->left || !sibling->left->red) &&
		   (!sibling->right || !sibling->right->red)) {
		sibling->red = 1;
		child = parent;
		/* case 1: child is the new root */
		if (!child->parent)
			return child;
		parent = child->parent;
		if (parent->left == child)
			sibling = parent->right;
		else
			sibling = parent->left;
	}

	/* case 2: sibling is red */
	if (sibling->red) {
		sibling->red = 0;
		parent->red = 1;
		sibling->parent = parent->parent;
		if (sibling->parent && sibling->parent->left == parent)
			sibling->parent->left = sibling;
		else if (sibling->parent)
			sibling->parent->right = sibling;
		parent->parent = sibling;
		if (parent->right == sibling) {
			parent->right = sibling->left;
			if (parent->right)
				parent->right->parent = parent;
			sibling->left = parent;
			sibling = parent->right;
		} else {
			parent->left = sibling->right;
			if (parent->left)
				parent->left->parent = parent;
			sibling->right = parent;
			sibling = parent->left;
		}
	}

	/* case 4: parent is red, sibling and nephews are black */
	if (parent->red && !sibling->red &&
		   (!sibling->left || !sibling->left->red) &&
		   (!sibling->right || !sibling->right->red)) {
		parent->red = 0;
		sibling->red = 1;
		return tree_root(parent);
	}

	/* case 5: sibling is black, closest nephew is red */
	if (parent->right == sibling &&
		sibling->left && sibling->left->red &&
		(!sibling->right || !sibling->right->red)) {
		parent->right = sibling->left;
		sibling->left->parent = parent;
		sibling->parent = sibling->left;
		sibling->left = sibling->left->right;
		if (sibling->left)
			sibling->left->parent = sibling;
		parent->right->right = sibling;
		sibling = parent->right;
	} else if (parent->left == sibling &&
			   sibling->right && sibling->right->red &&
			   (!sibling->left || !sibling->left->red)) {
		parent->left = sibling->right;
		sibling->right->parent = parent;
		sibling->parent = sibling->right;
		sibling->right = sibling->right->left;
		if (sibling->right)
			sibling->right->parent = sibling;
		parent->left->left = sibling;
		sibling = parent->left;
	}

	/* case 6: sibling is black, furthest nephew is red */
	if (parent->right == sibling &&
		sibling->right && sibling->right->red) {
		sibling->right->red = 0;
		sibling->red = parent->red;
		parent->red = 0;

		sibling->parent = parent->parent;
		if (sibling->parent && sibling->parent->left == parent)
			sibling->parent->left = sibling;
		else if (sibling->parent)
			sibling->parent->right = sibling;
		parent->parent = sibling;
		parent->right = sibling->left;
		if (parent->right)
			parent->right->parent = parent;
		sibling->left = parent;
		return tree_root(sibling);
	} else if (parent->left == sibling &&
			   sibling->left && sibling->left->red) {
		sibling->left->red = 0;
		sibling->red = parent->red;
		parent->red = 0;

		sibling->parent = parent->parent;
		if (sibling->parent && sibling->parent->left == parent)
			sibling->parent->left = sibling;
		else if (sibling->parent)
			sibling->parent->right = sibling;
		parent->parent = sibling;
		parent->left = sibling->right;
		if (parent->left)
			parent->left->parent = parent;
		sibling->right = parent;
		return tree_root(sibling);
	}

	assert(0);
}
#endif

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
