#ifndef LIST_H
#define LIST_H

#include <stdlib.h>

struct node {
	void *value;
	struct node *next;
};

struct list {
	struct node *head;
	struct node *tail;
};

struct list *initlist(void);
struct node *push(struct list *list, void *value);
void freelist(struct list *list);

#endif
