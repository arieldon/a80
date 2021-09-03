#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

#define errmsg(fmt, ...) \
	do { \
		fprintf(stderr, "a80 %ld: " #fmt "\n", lineno, __VA_ARGS__); \
		exit(EXIT_FAILURE); \
	} while(0)
#define assertarg(args) \
	do { \
		if (!args) \
			errmsg("%s", "arguments not correct for mnemonic"); \
	} while (0)

struct symtab {
	char *label;
	unsigned short value;
};

enum immtype {
	IMM8 = 8,
	IMM16 = 16,
};

static struct list *symtabs;
static unsigned char output[65536];
static unsigned short addr;
static size_t noutput;
static size_t lineno;
static int pass;

/* FORMAT [label:] [mnemonic [operand1[, operand2]]] [; comment] */
static char *label;
static char *mnemonic;
static char *operand1;
static char *operand2;
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
	mnemonic = NULL;
	operand1 = NULL;
	operand2 = NULL;
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

	/*
	 * If there exists a single quote or tick in the line after the comment
	 * has been tokenized and separated, then `operand1` consists of a
	 * string and `operand2` is empty.
	 *
	 * First find the opening single quote, then the closing single quote.
	 *
	 * This condition exists for mnemonicuction `db`.
	 */
	if ((operand1 = strchr(line, '\'')) != NULL) {
		end = operand1;
		*end = '\0';
		operand1 = strip(operand1 + 1);

		/* Find closing single quote. */
		end = strchr(operand1, '\'');
		*end = '\0';

		goto setmnem;
	}

	operand2 = memchr(line, ',', end - line);
	if (operand2) {
		end = operand2;
		*end = '\0';
		operand2 = strip(operand2 + 1);
	}

	operand1 = strrchr(strip(line), ' ');
	if (operand1) {
		end = operand1;
		*end = '\0';
		operand1 = strip(operand1 + 1);
	} else {
		if ((operand1 = strrchr(strip(line), '\t')) != NULL) {
			end = operand1;
			*end = '\0';
			operand1 = strip(operand1 + 1);
		}
	}
setmnem:
	mnemonic = memchr(line, ':', end - line);
	if (mnemonic) {
		end = mnemonic;
		*end = '\0';
		mnemonic = strip(mnemonic + 1);

		if (mnemonic[0] == '\0') {
			mnemonic = NULL;
		}

		label = line;
	} else {
		mnemonic = strip(line);
	}
}

static int
cmpsym(void *symtab, void *str)
{
	if (symtab == NULL || str == NULL) {
		return 0;
	}
	return strcmp(((struct symtab *)symtab)->label, (char *)str) == 0;
}

static struct symtab *
addsym(void)
{
	if (find(symtabs, label, cmpsym) != NULL) {
		errmsg("duplicate label %s", label);
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

	num = *end == 'h'
		? (unsigned short)strtol(input, &end, 16)
		: (unsigned short)strtol(input, &end, 10);

	errno = 0;
	if ((errno == ERANGE && (num == SHRT_MAX || num == 0))
			|| (errno != 0 && num == 0)) {
		errmsg("%s", "unable to convert input into a number");
	}

	if (end == input) {
		errmsg("%s", "no digits present");
	}

	return num;
}

static void
imm(enum immtype type)
{
	char *arg;
	unsigned short num;
	int found = 0;

	if (strcmp(mnemonic, "lxi") == 0 || strcmp(mnemonic, "mvi") == 0) {
		arg = operand2;
	} else {
		arg = operand1;
	}

	if (isdigit(arg[0])) {
		num = numcheck(arg);
	} else {
		if (pass == 2) {
			struct symtab *sym;
			struct node *node = symtabs->head;

			while (node != NULL) {
				sym = (struct symtab *)(node->value);
				if (sym && (strcmp(arg, sym->label) == 0)) {
					num = sym->value;
					found = 1;
					break;
				}
				node = node->next;
			}

			if (!found) {
				errmsg("label %s undefined", arg);
			}
		}
	}

	if (pass == 2) {
		output[noutput++] = (unsigned char)(num & 0xff);
		if (type == IMM16) {
			output[noutput++] = (unsigned char)((num >> 8) & 0xff);
		}
	}
}

static void
a16(void)
{
	unsigned short num;
	int found = 0;

	if (isdigit(operand1[0])) {
		num = numcheck(operand1);
	} else {
		struct symtab *sym;
		struct node *node = symtabs->head;

		while (node != NULL) {
			sym = (struct symtab *)(node->value);
			if (sym && (strcmp(operand1, sym->label) == 0)) {
				num = sym->value;
				found = 1;
				break;
			}
			node = node->next;
		}

		if (!found) {
			errmsg("label %s undefined", operand1);
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
		errmsg("invalid register %s", reg);
	}
}

static int
reg_mod16(void)
{
	if (strcmp(operand1, "b") == 0) {
		return 0x00;
	} else if (strcmp(operand1, "d") == 0) {
		return 0x10;
	} else if (strcmp(operand1, "h") == 0) {
		return 0x20;
	} else if (strcmp(operand1, "psw") == 0) {
		if (strcmp(mnemonic, "pop") == 0
				|| strcmp(mnemonic, "push") == 0) {
			return 0x30;
		} else {
			errmsg("psw may not be used with %s", mnemonic);
		}
	} else if (strcmp(operand1, "sp") == 0) {
		if (strcmp(mnemonic, "pop") != 0
				|| strcmp(mnemonic, "push") != 0) {
			return 0x30;
		} else {
			errmsg("sp may not be used with %s", mnemonic);
		}
	} else {
		errmsg("invalid register for mnemonicuction %s", mnemonic);
	}
}

static unsigned short
dollar(void)
{
	unsigned short num = addr;

	if (strlen(operand1) > 1) {
		if (operand1[1] == '+') {
			num += numcheck(operand1 + 2);
		} else if (operand1[1] == '-') {
			num -= numcheck(operand1 + 2);
		} else if (operand1[1] == '*') {
			num *= numcheck(operand1 + 2);
		} else if (operand1[1] == '/') {
			num /= numcheck(operand1 + 2);
		} else if (operand1[1] == '%') {
			num %= numcheck(operand1 + 2);
		} else {
			errmsg("%s", "invalid operator in equ");
		}
	}

	return num;
}

static void
nop(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0x00);
}

static void
mov(void)
{
	assertarg(operand1 && operand2);
	pass_act(1, 0x40 + (reg_mod8(operand1) << 3) + reg_mod8(operand2));
}

static void
hlt(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0x76);
}

static void
add(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0x80 + reg_mod8(operand1));
}

static void
adc(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0x88 + reg_mod8(operand1));
}

static void
sub(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0x90 + reg_mod8(operand1));
}

static void
sbb(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0x98 + reg_mod8(operand1));
}

static void
ana(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0xa0 + reg_mod8(operand1));
}

static void
xra(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0xa8 + reg_mod8(operand1));
}

static void
ora(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0xb0 + reg_mod8(operand1));
}

static void
cmp(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0xb8 + reg_mod8(operand1));
}

static void
adi(void)
{
	assertarg(operand1 && !operand2);
	pass_act(2, 0xc6);
	imm(IMM8);
}

static void
aci(void)
{
	assertarg(operand1 && !operand2);
	pass_act(2, 0xce);
	imm(IMM8);
}

static void
sui(void)
{
	assertarg(operand1 && !operand2);
	pass_act(2, 0xd6);
	imm(IMM8);
}

static void
sbi(void)
{
	assertarg(operand1 && !operand2);
	pass_act(2, 0xde);
	imm(IMM8);
}

static void
ani(void)
{
	assertarg(operand1 && !operand2);
	pass_act(2, 0xe6);
	imm(IMM8);
}

static void
xri(void)
{
	assertarg(operand1 && !operand2);
	pass_act(2, 0xee);
	imm(IMM8);
}

static void
ori(void)
{
	assertarg(operand1 && !operand2);
	pass_act(2, 0xf6);
	imm(IMM8);
}

static void
cpi(void)
{
	assertarg(operand1 && !operand2);
	pass_act(2, 0xfe);
	imm(IMM8);
}

static void
xthl(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0xe3);
}

static void
pchl(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0xe9);
}

static void
xchg(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0xeb);
}

static void
sphl(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0xf9);
}

static void
push(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0xc5 + reg_mod16());
}

static void
pop(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0xc1 + reg_mod16());
}

static void
out(void)
{
	assertarg(operand1 && !operand2);
	pass_act(2, 0xd3);
	imm(IMM8);
}

static void
in(void)
{
	assertarg(operand1 && !operand2);
	pass_act(2, 0xdb);
	imm(IMM8);
}

static void
di(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0xf3);
}

static void
ei(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0xfb);
}

static void
rnz(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0xc0);
}

static void
jnz(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(3, 0xc2);
	a16();
}

static void
jmp(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0xc3);
	a16();
}

static void
cnz(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0xc4);
	a16();
}

static void
rz(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0xc8);
}

static void
ret(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0xc9);
}

static void
jz(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0xca);
	a16();
}

static void
cz(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0xcc);
	a16();
}

static void
call(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0xcd);
	a16();
}

static void
rnc(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0xd0);
}

static void
jnc(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0xd2);
	a16();
}

static void
cnc(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0xd4);
	a16();
}

static void
rc(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0xd8);
}

static void
jc(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0xda);
	a16();
}

static void
cc(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0xdc);
	a16();
}

static void
rpo(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0xe0);
}

static void
jpo(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0xe2);
	a16();
}

static void
cpo(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0xe4);
	a16();
}

static void
rpe(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0xe8);
}

static void
jpe(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0xea);
	a16();
}

static void
cpe(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0xec);
	a16();
}

static void
rp(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0xf0);
}

static void
jp(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0xf2);
	a16();
}

static void
cp(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0xf4);
	a16();
}

static void
rm(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0xf8);
}

static void
jm(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0xfa);
	a16();
}

static void
cm(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0xfc);
	a16();
}

static void
rst(void)
{
	assertarg(operand1 && !operand2);

	int offset = (int)strtol(operand1, (char **)NULL, 10);
	if (offset >= 0 && offset <= 7) {
		pass_act(1, 0xc7 + (offset << 3));
	} else {
		errmsg("invalid reset vector %s", operand1);
	}
}

static void
rlc(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0x07);
}

static void
rrc(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0x0f);
}

static void
ral(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0x17);
}

static void
rar(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0x1f);
}

static void
daa(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0x27);
}

static void
cma(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0x2f);
}

static void
stc(void)
{
	assertarg(!operand1 && !operand2);
	pass_act(1, 0x37);
}

static void
cmc(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0x3f);
}

static void
inx(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0x03 + reg_mod16());
}

static void
dad(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0x09 + reg_mod16());
}

static void
dcx(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0x0b + reg_mod16());
}

static void
inr(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0x04 + (reg_mod8(operand1) << 3));
}

static void
dcr(void)
{
	assertarg(operand1 && !operand2);
	pass_act(1, 0x05 + (reg_mod8(operand1) << 3));
}

static void
stax(void)
{
	assertarg(operand1 && !operand2);

	switch (operand1[0]) {
	case 'b':
		pass_act(1, 0x02);
		break;
	case 'd':
		pass_act(1, 0x12);
		break;
	default:
		errmsg("%s", "stax operates on registers b and d");
	}
}

static void
ldax(void)
{
	assertarg(operand1 && !operand2);

	switch (operand1[0]) {
	case 'b':
		pass_act(1, 0x0a);
		break;
	case 'd':
		pass_act(1, 0x1a);
		break;
	default:
		errmsg("%s", "ladax operates on registers b and d");
	}
}

static void
shld(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0x22);
	a16();
}

static void
lhld(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0x2a);
	a16();
}

static void
sta(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0x32);
	a16();
}

static void
lda(void)
{
	assertarg(operand1 && !operand2);
	pass_act(3, 0x3a);
	a16();
}

static void
mvi(void)
{
	assertarg(operand1 && operand2);
	pass_act(2, 0x06 + (reg_mod8(operand1) << 3));
	imm(IMM8);
}

static void
lxi(void)
{
	assertarg(operand1 && operand2);
	pass_act(3, 0x01 + reg_mod16());
	imm(IMM16);
}

static void
name(void)
{
	assertarg(!label && operand1 && !operand2);
}

static void
title(void)
{
	assertarg(!label && operand1 && !operand2);
}

static void
end(void)
{
	assertarg(!label && !operand1 && !operand2);
}

static void
org(void)
{
	assertarg(!label && operand1 && !operand2);

	if (isdigit(operand1[0])) {
		if (pass == 1) {
			addr = numcheck(operand1);
		}
	} else {
		errmsg("%s", "org requires a number");
	}
}

static void
equ(void)
{
	unsigned short value;

	if (!label) {
		errmsg("%s", "equ statement requires a label");
	}

	if (operand1[0] == '$') {
		value = dollar();
	} else {
		value = numcheck(operand1);
	}

	if (pass == 1) {
		unsigned short tmp = addr;
		addr = value;
		addsym();
		addr = tmp;
	}
}

static void
dw(void)
{
	assertarg(operand1 && !operand2);

	if (pass == 1) {
		if (label) {
			addsym();
		}
	}
	a16();

	addr += 2;
}

static void
ds(void)
{
	assertarg(operand1 && !operand2);

	if (pass == 1) {
		if (label) {
			addsym();
		}
	} else {
		unsigned short num = numcheck(operand1);
		for (size_t i = 0; i < num; ++i) {
			output[++noutput] = 0;
		}
	}

	addr += numcheck(operand1);
}

static void
db(void)
{
	assertarg(operand1 && !operand2);

	if (isdigit(operand1[0])) {
		pass_act(1, numcheck(operand1));
	} else {
		if (pass == 1) {
			if (label) {
				addsym();
			}
		} else {
			for (char *c = operand1; *c != '\0'; ++c) {
				output[noutput++] = (unsigned char)*c;
			}
		}
		addr += strlen(operand1);
	}
}

static void
process(void)
{
	if (!mnemonic && !operand1 && !operand2) {
		pass_act(0, -1);
		return;
	}

	if (strcmp(mnemonic, "nop") == 0) {
		nop();
	} else if (strcmp(mnemonic, "mov") == 0) {
		mov();
	} else if (strcmp(mnemonic, "hlt") == 0) {
		hlt();
	} else if (strcmp(mnemonic, "add") == 0) {
		add();
	} else if (strcmp(mnemonic, "adc") == 0) {
		adc();
	} else if (strcmp(mnemonic, "sub") == 0) {
		sub();
	} else if (strcmp(mnemonic, "sbb") == 0) {
		sbb();
	} else if (strcmp(mnemonic, "ana") == 0) {
		ana();
	} else if (strcmp(mnemonic, "xra") == 0) {
		xra();
	} else if (strcmp(mnemonic, "ora") == 0) {
		ora();
	} else if (strcmp(mnemonic, "cmp") == 0) {
		cmp();
	} else if (strcmp(mnemonic, "adi") == 0) {
		adi();
	} else if (strcmp(mnemonic, "aci") == 0) {
		aci();
	} else if (strcmp(mnemonic, "sui") == 0) {
		sui();
	} else if (strcmp(mnemonic, "sbi") == 0) {
		sbi();
	} else if (strcmp(mnemonic, "ani") == 0) {
		ani();
	} else if (strcmp(mnemonic, "xri") == 0) {
		xri();
	} else if (strcmp(mnemonic, "ori") == 0) {
		ori();
	} else if (strcmp(mnemonic, "cpi") == 0) {
		cpi();
	} else if (strcmp(mnemonic, "xthl") == 0) {
		xthl();
	} else if (strcmp(mnemonic, "pchl") == 0) {
		pchl();
	} else if (strcmp(mnemonic, "xchg") == 0) {
		xchg();
	} else if (strcmp(mnemonic, "sphl") == 0) {
		sphl();
	} else if (strcmp(mnemonic, "push") == 0) {
		push();
	} else if (strcmp(mnemonic, "pop") == 0) {
		pop();
	} else if (strcmp(mnemonic, "out") == 0) {
		out();
	} else if (strcmp(mnemonic, "in") == 0) {
		in();
	} else if (strcmp(mnemonic, "di") == 0) {
		di();
	} else if (strcmp(mnemonic, "ei") == 0) {
		ei();
	} else if (strcmp(mnemonic, "rnz") == 0) {
		rnz();
	} else if (strcmp(mnemonic, "jnz") == 0) {
		jnz();
	} else if (strcmp(mnemonic, "jmp") == 0) {
		jmp();
	} else if (strcmp(mnemonic, "cnz") == 0) {
		cnz();
	} else if (strcmp(mnemonic, "rz") == 0) {
		rz();
	} else if (strcmp(mnemonic, "ret") == 0) {
		ret();
	} else if (strcmp(mnemonic, "jz") == 0) {
		jz();
	} else if (strcmp(mnemonic, "cz") == 0) {
		cz();
	} else if (strcmp(mnemonic, "call") == 0) {
		call();
	} else if (strcmp(mnemonic, "rnc") == 0) {
		rnc();
	} else if (strcmp(mnemonic, "jnc") == 0) {
		jnc();
	} else if (strcmp(mnemonic, "cnc") == 0) {
		cnc();
	} else if (strcmp(mnemonic, "rc") == 0) {
		rc();
	} else if (strcmp(mnemonic, "jc") == 0) {
		jc();
	} else if (strcmp(mnemonic, "cc") == 0) {
		cc();
	} else if (strcmp(mnemonic, "rpo") == 0) {
		rpo();
	} else if (strcmp(mnemonic, "jpo") == 0) {
		jpo();
	} else if (strcmp(mnemonic, "cpo") == 0) {
		cpo();
	} else if (strcmp(mnemonic, "rpe") == 0) {
		rpe();
	} else if (strcmp(mnemonic, "jpe") == 0) {
		jpe();
	} else if (strcmp(mnemonic, "cpe") == 0) {
		cpe();
	} else if (strcmp(mnemonic, "rp") == 0) {
		rp();
	} else if (strcmp(mnemonic, "jp") == 0) {
		jp();
	} else if (strcmp(mnemonic, "cp") == 0) {
		cp();
	} else if (strcmp(mnemonic, "rm") == 0) {
		rm();
	} else if (strcmp(mnemonic, "jm") == 0) {
		jm();
	} else if (strcmp(mnemonic, "cm") == 0) {
		cm();
	} else if (strcmp(mnemonic, "rst") == 0) {
		rst();
	} else if (strcmp(mnemonic, "rlc") == 0) {
		rlc();
	} else if (strcmp(mnemonic, "rrc") == 0) {
		rrc();
	} else if (strcmp(mnemonic, "ral") == 0) {
		ral();
	} else if (strcmp(mnemonic, "rar") == 0) {
		rar();
	} else if (strcmp(mnemonic, "daa") == 0) {
		daa();
	} else if (strcmp(mnemonic, "cma") == 0) {
		cma();
	} else if (strcmp(mnemonic, "stc") == 0) {
		stc();
	} else if (strcmp(mnemonic, "cmc") == 0) {
		cmc();
	} else if (strcmp(mnemonic, "inx") == 0) {
		inx();
	} else if (strcmp(mnemonic, "dad") == 0) {
		dad();
	} else if (strcmp(mnemonic, "dcx") == 0) {
		dcx();
	} else if (strcmp(mnemonic, "inr") == 0) {
		inr();
	} else if (strcmp(mnemonic, "dcr") == 0) {
		dcr();
	} else if (strcmp(mnemonic, "stax") == 0) {
		stax();
	} else if (strcmp(mnemonic, "ldax") == 0) {
		ldax();
	} else if (strcmp(mnemonic, "shld") == 0) {
		shld();
	} else if (strcmp(mnemonic, "lhld") == 0) {
		lhld();
	} else if (strcmp(mnemonic, "sta") == 0) {
		sta();
	} else if (strcmp(mnemonic, "lda") == 0) {
		lda();
	} else if (strcmp(mnemonic, "mvi") == 0) {
		mvi();
	} else if (strcmp(mnemonic, "lxi") == 0) {
		lxi();
	} else if (strcmp(mnemonic, "name") == 0) {
		name();
	} else if (strcmp(mnemonic, "title") == 0) {
		title();
	} else if (strcmp(mnemonic, "end") == 0) {
		end();
	} else if (strcmp(mnemonic, "org") == 0) {
		org();
	} else if (strcmp(mnemonic, "equ") == 0) {
		equ();
	} else if (strcmp(mnemonic, "dw") == 0) {
		dw();
	} else if (strcmp(mnemonic, "ds") == 0) {
		ds();
	} else if (strcmp(mnemonic, "db") == 0) {
		db();
	} else {
		errmsg("unknown mnemonic: %s", mnemonic);
	}
}

static void
assemble(struct list *lines)
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

	symtabs = initlist();
	assemble(lines);

	char *ext = strchr(argv[1], '.');
	*ext = '\0';

	ostream = fopen(argv[1], "w+");
	if (ostream == NULL) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	fwrite(output, sizeof(unsigned char), sizeof(output), ostream);

	freelist(lines);
	freelist(symtabs);
	fclose(istream);
	fclose(ostream);

	exit(EXIT_SUCCESS);
}
