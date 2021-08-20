#include "list.h"

struct list *
initlist(void)
{
	struct list *list = malloc(sizeof(struct list));
	if (list == NULL) {
		return NULL;
	}

	if ((list->head = malloc(sizeof(struct node))) == NULL) {
		free(list);
		return NULL;
	}
	if ((list->tail = malloc(sizeof(struct node))) == NULL) {
		free(list->head);
		free(list);
		return NULL;
	}

	list->head->value = NULL;
	list->head->next = list->tail;

	list->tail->value = NULL;
	list->tail->next = NULL;

	return list;
}

struct node *
push(struct list *list, void *value)
{
	struct node *node = malloc(sizeof(struct node));
	if (node == NULL) {
		return NULL;
	}

	node->value = value;
	node->next = NULL;

	list->tail->next = node;
	list->tail = node;

	return node;
}

struct node *
find(struct list *list, void *value, int (*cmp)(void *, void *))
{
	struct node *n = list->head;
	while (n != NULL) {
		if ((*cmp)(n->value, value)) {
			return n;
		}
		n = n->next;
	}
	return NULL;
}

void
freelist(struct list *list)
{
	struct node *n = list->head;
	while (n != NULL) {
		struct node *m = n;
		n = n->next;
		free(m->value);
		free(m);
	}
	free(list);
}
