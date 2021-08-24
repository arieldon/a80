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

	list->head->next = NULL;
	list->head->value = NULL;

	return list;
}

struct node *
append(struct list *list, void *value)
{
	struct node *n = malloc(sizeof(struct node));
	if (n == NULL) {
		return NULL;
	}
	n->value = value;
	n->next = NULL;

	struct node *m = list->head;
	while (m->next != NULL) {
		m = m->next;
	}
	m->next = n;

	return n;
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
