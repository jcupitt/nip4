// shim for parser

#include "nip4.h"

jmp_buf parse_error_point;

YYSTYPE yylval;

VipsBuf lex_text;

void
attach_input_string(const char *str)
{
}

void
free_lex(int yychar)
{
}

int
yylex(void)
{
	return 0;
}
