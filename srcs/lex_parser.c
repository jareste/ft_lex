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
    else if ((inside_mini_section == 0) && !isalpha((unsigned char)trimmed[0]))
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
    return ((*line == '/') && (*(line + 1) == '/') && (inside_mini_section))\
            || ((*line == '/') && (*(line + 1) == '*'));
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

void get_rules_section(FILE *fp)
{
    char line[MAX_LINE_LENGTH];
    int action_buffer_size = 4096; /* Lets assume an action must be below 4096 bytes */
    char action_buffer[action_buffer_size]; /* TODO use malloc instead of VLA */
    char *trimmed;
    char *action_start = NULL;
    char *pattern = NULL;

    while (fgets(line, MAX_LINE_LENGTH, fp))
    {
        line_num++;

        if (is_delimiter_line(line))
            break;

        trimmed = trim(line);
        if (*trimmed == '\0')
            continue;

        /* Reset used pointers */
        action_start = NULL;
        pattern = NULL;
        action_buffer[0] = '\0';

        if (*trimmed != '{')  /* Normal case: pattern and action on one line? */
        {
            /* Find the start of the action */
            action_start = strchr(trimmed, '{');
            if (!action_start)
            {
                if (strchr(trimmed, ';'))
                    action_start = strchr(trimmed, ';');
                else
                {
                    fprintf(stderr, "%s:%d: missing action '{' in rule: %s\n",
                            g_filename, line_num, trimmed);
                    exit(EXIT_FAILURE);
                }
            }
        }
        else
        {
            /* If the line starts with '{', you might have a helper like find_start_expansion */
            action_start = find_start_expansion(trimmed);
        }

        *action_start = '\0';
        pattern = strdup(trim(trimmed));

        char *action_part = action_start + 1;
        char *end_brace = strchr(action_part, '}');
        if (end_brace)
        {
            /* Single-line action: remove the closing brace */
            *end_brace = '\0';
            strncpy(action_buffer, action_part, action_buffer_size - 1);
        }
        else
        {
            /* Multiline action: start with the remainder of the current line */
            strncpy(action_buffer, action_part, action_buffer_size - 1);

            while (fgets(line, MAX_LINE_LENGTH, fp))
            {
                line_num++;
                char *line_end = strchr(line, '}');
                if (line_end)
                {
                    size_t current_len = strlen(action_buffer);
                    strncat(action_buffer, "\n", action_buffer_size - current_len - 1);
                    current_len = strlen(action_buffer);
                    size_t num_to_copy = line_end - line;
                    if (num_to_copy > action_buffer_size - current_len - 1)
                        num_to_copy = action_buffer_size - current_len - 1;
                    strncat(action_buffer, line, num_to_copy);

                    break;
                }
                else
                {
                    size_t current_len = strlen(action_buffer);
                    strncat(action_buffer, "\n", action_buffer_size - current_len - 1);
                    current_len = strlen(action_buffer);
                    strncat(action_buffer, line, action_buffer_size - current_len - 1);
                }
            }
        }

        LexRule *rule = malloc(sizeof(LexRule));
        rule->name = strdup(pattern);
        rule->pattern = strdup(action_buffer);
        rule->line_num = line_num;
        free(pattern);
        FT_LIST_ADD_LAST(&g_rules_2, rule);
    }
}


int lex_parser(char* filename)
{    
    FILE *fp;
    int delimiter_found = 0;
    char line[MAX_LINE_LENGTH];
    
    fp = fopen(filename, "r");
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

    rewind(fp);
    line_num = 0;
    while (fgets(line, MAX_LINE_LENGTH, fp))
    {
        line_num++;
        line[strcspn(line, "\n")] = 0;
        if (is_comment_line(line))
            continue;
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
            // if ((line[0] == '\0') || (strcmp(line, "%{") == 0) || (strcmp(line, "%}") == 0))
            {
                continue;
            }
            if (!inside_mini_section)
            {
                if (trimmed[0] == line[0])
                    fprintf(stderr, "%s:%d: unrecognized '%c' directive\n", g_filename, line_num, line[0]);
                else
                    fprintf(stderr, "%s:%d: bad character: '%c'\n", g_filename, line_num, trimmed[0]);
                exit(EXIT_FAILURE);
            }

            LexRule *rule = malloc(sizeof(LexRule));
            rule->pattern = strdup(line);

            rule->line_num = line_num;
            FT_LIST_ADD_LAST(&g_rules, rule);
        }
    }
    /* should never trigger */
    if (!delimiter_found)
    {
        line_num++;
        fprintf(stderr, "%s:%d: premature EOF\n", g_filename, line_num);
        fclose(fp);
        return EXIT_FAILURE;
    }

    /* ------------------------------DEBUG------------------------------ */
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

    // rewind(fp);
    // line_num = 0;
    /* ------------------------------DEBUGEND------------------------------ */

    get_rules_section(fp);

    /* ------------------------------DEBUG------------------------------ */
    printf("fooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo\n");
    r = g_rules_2;
    while (r)
    {
        printf("Rule: '%s' -> '%s'\n", r->name, r->pattern);
        r = FT_LIST_GET_NEXT(&g_rules_2, r);
    }
    /* ------------------------------DEBUGEND------------------------------ */

    fclose(fp);
    return EXIT_SUCCESS;
}
