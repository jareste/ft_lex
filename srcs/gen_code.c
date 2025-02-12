
int gen_code(char* filename)
{
    FILE* in = fopen(filename, "r");
    if (!in)
    {
        perror("Error opening file");
        return EXIT_FAILURE;
    }
    char* target = "ft_lex.yy.c";
    FILE* out = fopen(target, "w");
    if (!out)
    {
        perror("Error opening file");
        return EXIT_FAILURE;
    }
    char line[MAX_LINE_LENGTH];
    while (fgets(line, MAX_LINE_LENGTH, in))
    {
        if (strstr(line, "%%"))
        {
            fprintf(out, "int yylex(void)\n{\n");
            break;
        }
    }
}