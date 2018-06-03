/*
 * html2tf.c - convert limited html format to tf helpfile format.
 * Copyright 1996, 1997, 1998, 1999, 2002, 2003, 2005, 2006-2007 Ken Keys
 *
 * Usage: html2tf FILE...
 * Converts each FILE and prints the result on stdout. 
 *
 * <!-- END -->		Everything after this is ignored.
 * <!--"@...">		Converted to "&..." on a line by itself (tf topic).
 * <a name="...">	Converted to "#..." on a line by itself (tf subtopic).
 * <a href=...>...</a>	Bold.
 * <title>...</title>	Title.
 * <h1>...</h1>		Header 1.
 * <h2>...</h2>		Header 2.
 * <h3>...</h3>		Header 3.
 * <hr>			Horizontal rule.
 * <br>			Line break.
 * <p>			Paragraph.
 * <b>...</b>		Bold.
 * <u>...</u>		Underlined.
 * <strong>...</strong>	Bold reverse.
 * <i>...</i>		"<...>", underlined (italics in html).
 * <var>...</var>	Underline (italics in html).
 * <em>...</em>		Underline (usually italics in html).
 * <span style="color: ...">		Start foreground text color.
 * <span style="background: ...">	Start background text color.
 * <pre>...</pre>	Preformatted.
 * <dl>...</dl>		Definition list ("compact" attribute is assumed).
 * <dt>			<dl> term.
 * <dd>			<dl> definition.
 * <dir>...</dir>	Directory in columns (can't nest; used by command index)
 * <ul>...</ul>		Unordered list.
 * <li>			List item for <dir> or <ul>.
 * All other tags are ignored.
 * &amp;		"&".
 * &lt;			"<".
 * &gt;			">".
 * &copy;		"(C)".
 * &nbsp;		" ".
 * &#160;		" ".
 * All other character entities are errors.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define addc(c)			(word[wordvis++,wordlen++] = c)
#define precmp(str, literal)	strncasecmp(str, literal, sizeof(literal) - 1)

#define MAX_LINE_LEN		78
#define LIST_INDENT		4
#define MARGIN			2

#define PARTIAL			1

char word[4096], tag[4096], attr[4096];
int linelen, linevis, wordlen, wordvis, taglen, attrlen;
int spaces, newlines, margin, indent, pre, list, ignore;
int pendingindent = 0;

struct dir_s {
    char word[64];
    int vis;
} dir[1024];

int dirflag = 0, dircount = 0;

void putword(void);
void putnewline(int n, int flag);
void adds(char *s);

int main(int argc, char *argv[])
{
    FILE *file;
    int c, i, j;
    char *s, *t;
    long line;
    int inlink, under, bold, inquote, inattr, iscomment, defwidth, dt;

    while (*++argv) {
        line = 1;
        if (!(file = fopen(*argv, "r"))) {
            perror(*argv);
            exit(1);
        }
        linelen = 0;
        wordlen = 0;
        taglen = 0;
        attrlen = 0;
        spaces = 0;
        newlines = 1;
        indent = 0;
        margin = MARGIN;
        pre = 0;
        list = 0;
        under = bold = inlink = 0;
        while ((c = getc(file)) != EOF) {
            if (c == '<') {
                taglen = 0;
                attrlen = 0;
                inattr = 0;
                inquote = 0;
                iscomment = 0;
		continuetag:
                while ((c = getc(file)) != EOF) {
                    if (taglen == 0 && c == '!') iscomment = 1;
                    if (c == '"') inquote = !inquote;
                    if (!inquote && c == '>') break;
                    if (!inquote && !iscomment && isspace(c))
                        { inattr = 1; continue; }
                    if (!inattr)
                        tag[taglen++] = c;
                    else
                        attr[attrlen++] = c;
                }
                tag[taglen] = '\0';
                attr[attrlen] = '\0';
		if (iscomment) {
		    if (taglen < 5 || strcmp(tag + taglen - 2, "--") != 0)
			goto continuetag;
		}
                if (strcasecmp(tag, "!-- END --") == 0) {
                    break;
                } else if (precmp(tag, "!--\"@") == 0) {
                    putnewline(1, 0);
                    s = tag + 5;
                    for (t = s; *t && *t != '"'; t++);
                    *t = '\0';
                    fputc('&', stdout);
                    fputs(s, stdout);
                    fputc('\n', stdout);
                } else if (strcasecmp(tag, "span") == 0) {
                    if (precmp(attr, "style=\"color: ") == 0) {
                        s = attr + 14;
                        for (t = s; *t && *t != '"'; t++);
                        *t = '\0';
                        if (strcasecmp(s, "black") == 0)
                            strcpy(word+wordlen, "\033[30m"), wordlen +=5;
                        else if (strcasecmp(s, "red") == 0)
                            strcpy(word+wordlen, "\033[31m"), wordlen +=5;
                        else if (strcasecmp(s, "green") == 0)
                            strcpy(word+wordlen, "\033[32m"), wordlen +=5;
                        else if (strcasecmp(s, "yellow") == 0)
                            strcpy(word+wordlen, "\033[33m"), wordlen +=5;
                        else if (strcasecmp(s, "blue") == 0)
                            strcpy(word+wordlen, "\033[34m"), wordlen +=5;
                        else if (strcasecmp(s, "magenta") == 0)
                            strcpy(word+wordlen, "\033[35m"), wordlen +=5;
                        else if (strcasecmp(s, "cyan") == 0)
                            strcpy(word+wordlen, "\033[36m"), wordlen +=5;
                        else if (strcasecmp(s, "white") == 0)
                            strcpy(word+wordlen, "\033[37m"), wordlen +=5;
                    }
                    if (precmp(attr, "style=\"background: ") == 0) {
                        s = attr + 19;
                        for (t = s; *t && *t != '"'; t++);
                        *t = '\0';
                        if (strcasecmp(s, "black") == 0)
                            strcpy(word+wordlen, "\033[40m"), wordlen +=5;
                        else if (strcasecmp(s, "red") == 0)
                            strcpy(word+wordlen, "\033[41m"), wordlen +=5;
                        else if (strcasecmp(s, "green") == 0)
                            strcpy(word+wordlen, "\033[42m"), wordlen +=5;
                        else if (strcasecmp(s, "yellow") == 0)
                            strcpy(word+wordlen, "\033[43m"), wordlen +=5;
                        else if (strcasecmp(s, "blue") == 0)
                            strcpy(word+wordlen, "\033[44m"), wordlen +=5;
                        else if (strcasecmp(s, "magenta") == 0)
                            strcpy(word+wordlen, "\033[45m"), wordlen +=5;
                        else if (strcasecmp(s, "cyan") == 0)
                            strcpy(word+wordlen, "\033[46m"), wordlen +=5;
                        else if (strcasecmp(s, "white") == 0)
                            strcpy(word+wordlen, "\033[47m"), wordlen +=5;
                    }
                } else if (strcasecmp(tag, "/span") == 0) {
                    strcpy(word+wordlen, "\033[0m"), wordlen +=4;
#if PARTIAL
                } else if (strcasecmp(tag, "a") == 0) {
                    if (precmp(attr, "href=\"") == 0) {
                        if (attr[attrlen-1] != '"') {
                            fprintf(stderr, "%s, %d: missing \"\n",
                                argv[0], line);
                            exit(1);
                        }
                        if (!bold++) {
# if 0
                            strcpy(word+wordlen, "\033[1;35m");
                            wordlen += 7;
# else
                            strcpy(word+wordlen, "\033[1m");
                            wordlen += 4;
# endif
                        }
                        inlink=1;
                    } else if (precmp(attr, "name=\"") == 0) {
                        putnewline(1, 0);
                        s = attr + 6;
                        for (t = s; *t && *t != '"'; t++);
                        *t = '\0';
                        putc('#', stdout);
                        fputs(s, stdout);
                        newlines = 0;
                        putnewline(1, 0);
                    }
                } else if (precmp(tag, "/a") == 0) {
                    if (inlink) {
                        inlink=0;
                        if (!--bold) {
                            strcpy(word+wordlen, "\033[22;0m");
                            wordlen += 7;
                        }
                    }
#endif
                } else if (strcasecmp(tag, "h1") == 0) {
                    putnewline(2, 0);
                    margin = 0;
                } else if (strcasecmp(tag, "/h1") == 0) {
                    putnewline(2, 0);
                    margin = MARGIN;
                } else if (strcasecmp(tag, "h2") == 0) {
                    putnewline(2, 0);
                    margin = 0;
                } else if (strcasecmp(tag, "/h2") == 0) {
                    putnewline(2, 0);
                    margin = MARGIN;
                } else if (strcasecmp(tag, "h3") == 0) {
                    putnewline(1, 0);
                    margin = 0;
                } else if (strcasecmp(tag, "/h3") == 0) {
                    putnewline(1, 0);
                    margin = MARGIN;
                } else if (strcasecmp(tag, "title") == 0) {
                    ignore++;
                } else if (strcasecmp(tag, "/title") == 0) {
                    ignore--;
                } else if (strcasecmp(tag, "hr") == 0) {
                    putnewline(1, 0);
                    for (i = 0; i < margin; i++)
                        fputc(' ', stdout);
                    for ( ; i < MAX_LINE_LEN; i++)
                        fputc('_', stdout);
                    newlines = 0;
                    putnewline(2, 0);
                } else if (strcasecmp(tag, "br") == 0) {
                    putnewline(1, 1);
                } else if (strcasecmp(tag, "p") == 0) {
                    putnewline(2, 0);
                } else if (strcasecmp(tag, "i") == 0) {
                    addc('<');
#if PARTIAL
                    if (!under++) {
                        strcpy(word+wordlen, "\033[4m");
                        wordlen += 4;
                    }
#endif
                } else if (strcasecmp(tag, "/i") == 0) {
#if PARTIAL
                    if (!--under) {
                        strcpy(word+wordlen, "\033[24m");
                        wordlen += 5;
                    }
#endif
                    addc('>');
#if PARTIAL
                } else if (strcasecmp(tag, "b") == 0) {
                    if (!bold++) {
                        strcpy(word+wordlen, "\033[1m");
                        wordlen += 4;
                    }
                } else if (strcasecmp(tag, "/b") == 0) {
                    if (!--bold) {
                        strcpy(word+wordlen, "\033[22m");
                        wordlen += 5;
                    }
                } else if (strcasecmp(tag, "u") == 0) {
                    if (!under++) {
                        strcpy(word+wordlen, "\033[4m");
                        wordlen += 4;
                    }
                } else if (strcasecmp(tag, "/u") == 0) {
                    if (!--under) {
                        strcpy(word+wordlen, "\033[24m");
                        wordlen += 5;
                    }
                } else if (strcasecmp(tag, "em") == 0) {
                    if (!under++) {
                        strcpy(word+wordlen, "\033[4m");
                        wordlen += 4;
                    }
                } else if (strcasecmp(tag, "/em") == 0) {
                    if (!--under) {
                        strcpy(word+wordlen, "\033[24m");
                        wordlen += 5;
                    }
                } else if (strcasecmp(tag, "var") == 0) {
                    if (!under++) {
                        strcpy(word+wordlen, "\033[4m");
                        wordlen += 4;
                    }
                } else if (strcasecmp(tag, "/var") == 0) {
                    if (!--under) {
                        strcpy(word+wordlen, "\033[24m");
                        wordlen += 5;
                    }
                } else if (strcasecmp(tag, "strong") == 0) {
                    if (!under++) {
                        strcpy(word+wordlen, "\033[1;7m");
                        wordlen += 6;
                    }
                } else if (strcasecmp(tag, "/strong") == 0) {
                    if (!--under) {
                        strcpy(word+wordlen, "\033[22;27m");
                        wordlen += 8;
                    }
#endif
                } else if (strcasecmp(tag, "pre") == 0) {
                    pre = 1;
                    putnewline(1, 0);
                    /* fputs("<pre>", stdout); */
                } else if (strcasecmp(tag, "/pre") == 0) {
                    pre = 0;
                    putnewline(2, 0);
                    /* fputs("</pre>", stdout); */
                } else if (strcasecmp(tag, "dl") == 0) {
                    /* list += 2; */
                    putnewline(1, 0);
                    defwidth = 7;
                    indent += defwidth + 1;
                    dt = 0;
                } else if (precmp(tag, "!-- defwidth=") == 0) {
                    indent -= defwidth + 1;
                    defwidth = atoi(tag + 13);
                    indent += defwidth + 1;
                } else if (strcasecmp(tag, "dt") == 0) {
                    if (!dt)
                        indent += pendingindent - (defwidth + 1);
                    dt++;
                    pendingindent = 0;
                    putnewline(1, 0);
                    putword();
                } else if (strcasecmp(tag, "dd") == 0) {
                    dt = 0;
                    putword();
                    pendingindent = defwidth + 1;
                    if (linevis >= pendingindent) {
                        putnewline(1, 0);
                    } else {
                        while (linevis < pendingindent) {
                            fputc(' ', stdout);
                            linelen++;
                            linevis++;
                        }
                    }
                } else if (strcasecmp(tag, "/dl") == 0) {
                    indent += pendingindent - (defwidth + 1);
                    /* defwidth = 0; */
                    pendingindent = 0;
                    /* list -= 2; */
                    putnewline(1, 0);
                } else if (strcasecmp(tag, "dir") == 0) {
                    putword();
                    dirflag++;
                    dircount = -1;
                    putnewline(2, 0);
                } else if (strcasecmp(tag, "/dir") == 0) {
                    int maxvis, rows, cols;
                    strncpy(dir[dircount].word, word, wordlen);
                    dir[dircount].word[wordlen] = '\0';
                    dir[dircount].vis = wordvis;
                    dircount++;
                    wordlen = wordvis = 0;
                    dirflag--;
                    /* fprintf(stderr, "found %d dir items\n", dircount); */
                    for (maxvis = 0, i = 0; i < dircount; i++) {
                        /* fprintf(stderr, "%3d \"%s\"\n", len, dir[i].vis); */
                        if (dir[i].vis > maxvis) maxvis = dir[i].vis;
                    }
                    /* fprintf(stderr, "longest = %d\n", maxvis); */
                    /* maxvis += 1; */
                    cols = (MAX_LINE_LEN - margin - indent) / maxvis;
                    /* fprintf(stderr, "cols = %d\n", cols); */
                    rows = (dircount - 1) / cols + 1;
                    /* fprintf(stderr, "rows = %d\n", rows); */
                    for (i = 0; i < rows; i++) {
                        printf("%*s", margin + indent, "");
                        for (j = 0; j < cols; j++)
                            printf("%s%*s", dir[j*rows+i].word,
                                maxvis - dir[j*rows+i].vis, "");
                        fputc('\n', stdout);
                    }
                    newlines = 1;
                    indent = LIST_INDENT * list;
                    putnewline(2, 0);
                } else if (strcasecmp(tag, "ul") == 0) {
                    list++;
                    putnewline(2, 0);
                } else if (strcasecmp(tag, "/ul") == 0) {
                    list--;
                    indent = LIST_INDENT * list;
                    putnewline(2, 0);
                } else if (strcasecmp(tag, "ol") == 0) {
                    list++;
                    putnewline(2, 0);
                } else if (strcasecmp(tag, "/ol") == 0) {
                    list--;
                    indent = LIST_INDENT * list;
                    putnewline(2, 0);
                } else if (strcasecmp(tag, "li") == 0) {
                    if (dirflag) {
                        if (dircount >= 0) {
                            strncpy(dir[dircount].word, word, wordlen);
                            dir[dircount].word[wordlen] = '\0';
                            dir[dircount].vis = wordvis;
                        }
                        dircount++;
                        wordlen = wordvis = 0;
                    } else {
                        putword();
                        putnewline(1, 0);
                        indent = LIST_INDENT * (list - 1);
                        adds("  * ");
                        putword();
                        spaces = 1;
                        indent = LIST_INDENT * list;
                    }
                }
            } else if (ignore) {
                /* ignore */
            } else if (c == '&') {
                taglen = 0;
                attrlen = 0;
                while ((c = getc(file)) != EOF) {
                    if (c == ';') break;
                    tag[taglen++] = c;
                }
                tag[taglen] = '\0';
                if (strcasecmp(tag, "amp") == 0) {
                    addc('&');
                } else if (strcasecmp(tag, "lt") == 0) {
                    addc('<');
                } else if (strcasecmp(tag, "gt") == 0) {
                    addc('>');
                } else if (strcasecmp(tag, "copy") == 0) {
                    adds("(C)");
                } else if (strcasecmp(tag, "nbsp") == 0) {
                    addc(' ');
                    spaces++;
                } else if (strcasecmp(tag, "#160") == 0) {
                    addc(' ');
                    spaces++;
                } else if (tag[0] == '#') {
                    addc(atoi(tag+1));
                } else {
                    fprintf(stderr, "%s, %d: Unknown token &%s;\n",
                        argv[0], line, tag);
                    exit(1);
                }
            } else if (pre) {
                if (c == '\n') {
                    putnewline(1, 1);
                    line++;
                } else if (c == '\t') {
                    do {
                        addc(' ');
                    } while (wordvis % 8);
                    newlines = 0;
                } else {
                    addc(c);
                    newlines = 0;
                }
            } else if (isspace(c)) {
                putword();
                if (c == '\n') line++;
                if ((linevis || dirflag) && !spaces) {
                    if (dirflag) addc(' ');
                    else fputc(' ', stdout);
                    spaces++;
                    linelen++;
                    linevis++;
                }
            } else {
                spaces = 0;
                addc(c);
            }
        }
        fclose(file);
        putnewline(1, 0);
    }
    fputs("&\n", stdout);
    return 0;
}

void putword(void)
{
    int i;

    if (wordlen == 0 || dirflag) return;
    word[wordlen] = '\0';
    if (!pre && !newlines && margin + indent + linevis + wordvis > MAX_LINE_LEN)
    {
        linelen = linevis = 0;
        spaces = 0;
        newlines++;
        fputc('\n', stdout);
        indent += pendingindent;
        pendingindent = 0;
    }
    if (linelen == 0) {
        for (i = 0; i < indent + margin; i++)
            fputc(' ', stdout);
    }
    fputs(word, stdout);
    linelen += wordlen;
    linevis += wordvis;
    if (word[wordlen - 1] == '.' && !pre &&
        margin + indent + linevis < MAX_LINE_LEN)
    {
        fputc(' ', stdout);
        linelen++;
        linevis++;
    }
    wordlen = wordvis = 0;
    spaces = 0;
    newlines = 0;
}

/* flag: if 1, print newline(s) unconditionally; if 0, print newline(s) as
 * needed to bring number of visible newlines up to n.
 */
void putnewline(int n, int flag)
{
    putword();
    if (!flag)
        n -= newlines;
    linelen = linevis = 0;
    spaces = 0;
    if (n <= 0) return;
    newlines += n;
    while (n--)
        fputc('\n', stdout);
    indent += pendingindent;
    pendingindent = 0;
}

void adds(char *s)
{
   for ( ; *s; s++)
       addc(*s);
}
