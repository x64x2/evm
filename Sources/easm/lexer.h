#ifndef __DASM_LEXER_H__
#define __DASM_LEXER_H__

#include <stdio.h>

void gettoken(FILE *f);
extern int lex_line;

extern const char *lextoken;

#endif
