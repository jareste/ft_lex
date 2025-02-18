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
static LexRule *g_rules_2 = NULL;

static int inside_mini_section = 0; // for now only being used for section 1
static int inside_rules_section = 0;

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
    else if (!isalpha((unsigned char)trimmed[0]))
    {
        // for (int i = 0; trimmed[i]; i++)
        // {
            fprintf(stderr, "%s:%d: bad character '%c'\n", g_filename, line_num, trimmed[0]);
        // }
        exit(EXIT_FAILURE);
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

int is_comment_line(const char *line)
{
    while (*line && isspace((unsigned char)*line))
    {
        line++;
    }
    return (*line == '/' && *(line + 1) == '/') || (*line == '/' && *(line + 1) == '*');
}

int has_delimiter_line(FILE *fp)
{
    char line[MAX_LINE_LENGTH];
    while (fgets(line, MAX_LINE_LENGTH, fp))
    {
        line_num++;
        if (is_delimiter_line(line))
        {
            return 1;
        }
    }
    return 0;
}

char* find_start_expansion(char* line)
{
    char* start = line;
    int expansion_started = 0;
    char* last_start;
    if (*start == '{')
    {
        expansion_started = 1;
        last_start = start;
        start++;
    }
    while (*start)
    {
        if (*start == '{')
        {
            if (expansion_started == 0)
            {
                last_start = start;
                return last_start;
            }
            expansion_started += 1;
        }
        else if (*start == '}')
        {
            expansion_started -= 1;
        }
        start++;
    }
    return NULL;
}

int lex_parser(char* filename)
{    
    FILE *fp = fopen(filename, "r");
    int delimiter_found = 0;
    if (!fp)
    {
        perror("Error opening file");
        return EXIT_FAILURE;
    }
    g_filename = filename;

    if (!has_delimiter_line(fp))
    {
        line_num++;
        fprintf(stderr, "%s:%d: premature EOF\n", g_filename, line_num);
        fclose(fp);
        return EXIT_FAILURE;
    }

    char line[MAX_LINE_LENGTH];

    rewind(fp);
    line_num = 0;
    while (fgets(line, MAX_LINE_LENGTH, fp))
    {
        line_num++;
        if (is_comment_line(line))
        {
            continue;
        }
        if (is_delimiter_line(line) && !inside_mini_section)
        {
            delimiter_found = 1;
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
            if (!inside_mini_section)
            {
                fprintf(stderr, "%s:%d: unrecognized '%c' directive\n", g_filename, line_num, line[0]);
                exit(EXIT_FAILURE);
            }

            LexRule *rule = malloc(sizeof(LexRule));
            rule->pattern = strdup(line);

            rule->line_num = line_num;
            FT_LIST_ADD_LAST(&g_rules, rule);
        }
    }
    if (!delimiter_found)
    {
        line_num++;
        fprintf(stderr, "%s:%d: premature EOF\n", g_filename, line_num);
        fclose(fp);
        return EXIT_FAILURE;
    }

    LexMacro *m;
    m = FT_LIST_GET_FIRST(&g_macros);
    while (m)
    {
        printf("Macro: %s -> %s\n", m->name, m->pattern);
        m = FT_LIST_GET_NEXT(&g_macros, m);
    }

    LexRule *r;
    r = g_rules;
    while (r)
    {
        printf("Rule: %s\n", r->pattern);
        r = FT_LIST_GET_NEXT(&g_rules, r);
    }

    rewind(fp);
    line_num = 0;
    while (fgets(line, MAX_LINE_LENGTH, fp))
    {
        line_num++;

        if (is_comment_line(line)) continue;

        if (is_delimiter_line(line)) {  // Found "%%"
            if (inside_rules_section) break;  // End of rules section
            inside_rules_section = 1;
            continue;
        }

        if (!inside_rules_section) continue;  // Ignore lines before "%%"

        // --- Now we are inside the rules section ---

        char *trimmed = trim(line);
        char *action = NULL;
        char *action_start = NULL;
        char *pattern = NULL;
        if (*trimmed == '\0') continue;  // Ignore empty lines

        if (*trimmed != '{')
        {
            action_start = strchr(trimmed, '{');
            if (!action_start)
            {
                if (strchr(trimmed, ';'))
                {
                    action_start = strchr(trimmed, ';');
                    *action_start = '\0';
                    trimmed = trim(trimmed);
                    action = strdup(trimmed);
                    pattern = strdup(action_start + 1);
                }
                else
                {
                    fprintf(stderr, "%s:%d: missing action '{' in rule: %s\n", g_filename, line_num, trimmed);
                    exit(EXIT_FAILURE);
                }
            }
        }
        else
        {
            action_start = find_start_expansion(trimmed);
            *action_start = '\0';
            trimmed = trim(trimmed);
            action = strdup(action_start + 1);
            pattern = strdup(trimmed);
        }

        *action_start = '\0';
        if (!action)
        {    
            pattern = strdup(trim(trimmed));
            action = strdup(trim(action_start + 1));
        }
        LexRule *rule = malloc(sizeof(LexRule));
        rule->name = strdup(pattern);
        rule->pattern = strdup(action);
        rule->line_num = line_num;
        free(action);
        free(pattern);
        FT_LIST_ADD_LAST(&g_rules_2, rule);
    }

    printf("fooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo\n");
    r = g_rules_2;
    while (r)
    {
        printf("Rule: '%s' -> '%s'\n", r->name, r->pattern);
        r = FT_LIST_GET_NEXT(&g_rules_2, r);
    }

    fclose(fp);
    return EXIT_SUCCESS;
}
