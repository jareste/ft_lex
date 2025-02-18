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

char *find_matching_brace(char *start)
{
    if (*start != '{')
        return NULL;
    int brace = 0;
    char *p = start;
    while (*p)
    {
        if (*p == '{')
            brace++;
        else if (*p == '}')
        {
            brace--;
            if (brace == 0)
                return p;
        }
        p++;
    }
    return NULL;
}

void get_rules_section(FILE *fp)
{
    char line[MAX_LINE_LENGTH];
    char rule_buffer[8192] = "";
    
    while (fgets(line, MAX_LINE_LENGTH, fp))
    {
        line_num++;
        if (is_delimiter_line(line))
            break;
        
        char *trimmed = trim(line);
        if (*trimmed == '\0')
            continue;
        
        if (strlen(rule_buffer) > 0)
            strcat(rule_buffer, " ");
        strcat(rule_buffer, trimmed);
        
        /* If the current trimmed line ends with a '|' then we expect more alternatives.
         * Remove the trailing '|' and continue accumulating.
         */
        int len = strlen(trimmed);
        if (len > 0 && trimmed[len - 1] == '|')
        {
            rule_buffer[strlen(rule_buffer) - 1] = '\0';
            continue;
        }
        
        /* Check if the rule is complete.
         * If a block action is present (detected by a '{'), count the braces.
         * Only when they balance (brace_count==0) do we consider the rule complete.
         * Otherwise, if there’s a semicolon or comment marker, we assume it’s complete.
         */
        int complete = 0;
        char *first_brace = strchr(rule_buffer, '{');
        if (first_brace)
        {
            int brace_count = 0;
            for (char *p = first_brace; *p; p++)
            {
                if (*p == '{')
                    brace_count++;
                else if (*p == '}')
                    brace_count--;
            }
            if (brace_count != 0)
                continue;  /* still in a multiline action */
            complete = 1;
        }
        else if (strchr(rule_buffer, ';'))
        {
            complete = 1;
        }
        else if (strstr(rule_buffer, "//") || strstr(rule_buffer, "/*"))
        {
            complete = 1;
        }
        else
        {
            complete = 1;  /* default to complete */
        }
        
        if (!complete)
            continue;
        
        /* Now rule_buffer holds a complete rule.
         * We need to split it into its components.
         * If the rule starts with a macro expansion (i.e. first character is '{'),
         * then extract the macro’s contents as the rule name.
         */
        if (rule_buffer[0] == '{')
        {
            /* Find the matching closing brace for the macro expansion */
            char *macro_end = find_matching_brace(rule_buffer);
            if (!macro_end)
            {
                fprintf(stderr, "%s:%d: missing closing '}' for macro in rule: %s\n",
                        g_filename, line_num, rule_buffer);
                exit(EXIT_FAILURE);
            }
            
            int macro_name_len = macro_end - rule_buffer + 1;
            char *macro_name = malloc(macro_name_len + 1);
            strncpy(macro_name, rule_buffer, macro_name_len);
            macro_name[macro_name_len] = '\0';
            
            /* After the macro expansion, there may be additional regex text.
             * In many lex implementations the regular expression is not multiline;
             * so we assume here that the remainder (if any) is not needed.
             * We now look for the action delimiter.
             */
            char *after_macro = trim(macro_end + 1);
            /* Look for the start of the action block: a '{' (or a semicolon/comment) */
            char *action_delim = strchr(after_macro, '{');
            if (!action_delim)
                action_delim = strchr(after_macro, ';');
            if (!action_delim)
            {
                char *comment = strstr(after_macro, "//");
                if (!comment)
                    comment = strstr(after_macro, "/*");
                action_delim = comment;
            }
            if (!action_delim)
            {
                fprintf(stderr, "%s:%d: missing action delimiter in rule: %s\n", 
                        g_filename, line_num, rule_buffer);
                exit(EXIT_FAILURE);
            }
            
            /* Extract the action.
             * If the delimiter is '{', then we have a block action.
             */
            char *action = NULL;
            if (*action_delim == '{')
            {
                action = strdup(trim(action_delim + 1));
                /* Remove trailing '}' from the action, if present */
                char *end_brace = strrchr(action, '}');
                if (end_brace)
                    *end_brace = '\0';
            }
            else
            {
                /* If the delimiter is a semicolon or comment, assume an empty action */
                *action_delim = '\0';
                action = strdup("");
            }
            
            /* Create the rule using the macro content as the name.
             * (Here, rule->name stores the macro expansion; rule->pattern stores the action.)
             */
            LexRule *rule = malloc(sizeof(LexRule));
            rule->name = macro_name;
            rule->pattern = action;
            rule->line_num = line_num;
            FT_LIST_ADD_LAST(&g_rules_2, rule);
        }
        else
        {
            /* Normal rule that does not start with a macro expansion.
             * Split at the first '{' (or semicolon/comment) to separate pattern and action.
             */
            char *action_start = strchr(rule_buffer, '{');
            if (action_start)
            {
                *action_start = '\0';
                char *pattern = strdup(trim(rule_buffer));
                char *action = strdup(trim(action_start + 1));
                char *end_brace = strrchr(action, '}');
                if (end_brace)
                    *end_brace = '\0';
                LexRule *rule = malloc(sizeof(LexRule));
                rule->name = pattern;
                rule->pattern = action;
                rule->line_num = line_num;
                FT_LIST_ADD_LAST(&g_rules_2, rule);
            }
            else
            {
                char *semi = strchr(rule_buffer, ';');
                if (!semi)
                {
                    char *comment = strstr(rule_buffer, "//");
                    if (!comment)
                        comment = strstr(rule_buffer, "/*");
                    if (comment)
                    {
                        *comment = '\0';
                        char *pattern = strdup(trim(rule_buffer));
                        char *action = strdup("");
                        LexRule *rule = malloc(sizeof(LexRule));
                        rule->name = pattern;
                        rule->pattern = action;
                        rule->line_num = line_num;
                        FT_LIST_ADD_LAST(&g_rules_2, rule);
                    }
                    else
                    {
                        fprintf(stderr, "%s:%d: missing action delimiter in rule: %s\n", 
                                g_filename, line_num, rule_buffer);
                        exit(EXIT_FAILURE);
                    }
                }
                else
                {
                    *semi = '\0';
                    char *p = rule_buffer;
                    while (*p && !isspace((unsigned char)*p))
                        p++;
                    if (*p)
                    {
                        *p = '\0';
                        p++;
                    }
                    char *pattern = strdup(trim(rule_buffer));
                    char *action = strdup(trim(p));
                    LexRule *rule = malloc(sizeof(LexRule));
                    rule->name = pattern;
                    rule->pattern = action;
                    rule->line_num = line_num;
                    FT_LIST_ADD_LAST(&g_rules_2, rule);
                }
            }
        }
        
        /* Clear rule_buffer for the next rule */
        rule_buffer[0] = '\0';
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
