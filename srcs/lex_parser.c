#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lex_parser.h"

#define MAX_LINE_LENGTH 1024

static char* g_filename;
static int line_num = 0;
static LexMacro *g_macros = NULL;
static LexRule *g_rules = NULL;

char *trim(char *str)
{
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0)
        return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    return str;
}

LexMacro *parse_macro_line(const char *line)
{
    static int inside_mini_section = 0;
    char *buf = strdup(line);
    if (!buf) return NULL;
    char *trimmed = trim(buf);
    if (*trimmed == '\0' || trimmed[0] == '/' || trimmed[0] == '#')
    {
        free(buf);
        return NULL;
    }
    if (trimmed[0] == '%')
    {
        if (trimmed[1] == '{')
            inside_mini_section = 1;
        else if (trimmed[1] == '}' && (inside_mini_section == 1))
            inside_mini_section = !inside_mini_section;
        else if (trimmed[1] == '}' && (inside_mini_section == 0))
        {
            for (int i = 0; trimmed[i]; i++)
            {
                fprintf(stderr, "%s:%d: bad character '%c'\n", g_filename, line_num, trimmed[i]);
            }
            exit(EXIT_FAILURE);
        }


        free(buf);
        return NULL;
    }
    char *space = strchr(trimmed, ' ');
    if (!space)
    {
        free(buf);
        return NULL;
    }
    *space = '\0';
    char *name = trimmed;
    char *pattern = trim(space + 1);
    if (*pattern == '\0')
    {
        free(buf);
        return NULL;
    }
    
    LexMacro *macro = malloc(sizeof(LexMacro));
    macro->name = strdup(name);
    macro->pattern = strdup(pattern);
    free(buf);
    return macro;
}

int is_delimiter_line(const char *line)
{
    while (*line && isspace((unsigned char)*line))
    {
        line++;
    }
    if (line[0] != '%' || line[1] != '%')
        return 0;
    line += 2;
    while (*line)
    {
        if (!isspace((unsigned char)*line))
            return 0;
        line++;
    }
    return 1;
}

int lex_parser(char* filename)
{    
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        perror("Error opening file");
        return EXIT_FAILURE;
    }
    g_filename = filename;
    char line[MAX_LINE_LENGTH];
       
    rewind(fp);
    while (fgets(line, MAX_LINE_LENGTH, fp))
    {
        line_num++;
        if (is_delimiter_line(line))
        {
            break;
        }
        LexMacro *macro = parse_macro_line(line);
        if (macro)
        {
            macro->line_num = line_num;
            FT_LIST_ADD_LAST(&g_macros, macro);
        }
        else
        {
            char* trimmed = trim(line);
            if ((*trimmed == '\0') || (strcmp(trimmed, "%{") == 0) || (strcmp(trimmed, "%}") == 0))// || trimmed[0] == '/' || trimmed[0] == '#')
            {
                continue;
            }

            LexRule *rule = malloc(sizeof(LexRule));
            rule->pattern = strdup(line);

            rule->line_num = line_num;
            FT_LIST_ADD_LAST(&g_rules, rule);
        }
    }

    LexMacro *m;
    m = FT_LIST_GET_FIRST(&g_macros);
    while (m)
    {
        printf("Macro: %s -> %s\n", m->name, m->pattern);
        m = FT_LIST_GET_NEXT(&g_macros, m);
    }

    LexRule *r;
    r = FT_LIST_GET_FIRST(&g_rules);
    while (r)
    {
        printf("Rule: %s\n", r->pattern);
        r = FT_LIST_GET_NEXT(&g_rules, r);
    }

    fclose(fp);
    return EXIT_SUCCESS;
}
