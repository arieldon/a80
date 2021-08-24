#ifndef LIST_H
#define LIST_H

#include <stdlib.h>

struct node {
	struct node *next;
	void *value;
};

struct list {
	struct node *head;
};

struct list *initlist(void);
struct node *append(struct list *list, void *value);
struct node *find(struct list *list, void *value, int (*cmp)(void *, void *));
void freelist(struct list *list);

#endif
