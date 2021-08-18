#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arr.h"

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
    if (s == NULL || s[0] == '\0') return NULL;

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

    char *prevdelim = line;
    char *end = strchr(line, '\0');

    char *comment = memchr(line, ';', end - line);
    if (comment) {
        end = comment;
    }

    char *labeldelim = memchr(line, ':', end - prevdelim);
    if (labeldelim) {
        *labeldelim = '\0';
        label = prevdelim;

        prevdelim = labeldelim + 1;
        if (prevdelim >= end) return;
    }

    char *opdelim = memchr(prevdelim, ' ', end - prevdelim);
    if (opdelim) {
        *opdelim = '\0';
        op = prevdelim;

        prevdelim = opdelim + 1;
        if (prevdelim >= end) return;
    } else {
        op = prevdelim;
        return;
    }

    char *arg2delim = memchr(prevdelim, ',', end - prevdelim);
    if (arg2delim) {
        arg2 = strip(arg2delim + 1);
    }

    char *arg1delim = arg2delim ? arg2delim : end;
    if (arg1delim) {
        arg1 = strip(prevdelim);
        *arg1delim = '\0';

        prevdelim = arg1delim + 1;
        if (prevdelim >= end) return;
    }
}

static void
assemble(struct arr *lines, FILE *outfile)
{
    /* Record address of label declarations. */
    pass = 1;
    for (lineno = 0; lineno < lines->size; ++lineno) {
        parse(lines->items[lineno]);
    }

    /* Generate object code. */
    pass = 2;
    for (lineno = 0; lineno < lines->size; ++lineno) {
        parse(lines->items[lineno]);
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

    struct arr *lines = initarr(64);
    while ((nread = getline(&line, &len, istream)) != -1) {
        addarr(lines, strdup(line));
    }
    free(line);

    /* TODO Dynamically name the file using the stem of argv[1]. */
    ostream = fopen("x.com", "w+");
    if (ostream == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    assemble(lines, ostream);

    freearr(lines);
    fclose(istream);
    fclose(ostream);

    exit(EXIT_SUCCESS);
}
