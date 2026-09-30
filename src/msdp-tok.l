%{
	#include <stdlib.h>
	#include <stdio.h>
	#include <string.h>
	#include "msdp-tok.h"
%}

%option noyywrap
%option prefix="msdp"

%%
[\[\{}\]=]		return 1;
[a-zA-Z0-9_]*		return 1;
[ \t]*			return 1;
\"(\\.|[^\\"])*\"	return 1;
%%

void msdp_squelch(void) {
	(void)yyunput;
	(void)input;
}

const char **msdp_tok(const char *buf) {
	int csize = 10;
	int cpos = 0;
	char *c;
	char **ret = malloc(sizeof(*ret) * csize);

	msdp_scan_string(buf);
	while (msdplex()) {
		c = yytext;
		if (cpos >= csize - 1) {
			csize += 10;
			ret = realloc(ret, sizeof(*ret) * csize);
		}
		ret[cpos++] = strdup(c);
	}
	ret[cpos] = 0;
	msdp_delete_buffer(YY_CURRENT_BUFFER);
	return (const char **) ret;
}

void msdp_tok_free(const char **c) {
	const char **r = c;
	while (*r)
		free((void *)*r++);
	free(c);
}
