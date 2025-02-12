#ifndef LEX_PARSER_H
#define LEX_PARSER_H

#include "ft_list.h"

typedef struct
{
    list_item_t l;
    char* name;
    char* pattern;
    int line_num;
} LexMacro;

typedef struct
{
    list_item_t l;
    char*   name;
    char*   pattern;
    int     line_num;
} LexRule;

int lex_parser(char* filename);

#endif