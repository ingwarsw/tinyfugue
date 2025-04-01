%{
	#include <stdlib.h>
	#include <stdio.h>
	#include "msdp-tok.h"
%}	

%option noyywrap
%x qstr

%%
[\[\{}\]=]		return 1;
[a-zA-Z0-9_]*		return 1;
[ \t]*			return 1;
\"(\\.|[^\\"])*\"	return 1;
%%

void squelch() {
	(void)yyunput;
	(void)input;
}

const char ** msdp_tok(const char * buf) {
	int csize=10;
	int cpos=0;
	char * c;
	char ** ret=malloc(sizeof(*ret)*csize);

	msdp_scan_string(buf);
	while(msdplex()) {
		c=yytext;
		if (cpos>=csize-1) {
			csize += 10;
			ret=realloc(ret, sizeof(*ret)*csize);
		}
		ret[cpos++]=strdup(c);
	}
	ret[cpos]=0;
	msdp_delete_buffer(YY_CURRENT_BUFFER);
	return (const char **) ret;
}

void msdp_tok_free(const char ** c) {
	const char ** r=c;
	while(*r)
		free((void *)*r++);
	free(c);
}
		
#ifdef MSDP_PARSE_TEST
#include <sys/resource.h>

int main() {
//	char buf[]="TEST=\"1 2 3\" FOO [{}]";
	const char buf[] = "NAME=\"DAN\" ROOM=[ NAME=\"The void\" COORDINATES=[ X=\"0\" Y=\"0\" Z=\"0\" ] ITEMS={ \"FOO\" \"BAR\" \"BAZ\" } ] HP=\"32\"";
	const char ** foo=0;
	const char **r=foo;
	int i;
	struct rusage a, b;
	foo=msdp_tok(buf);
	getrusage(RUSAGE_SELF, &a);
	for(i=0;i<10000;i++) {
		if (foo)
			msdp_tok_free(foo);
		foo=msdp_tok(buf);
	}
	getrusage(RUSAGE_SELF, &b);
#define YY(y)	(b.ru_##y - a.ru_##y)
#define XX(x)	printf(#x": %ld\n", YY(x))
	XX(maxrss);
	XX(ixrss);
	XX(idrss);
	XX(isrss);
	XX(minflt);
	XX(majflt);
	XX(nswap);
	XX(nvcsw);
	XX(nivcsw);

	r=foo;
	while(*r)
		printf("|%s\n", *r++);
	msdp_tok_free(foo);
	return 0;
}
#endif
