#ifndef tree_h
#define tree_h

typedef struct _tree {
	struct _tree *prev;
	struct _tree *next;
	void *data;
} tree;

/* returns > 1 if a > b */
typedef int (*tree_cmpfunc)(const void *, const void *);

extern tree *tree_new(void *);
extern unsigned int tree_length(tree *);
extern tree *tree_insert(tree *, void *, tree_cmpfunc);
extern tree *tree_find(tree *, void *, tree_cmpfunc);
extern tree *tree_remove(tree *, void *);
extern void tree_free(tree *);

#endif
