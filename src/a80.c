#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arr.h"

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

    ostream = fopen("x.com", "w+");
    if (ostream == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    freearr(lines);
    fclose(istream);
    fclose(ostream);

    exit(EXIT_SUCCESS);
}
