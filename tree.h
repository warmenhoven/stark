#ifndef tree_h
#define tree_h

typedef struct _tree {
	struct _tree *left;
	struct _tree *right;
	struct _tree *parent;
	void *data;
	int red;
} tree;

/* returns > 1 if a > b */
typedef int (*tree_cmpfunc)(const void *, const void *);

extern tree *tree_new(void *);
extern tree *tree_insert(tree *, void *, tree_cmpfunc);
extern tree *tree_find(tree *, void *, tree_cmpfunc);
extern tree *tree_remove(tree *, void *, tree_cmpfunc);
extern tree *tree_root(tree *);
extern void tree_free(tree *);

#endif
