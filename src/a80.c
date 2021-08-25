#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

struct symtab {
	char *label;
	unsigned short value;
};

enum immtype {
	IMM8 = 8,
	IMM16 = 16,
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
	return strcmp(((struct symtab *)s)->label, (char *)t) == 0;
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

	append(symtabs, newsym);

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

static unsigned short
numcheck(char *input)
{
	unsigned short num;
	char *end = input + strlen(input) - 1;

	/* TODO Test for overflow? */
	if (*end == 'h') {
		*end = '\0';
		num = (unsigned short)strtol(input, &end, 16);
	} else {
		num = (unsigned short)strtol(input, &end, 10);
	}

	return num;
}

static void
imm(enum immtype type)
{
	char *arg;
	unsigned short num;
	int found = 0;

	if (strcmp(op, "lxi") == 0 || strcmp(op, "mvi") == 0) {
		arg = arg2;
	} else {
		arg = arg1;
	}

	if (isdigit(arg1[0])) {
		num = numcheck(arg);
	} else {
		if (pass == 2) {
			struct symtab *sym;
			struct node *node = symtabs->head;

			while (node != NULL) {
				sym = (struct symtab *)(node->value);
				if (sym && (strcmp(arg1, sym->label) == 0)) {
					num = sym->value;
					found = 1;
					break;
				}
				node = node->next;
			}

			if (!found) {
				fprintf(stderr, "a80 %ld: label %s undefined\n",
					lineno, arg1);
				exit(EXIT_FAILURE);
			}
		}

		if (pass == 2) {
			output[noutput++] = (unsigned char)(num & 0xff);
			if (type == IMM16) {
				output[noutput++] = (unsigned char)((num >> 8) & 0xff);
			}
		}
	}
}

static void
a16(void)
{
	unsigned short num;
	int found = 0;

	if (isdigit(arg1[0])) {
		num = numcheck(arg1);
	} else {
			struct symtab *sym;
			struct node *node = symtabs->head;

			while (node != NULL) {
				sym = (struct symtab *)(node->value);
				if (sym && (strcmp(arg1, sym->label) == 0)) {
					num = sym->value;
					found = 1;
					break;
				}
				node = node->next;
			}

			if (!found) {
				fprintf(stderr, "a80 %ld: label %s undefined\n",
					lineno, arg1);
				exit(EXIT_FAILURE);
			}
	}

	if (pass == 2) {
		output[noutput++] = (unsigned char)(num & 0xff);
		output[noutput++] = (unsigned char)((num >> 8) & 0xff);
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

static int
reg_mod16(void)
{
	if (strcmp(arg1, "b") == 0) {
		return 0x00;
	} else if (strcmp(arg1, "d") == 0) {
		return 0x10;
	} else if (strcmp(arg1, "h") == 0) {
		return 0x20;
	} else if (strcmp(arg1, "psw") == 0) {
		if (strcmp(op, "pop") == 0 || strcmp(op, "push") == 0) {
			return 0x30;
		} else {
			fprintf(stderr, "a80 %ld: psw may not be used with %s\n",
				lineno, op);
			exit(EXIT_FAILURE);
		}
	} else if (strcmp(arg1, "sp") == 0) {
		if (strcmp(op, "pop") != 0 || strcmp(op, "push") != 0) {
			return 0x30;
		} else {
			fprintf(stderr, "a80 %ld: sp may not be used with %s\n",
				lineno, op);
			exit(EXIT_FAILURE);
		}
	} else {
		fprintf(stderr, "a80 %ld: invalid register for opcode %s\n",
			lineno, op);
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
	argcheck(arg1 && !arg2);
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
adi(void)
{
	argcheck(arg1 && !arg2);
	pass_act(2, 0xc6);
	imm(IMM8);
}

static void
aci(void)
{
	argcheck(arg1 && !arg2);
	pass_act(2, 0xce);
	imm(IMM8);
}

static void
sui(void)
{
	argcheck(arg1 && !arg2);
	pass_act(2, 0xd6);
	imm(IMM8);
}

static void
sbi(void)
{
	argcheck(arg1 && !arg2);
	pass_act(2, 0xde);
	imm(IMM8);
}

static void
ani(void)
{
	argcheck(arg1 && !arg2);
	pass_act(2, 0xe6);
	imm(IMM8);
}

static void
xri(void)
{
	argcheck(arg1 && !arg2);
	pass_act(2, 0xee);
	imm(IMM8);
}

static void
ori(void)
{
	argcheck(arg1 && !arg2);
	pass_act(2, 0xf6);
	imm(IMM8);
}

static void
cpi(void)
{
	argcheck(arg1 && !arg2);
	pass_act(2, 0xfe);
	imm(IMM8);
}

static void
xthl(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(1, 0xe3);
}

static void
pchl(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(1, 0xe9);
}

static void
xchg(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(1, 0xeb);
}

static void
sphl(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(1, 0xf9);
}

static void
push(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0xc5 + reg_mod16());
}

static void
pop(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0xc1 + reg_mod16());
}

static void
out(void)
{
	argcheck(arg1 && !arg2);
	pass_act(2, 0xd3);
	imm(IMM8);
}

static void
in(void)
{
	argcheck(arg1 && !arg2);
	pass_act(2, 0xdb);
	imm(IMM8);
}

static void
di(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(1, 0xf3);
}

static void
ei(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(1, 0xfb);
}

static void
rnz(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(1, 0xc0);
}

static void
jnz(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(3, 0xc2);
	a16();
}

static void
jmp(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0xc3);
	a16();
}

static void
cnz(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0xc4);
	a16();
}

static void
rz(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(1, 0xc8);
}

static void
ret(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(1, 0xc9);
}

static void
jz(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0xca);
	a16();
}

static void
cz(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0xcc);
	a16();
}

static void
call(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0xcd);
	a16();
}

static void
rnc(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(1, 0xd0);
}

static void
jnc(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0xd2);
	a16();
}

static void
cnc(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0xd4);
	a16();
}

static void
rc(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(1, 0xd8);
}

static void
jc(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0xda);
	a16();
}

static void
cc(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0xdc);
	a16();
}

static void
rpo(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(1, 0xe0);
}

static void
jpo(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0xe2);
	a16();
}

static void
cpo(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0xe4);
	a16();
}

static void
rpe(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(1, 0xe8);
}

static void
jpe(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0xea);
	a16();
}

static void
cpe(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0xce);
	a16();
}

static void
rp(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(1, 0xf0);
}

static void
jp(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0xf2);
	a16();
}

static void
cp(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0xf4);
	a16();
}

static void
rm(void)
{
	argcheck(!arg1 && !arg2);
	pass_act(1, 0xf8);
}

static void
jm(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0xfa);
	a16();
}

static void
cm(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0xfc);
	a16();
}

static void
rst(void)
{
	argcheck(arg1 && !arg2);

	int offset = (int)strtol(arg1, (char **)NULL, 10);
	if (offset >= 0 && offset <= 7) {
		pass_act(1, 0xc7 + (offset << 3));
	} else {
		fprintf(stderr, "a80 %ld: invalid reset vector %s\n",
			lineno, arg1);
		exit(EXIT_FAILURE);
	}
}

static void
rlc(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0x07);
}

static void
rrc(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0x0f);
}

static void
ral(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0x17);
}

static void
rar(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0x1f);
}

static void
daa(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0x27);
}

static void
cma(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0x2f);
}

static void
stc(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0x37);
}

static void
cmc(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0x3f);
}

static void
inx(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0x03 + reg_mod16());
}

static void
dad(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0x09 + reg_mod16());
}

static void
dcx(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0x0b + reg_mod16());
}

static void
inr(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0x04 + (reg_mod8(arg1) << 3));
}

static void
dcr(void)
{
	argcheck(arg1 && !arg2);
	pass_act(1, 0x05 + (reg_mod8(arg1) << 3));
}

static void
stax(void)
{
	argcheck(arg1 && !arg2);

	switch (arg1[0]) {
	case 'b':
		pass_act(1, 0x02);
		break;
	case 'd':
		pass_act(1, 0x12);
		break;
	default:
		fprintf(stderr,
			"a80 %ld: stax operates on registers b and d.\n",
			lineno);
		exit(EXIT_FAILURE);
	}
}

static void
ldax(void)
{
	argcheck(arg1 && !arg2);

	switch (arg1[0]) {
	case 'b':
		pass_act(1, 0x0a);
		break;
	case 'd':
		pass_act(1, 0x1a);
		break;
	default:
		fprintf(stderr,
			"a80 %ld: ldax operates on registers b and d.\n",
			lineno);
		exit(EXIT_FAILURE);
	}
}

static void
shld(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0x22);
	a16();
}

static void
lhld(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0x2a);
	a16();
}

static void
sta(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0x32);
	a16();
}

static void
lda(void)
{
	argcheck(arg1 && !arg2);
	pass_act(3, 0x3a);
	a16();
}

static void
mvi(void)
{
	argcheck(arg1 && arg2);
	pass_act(2, 0x06 + (reg_mod8(arg1) << 3));
	imm(IMM8);
}

static void
lxi(void)
{
	argcheck(arg1 && arg2);
	pass_act(3, 0x01 + reg_mod16());
	imm(IMM16);
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
	} else if (strcmp(op, "mov") == 0) {
		mov();
	} else if (strcmp(op, "hlt") == 0) {
		hlt();
	} else if (strcmp(op, "add") == 0) {
		add();
	} else if (strcmp(op, "adc") == 0) {
		adc();
	} else if (strcmp(op, "sub") == 0) {
		sub();
	} else if (strcmp(op, "sbb") == 0) {
		sbb();
	} else if (strcmp(op, "ana") == 0) {
		ana();
	} else if (strcmp(op, "xra") == 0) {
		xra();
	} else if (strcmp(op, "ora") == 0) {
		ora();
	} else if (strcmp(op, "cmp") == 0) {
		cmp();
	} else if (strcmp(op, "adi") == 0) {
		adi();
	} else if (strcmp(op, "aci") == 0) {
		aci();
	} else if (strcmp(op, "sui") == 0) {
		sui();
	} else if (strcmp(op, "sbi") == 0) {
		sbi();
	} else if (strcmp(op, "ani") == 0) {
		ani();
	} else if (strcmp(op, "xri") == 0) {
		xri();
	} else if (strcmp(op, "ori") == 0) {
		ori();
	} else if (strcmp(op, "cpi") == 0) {
		cpi();
	} else if (strcmp(op, "xthl") == 0) {
		xthl();
	} else if (strcmp(op, "pchl") == 0) {
		pchl();
	} else if (strcmp(op, "xchg") == 0) {
		xchg();
	} else if (strcmp(op, "sphl") == 0) {
		sphl();
	} else if (strcmp(op, "push") == 0) {
		push();
	} else if (strcmp(op, "pop") == 0) {
		pop();
	} else if (strcmp(op, "out") == 0) {
		out();
	} else if (strcmp(op, "in") == 0) {
		in();
	} else if (strcmp(op, "di") == 0) {
		di();
	} else if (strcmp(op, "ei") == 0) {
		ei();
	} else if (strcmp(op, "rnz") == 0) {
		rnz();
	} else if (strcmp(op, "jnz") == 0) {
		jnz();
	} else if (strcmp(op, "jmp") == 0) {
		jmp();
	} else if (strcmp(op, "cnz") == 0) {
		cnz();
	} else if (strcmp(op, "rz") == 0) {
		rz();
	} else if (strcmp(op, "ret") == 0) {
		ret();
	} else if (strcmp(op, "jz") == 0) {
		jz();
	} else if (strcmp(op, "cz") == 0) {
		cz();
	} else if (strcmp(op, "call") == 0) {
		call();
	} else if (strcmp(op, "rnc") == 0) {
		rnc();
	} else if (strcmp(op, "jnc") == 0) {
		jnc();
	} else if (strcmp(op, "cnc") == 0) {
		cnc();
	} else if (strcmp(op, "rc") == 0) {
		rc();
	} else if (strcmp(op, "jc") == 0) {
		jc();
	} else if (strcmp(op, "cc") == 0) {
		cc();
	} else if (strcmp(op, "rpo") == 0) {
		rpo();
	} else if (strcmp(op, "jpo") == 0) {
		jpo();
	} else if (strcmp(op, "cpo") == 0) {
		cpo();
	} else if (strcmp(op, "rpe") == 0) {
		rpe();
	} else if (strcmp(op, "jpe") == 0) {
		jpe();
	} else if (strcmp(op, "cpe") == 0) {
		cpe();
	} else if (strcmp(op, "rp") == 0) {
		rp();
	} else if (strcmp(op, "jp") == 0) {
		jp();
	} else if (strcmp(op, "cp") == 0) {
		cp();
	} else if (strcmp(op, "rm") == 0) {
		rm();
	} else if (strcmp(op, "jm") == 0) {
		jm();
	} else if (strcmp(op, "cm") == 0) {
		cm();
	} else if (strcmp(op, "rst") == 0) {
		rst();
	} else if (strcmp(op, "rlc") == 0) {
		rlc();
	} else if (strcmp(op, "rrc") == 0) {
		rrc();
	} else if (strcmp(op, "ral") == 0) {
		ral();
	} else if (strcmp(op, "rar") == 0) {
		rar();
	} else if (strcmp(op, "daa") == 0) {
		daa();
	} else if (strcmp(op, "cma") == 0) {
		cma();
	} else if (strcmp(op, "stc") == 0) {
		stc();
	} else if (strcmp(op, "cmc") == 0) {
		cmc();
	} else if (strcmp(op, "inx") == 0) {
		inx();
	} else if (strcmp(op, "dad") == 0) {
		dad();
	} else if (strcmp(op, "dcx") == 0) {
		dcx();
	} else if (strcmp(op, "inr") == 0) {
		inr();
	} else if (strcmp(op, "dcr") == 0) {
		dcr();
	} else if (strcmp(op, "stax") == 0) {
		stax();
	} else if (strcmp(op, "ldax") == 0) {
		ldax();
	} else if (strcmp(op, "shld") == 0) {
		shld();
	} else if (strcmp(op, "lhld") == 0) {
		lhld();
	} else if (strcmp(op, "sta") == 0) {
		sta();
	} else if (strcmp(op, "lda") == 0) {
		lda();
	} else if (strcmp(op, "mvi") == 0) {
		mvi();
	} else if (strcmp(op, "lxi") == 0) {
		lxi();
	} else {
		fprintf(stderr, "a80 %ld: unknown mnemonic: %s\n", lineno, op);
		exit(EXIT_FAILURE);
	}
}

static void
assemble(struct list *lines, FILE *outfile)
{
	struct node *line;

	/*
	 * parse() directly modifies the list. Therefore, copy the original
	 * list for the second pass.
	 */
	struct list *linesdup = initlist();
	for (line = lines->head; line != NULL; line = line->next) {
		if (line->value != NULL) {
			append(linesdup, strdup((char *)line->value));
		}
	}

	/* Record address of label declarations. */
	pass = 1;
	for (line = lines->head; line != NULL; line = line->next, ++lineno) {
		parse((char *)line->value);
		process();
	}

	/* Generate object code. */
	pass = 2, lineno = 0;
	for (line = linesdup->head; line != NULL; line = line->next, ++lineno) {
		parse((char *)line->value);
		process();
	}

	fwrite(output, sizeof(unsigned char), BUFSIZ, outfile);
	freelist(linesdup);
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
		append(lines, strdup(line));
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
