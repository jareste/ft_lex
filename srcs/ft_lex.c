#include <stdio.h>
#include "lex_parser.h"

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
    }

    if (lex_parser(argv[1]) == 0)
    {
        printf("File is a valid lex file\n");
    }
    else
    {
        printf("File is not a valid lex file\n");
    }

}