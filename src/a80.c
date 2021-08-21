#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

struct symtab {
	char *label;
	unsigned short value;
};

static struct list *symtabs;
static unsigned char output[BUFSIZ];
static unsigned short addr;
static size_t noutput;
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

		if (op[0] == '\0') {
			op = NULL;
		}

		label = line;
	} else {
		op = strip(line);
	}
}

static void
argcheck(int passed)
{
	if (!passed) {
		fprintf(stderr,
			"a80 %ld: arguments not correct for mnemonic %s\n",
			lineno, op);
		exit(EXIT_FAILURE);
	}
}

static int
cmpsym(void *s, void *t)
{
	if (s == NULL || t == NULL) {
		return 0;
	}
	return strcmp(
		((struct symtab *)s)->label, ((struct symtab *)t)->label) == 0;
}

static struct symtab *
addsym(void)
{
	if (find(symtabs, label, cmpsym) != NULL) {
		fprintf(stderr, "a80 %ld: duplicate label %s\n", lineno, label);
		exit(EXIT_FAILURE);
	}

	struct symtab *newsym = malloc(sizeof(struct symtab));
	if (newsym == NULL) {
		return NULL;
	}

	newsym->label = label;
	newsym->value = addr;

	push(symtabs, newsym);

	return newsym;
}

static void
pass_act(unsigned short size, int outbyte)
{
	if (pass == 1) {
		if (label) {
			addsym();
		}
		addr += size;
	} else {
		if (outbyte >= 0) {
			output[noutput++] = (unsigned char)outbyte;
		}
	}
}

static int
reg_mod8(char *reg)
{
	switch (reg[0]) {
	case 'b':
		return 0x00;
	case 'c':
		return 0x01;
	case 'd':
		return 0x02;
	case 'e':
		return 0x03;
	case 'h':
		return 0x04;
	case 'l':
		return 0x05;
	case 'm':
		return 0x06;
	case 'a':
		return 0x07;
	default:
		fprintf(stderr, "a80 %ld: invalid register %s\n", lineno, reg);
		exit(EXIT_FAILURE);
	}
}

static void
nop(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(1, 0x00);
}

static void
mov(void)
{
	argcheck(arg1 && arg2);
	pass_act(1, 0x40 + (reg_mod8(arg1) << 3) + reg_mod8(arg2));
}

static void
hlt(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(1, 0x76);
}

static void
add(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0x80 + reg_mod8(arg1));
}

static void
adc(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0x88 + reg_mod8(arg1));
}

static void
sub(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0x90 + reg_mod8(arg1));
}

static void
sbb(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0x98 + reg_mod8(arg1));
}

static void
ana(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0xa0 + reg_mod8(arg1));
}

static void
xra(void)
{
	argcheck(arg1 && arg2);
	pass_act(1, 0xa8 + reg_mod8(arg1));
}

static void
ora(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0xb0 + reg_mod8(arg1));
}

static void
cmp(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0xb8 + reg_mod8(arg1));
}

static void
process(void)
{
	if (!op && !arg1 && !arg2) {
		pass_act(0, -1);
		return;
	}

	if (strcmp(op, "nop") == 0) {
		nop();
	} else if (strcmp(op, "mov")) {
		mov();
	} else if (strcmp(op, "hlt")) {
		hlt();
	} else if (strcmp(op, "add")) {
		add();
	} else if (strcmp(op, "adc")) {
		adc();
	} else if (strcmp(op, "sub")) {
		sub();
	} else if (strcmp(op, "sbb")) {
		sbb();
	} else if (strcmp(op, "ana")) {
		ana();
	} else if (strcmp(op, "xra")) {
		xra();
	} else if (strcmp(op, "ora")) {
		ora();
	} else if (strcmp(op, "cmp")) {
		cmp();
	} else {
		fprintf(stderr, "a80 %ld: unknown mnemonic: %s\n", lineno, op);
		exit(EXIT_FAILURE);
	}
}

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
	lineno = 0;
	currentline = lines->head;
	while (currentline != NULL) {
		++lineno;
		parse((char *)currentline->value);
		process();
		currentline = currentline->next;
	}

	fwrite(output, sizeof(unsigned char), BUFSIZ, outfile);
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

	symtabs = initlist();
	assemble(lines, ostream);

	freelist(lines);
	freelist(symtabs);
	fclose(istream);
	fclose(ostream);

	exit(EXIT_SUCCESS);
}
