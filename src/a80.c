#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arr.h"

static unsigned char output[BUFSIZ];
static unsigned short addr;
static size_t lineno;
static int pass;

static void
assemble(struct arr *lines, FILE *outfile)
{
    pass = 1;
    for (lineno = 0; lineno < lines->size; ++lineno) {
        parse();
    }

    pass = 2;
    for (lineno = 0; lineno < lines->size; ++lineno) {
        parse();
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
