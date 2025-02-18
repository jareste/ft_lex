%{
#include <stdio.h>

#include <string.h>
%}
%deff
/* Comment */
DIGIT [0-9]
d [a-z]

%{
%}
%%

{DIGIT}+        { printf("NUMBER: %s\n", yytext); }
[a-zA-Z]+       { printf("WORD: %s\n", yytext); }
\n              { printf("NEWLINE\n"); }
[ \t]+          ;  /* Ignore whitespace */
.               { printf("UNKNOWN: %s\n", yytext); }

%%

int main() {
    yylex();  // Start scanning input
    return 0;
}