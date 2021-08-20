#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

static unsigned char output[BUFSIZ];
static unsigned short addr;
static size_t lineno;
static int pass;

/* FORMAT [label:] [op [arg1[, arg2]]] [; comment] */
static char *label;
static char *op;
static char *arg1;
static char *arg2;
static char *comment;

static char *
strip(char *s)
{
	if (s == NULL) return NULL;
	if (s[0] == '\0') return "";

	char *t = strchr(s, '\0') - 1;
	while (t > s && isspace(*t)) --t;
	t[1] = '\0';

	return s + strspn(s, " \f\n\r\t\v");
}

static void
parse(char *line)
{
	label = NULL;
	op = NULL;
	arg1 = NULL;
	arg2 = NULL;
	comment = NULL;

	line = strip(line);
	if (line == NULL || line[0] == '\0') return;

	char *end = line + strlen(line);

	comment = memchr(line, ';', end - line);
	if (comment) {
		if (line[0] == ';') {
			return;
		}

		end = comment;
		*end = '\0';
		comment = strip(comment + 1);
	}

	arg2 = memchr(line, ',', end - line);
	if (arg2) {
		end = arg2;
		*end = '\0';
		arg2 = strip(arg2 + 1);
	}

	arg1 = strrchr(strip(line), ' ');
	if (arg1) {
		end = arg1;
		*end = '\0';
		arg1 = strip(arg1 + 1);
	} else {
		if ((arg1 = strrchr(strip(line), '\t')) != NULL) {
			end = arg1;
			*end = '\0';
			arg1 = strip(arg1 + 1);
		}
	}

	op = memchr(line, ':', end - line);
	if (op) {
		end = op;
		*end = '\0';
		op = strip(op + 1);

		label = line;
	} else {
		op = strip(line);
	}
}

static void
assemble(struct arr *lines, FILE *outfile)
{
static void
assemble(struct list *lines, FILE *outfile)
{
	struct node *currentline = lines->head;

	/* Record address of label declarations. */
	pass = 1;
	while (currentline != NULL) {
		++lineno;
		parse((char *)currentline->value);
		process();
		currentline = currentline->next;
	}

	/* Generate object code. */
	pass = 2;
	while (currentline != NULL) {
		++lineno;
		parse((char *)currentline->value);
		process();
		currentline = currentline->next;
	}

	fwrite(output, sizeof(unsigned char), BUFSIZ, outfile);
}

static void
errmsg(char *msg)
{
	fprintf(stderr, "a80 %ld: %s\n", lineno + 1, msg);
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	FILE *istream, *ostream;
	char *line = NULL;
	size_t len = 0;
	ssize_t nread;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <file.asm>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	istream = fopen(argv[1], "r");
	if (istream == NULL) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}

	struct list *lines = initlist();
	while ((nread = getline(&line, &len, istream)) != -1) {
		push(lines, strdup(line));
	}
	free(line);

	/* TODO Dynamically name the file using the stem of argv[1]. */
	ostream = fopen("x.com", "w+");
	if (ostream == NULL) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}

	assemble(lines, ostream);

	freelist(lines);
	fclose(istream);
	fclose(ostream);

	exit(EXIT_SUCCESS);
}
