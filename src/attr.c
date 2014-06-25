/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
static const char RCSid[] = "$Id: attr.c,v 35004.10 2007/01/13 23:12:39 kkeys Exp $";

#include "tfconfig.h"
#include "port.h"
#include "tf.h"
#include "util.h"
#include "pattern.h"	/* for tfio.h */
#include "search.h"
#include "tfio.h"
#include "output.h"
#include "attr.h"
#include "variable.h"
#include "parse.h"	/* valstd() */


const int feature_256colors = (NCOLORS == 256);

/* gagged labels will be elided in the "valid values" list of enum2int() */
conString enum_color[]= {
    STRING_LITERAL("black"),
    STRING_LITERAL("red"),
    STRING_LITERAL("green"),
    STRING_LITERAL("yellow"),
    STRING_LITERAL("blue"),
    STRING_LITERAL("magenta"),
    STRING_LITERAL("cyan"),
    STRING_LITERAL("white"),

    STRING_LITERAL("gray"),
    STRING_LITERAL("brightred"),
    STRING_LITERAL_ATTR("brightgreen", F_GAG),
    STRING_LITERAL_ATTR("brightyellow", F_GAG),
    STRING_LITERAL_ATTR("brightblue", F_GAG),
    STRING_LITERAL_ATTR("brightmagenta", F_GAG),
    STRING_LITERAL_ATTR("brightcyan", F_GAG),
    STRING_LITERAL("brightwhite"),

#if NCOLORS == 256
    STRING_LITERAL("rgb000"),
    STRING_LITERAL_ATTR("rgb001", F_GAG),
    STRING_LITERAL_ATTR("rgb002", F_GAG),
    STRING_LITERAL_ATTR("rgb003", F_GAG),
    STRING_LITERAL_ATTR("rgb004", F_GAG),
    STRING_LITERAL_ATTR("rgb005", F_GAG),
    STRING_LITERAL_ATTR("rgb010", F_GAG),
    STRING_LITERAL_ATTR("rgb011", F_GAG),
    STRING_LITERAL_ATTR("rgb012", F_GAG),
    STRING_LITERAL_ATTR("rgb013", F_GAG),
    STRING_LITERAL_ATTR("rgb014", F_GAG),
    STRING_LITERAL_ATTR("rgb015", F_GAG),
    STRING_LITERAL_ATTR("rgb020", F_GAG),
    STRING_LITERAL_ATTR("rgb021", F_GAG),
    STRING_LITERAL_ATTR("rgb022", F_GAG),
    STRING_LITERAL_ATTR("rgb023", F_GAG),
    STRING_LITERAL_ATTR("rgb024", F_GAG),
    STRING_LITERAL_ATTR("rgb025", F_GAG),
    STRING_LITERAL_ATTR("rgb030", F_GAG),
    STRING_LITERAL_ATTR("rgb031", F_GAG),
    STRING_LITERAL_ATTR("rgb032", F_GAG),
    STRING_LITERAL_ATTR("rgb033", F_GAG),
    STRING_LITERAL_ATTR("rgb034", F_GAG),
    STRING_LITERAL_ATTR("rgb035", F_GAG),
    STRING_LITERAL_ATTR("rgb040", F_GAG),
    STRING_LITERAL_ATTR("rgb041", F_GAG),
    STRING_LITERAL_ATTR("rgb042", F_GAG),
    STRING_LITERAL_ATTR("rgb043", F_GAG),
    STRING_LITERAL_ATTR("rgb044", F_GAG),
    STRING_LITERAL_ATTR("rgb045", F_GAG),
    STRING_LITERAL_ATTR("rgb050", F_GAG),
    STRING_LITERAL_ATTR("rgb051", F_GAG),
    STRING_LITERAL_ATTR("rgb052", F_GAG),
    STRING_LITERAL_ATTR("rgb053", F_GAG),
    STRING_LITERAL_ATTR("rgb054", F_GAG),
    STRING_LITERAL_ATTR("rgb055", F_GAG),
    STRING_LITERAL_ATTR("rgb100", F_GAG),
    STRING_LITERAL_ATTR("rgb101", F_GAG),
    STRING_LITERAL_ATTR("rgb102", F_GAG),
    STRING_LITERAL_ATTR("rgb103", F_GAG),
    STRING_LITERAL_ATTR("rgb104", F_GAG),
    STRING_LITERAL_ATTR("rgb105", F_GAG),
    STRING_LITERAL_ATTR("rgb110", F_GAG),
    STRING_LITERAL_ATTR("rgb111", F_GAG),
    STRING_LITERAL_ATTR("rgb112", F_GAG),
    STRING_LITERAL_ATTR("rgb113", F_GAG),
    STRING_LITERAL_ATTR("rgb114", F_GAG),
    STRING_LITERAL_ATTR("rgb115", F_GAG),
    STRING_LITERAL_ATTR("rgb120", F_GAG),
    STRING_LITERAL_ATTR("rgb121", F_GAG),
    STRING_LITERAL_ATTR("rgb122", F_GAG),
    STRING_LITERAL_ATTR("rgb123", F_GAG),
    STRING_LITERAL_ATTR("rgb124", F_GAG),
    STRING_LITERAL_ATTR("rgb125", F_GAG),
    STRING_LITERAL_ATTR("rgb130", F_GAG),
    STRING_LITERAL_ATTR("rgb131", F_GAG),
    STRING_LITERAL_ATTR("rgb132", F_GAG),
    STRING_LITERAL_ATTR("rgb133", F_GAG),
    STRING_LITERAL_ATTR("rgb134", F_GAG),
    STRING_LITERAL_ATTR("rgb135", F_GAG),
    STRING_LITERAL_ATTR("rgb140", F_GAG),
    STRING_LITERAL_ATTR("rgb141", F_GAG),
    STRING_LITERAL_ATTR("rgb142", F_GAG),
    STRING_LITERAL_ATTR("rgb143", F_GAG),
    STRING_LITERAL_ATTR("rgb144", F_GAG),
    STRING_LITERAL_ATTR("rgb145", F_GAG),
    STRING_LITERAL_ATTR("rgb150", F_GAG),
    STRING_LITERAL_ATTR("rgb151", F_GAG),
    STRING_LITERAL_ATTR("rgb152", F_GAG),
    STRING_LITERAL_ATTR("rgb153", F_GAG),
    STRING_LITERAL_ATTR("rgb154", F_GAG),
    STRING_LITERAL_ATTR("rgb155", F_GAG),
    STRING_LITERAL_ATTR("rgb200", F_GAG),
    STRING_LITERAL_ATTR("rgb201", F_GAG),
    STRING_LITERAL_ATTR("rgb202", F_GAG),
    STRING_LITERAL_ATTR("rgb203", F_GAG),
    STRING_LITERAL_ATTR("rgb204", F_GAG),
    STRING_LITERAL_ATTR("rgb205", F_GAG),
    STRING_LITERAL_ATTR("rgb210", F_GAG),
    STRING_LITERAL_ATTR("rgb211", F_GAG),
    STRING_LITERAL_ATTR("rgb212", F_GAG),
    STRING_LITERAL_ATTR("rgb213", F_GAG),
    STRING_LITERAL_ATTR("rgb214", F_GAG),
    STRING_LITERAL_ATTR("rgb215", F_GAG),
    STRING_LITERAL_ATTR("rgb220", F_GAG),
    STRING_LITERAL_ATTR("rgb221", F_GAG),
    STRING_LITERAL_ATTR("rgb222", F_GAG),
    STRING_LITERAL_ATTR("rgb223", F_GAG),
    STRING_LITERAL_ATTR("rgb224", F_GAG),
    STRING_LITERAL_ATTR("rgb225", F_GAG),
    STRING_LITERAL_ATTR("rgb230", F_GAG),
    STRING_LITERAL_ATTR("rgb231", F_GAG),
    STRING_LITERAL_ATTR("rgb232", F_GAG),
    STRING_LITERAL_ATTR("rgb233", F_GAG),
    STRING_LITERAL_ATTR("rgb234", F_GAG),
    STRING_LITERAL_ATTR("rgb235", F_GAG),
    STRING_LITERAL_ATTR("rgb240", F_GAG),
    STRING_LITERAL_ATTR("rgb241", F_GAG),
    STRING_LITERAL_ATTR("rgb242", F_GAG),
    STRING_LITERAL_ATTR("rgb243", F_GAG),
    STRING_LITERAL_ATTR("rgb244", F_GAG),
    STRING_LITERAL_ATTR("rgb245", F_GAG),
    STRING_LITERAL_ATTR("rgb250", F_GAG),
    STRING_LITERAL_ATTR("rgb251", F_GAG),
    STRING_LITERAL_ATTR("rgb252", F_GAG),
    STRING_LITERAL_ATTR("rgb253", F_GAG),
    STRING_LITERAL_ATTR("rgb254", F_GAG),
    STRING_LITERAL_ATTR("rgb255", F_GAG),
    STRING_LITERAL_ATTR("rgb300", F_GAG),
    STRING_LITERAL_ATTR("rgb301", F_GAG),
    STRING_LITERAL_ATTR("rgb302", F_GAG),
    STRING_LITERAL_ATTR("rgb303", F_GAG),
    STRING_LITERAL_ATTR("rgb304", F_GAG),
    STRING_LITERAL_ATTR("rgb305", F_GAG),
    STRING_LITERAL_ATTR("rgb310", F_GAG),
    STRING_LITERAL_ATTR("rgb311", F_GAG),
    STRING_LITERAL_ATTR("rgb312", F_GAG),
    STRING_LITERAL_ATTR("rgb313", F_GAG),
    STRING_LITERAL_ATTR("rgb314", F_GAG),
    STRING_LITERAL_ATTR("rgb315", F_GAG),
    STRING_LITERAL_ATTR("rgb320", F_GAG),
    STRING_LITERAL_ATTR("rgb321", F_GAG),
    STRING_LITERAL_ATTR("rgb322", F_GAG),
    STRING_LITERAL_ATTR("rgb323", F_GAG),
    STRING_LITERAL_ATTR("rgb324", F_GAG),
    STRING_LITERAL_ATTR("rgb325", F_GAG),
    STRING_LITERAL_ATTR("rgb330", F_GAG),
    STRING_LITERAL_ATTR("rgb331", F_GAG),
    STRING_LITERAL_ATTR("rgb332", F_GAG),
    STRING_LITERAL_ATTR("rgb333", F_GAG),
    STRING_LITERAL_ATTR("rgb334", F_GAG),
    STRING_LITERAL_ATTR("rgb335", F_GAG),
    STRING_LITERAL_ATTR("rgb340", F_GAG),
    STRING_LITERAL_ATTR("rgb341", F_GAG),
    STRING_LITERAL_ATTR("rgb342", F_GAG),
    STRING_LITERAL_ATTR("rgb343", F_GAG),
    STRING_LITERAL_ATTR("rgb344", F_GAG),
    STRING_LITERAL_ATTR("rgb345", F_GAG),
    STRING_LITERAL_ATTR("rgb350", F_GAG),
    STRING_LITERAL_ATTR("rgb351", F_GAG),
    STRING_LITERAL_ATTR("rgb352", F_GAG),
    STRING_LITERAL_ATTR("rgb353", F_GAG),
    STRING_LITERAL_ATTR("rgb354", F_GAG),
    STRING_LITERAL_ATTR("rgb355", F_GAG),
    STRING_LITERAL_ATTR("rgb400", F_GAG),
    STRING_LITERAL_ATTR("rgb401", F_GAG),
    STRING_LITERAL_ATTR("rgb402", F_GAG),
    STRING_LITERAL_ATTR("rgb403", F_GAG),
    STRING_LITERAL_ATTR("rgb404", F_GAG),
    STRING_LITERAL_ATTR("rgb405", F_GAG),
    STRING_LITERAL_ATTR("rgb410", F_GAG),
    STRING_LITERAL_ATTR("rgb411", F_GAG),
    STRING_LITERAL_ATTR("rgb412", F_GAG),
    STRING_LITERAL_ATTR("rgb413", F_GAG),
    STRING_LITERAL_ATTR("rgb414", F_GAG),
    STRING_LITERAL_ATTR("rgb415", F_GAG),
    STRING_LITERAL_ATTR("rgb420", F_GAG),
    STRING_LITERAL_ATTR("rgb421", F_GAG),
    STRING_LITERAL_ATTR("rgb422", F_GAG),
    STRING_LITERAL_ATTR("rgb423", F_GAG),
    STRING_LITERAL_ATTR("rgb424", F_GAG),
    STRING_LITERAL_ATTR("rgb425", F_GAG),
    STRING_LITERAL_ATTR("rgb430", F_GAG),
    STRING_LITERAL_ATTR("rgb431", F_GAG),
    STRING_LITERAL_ATTR("rgb432", F_GAG),
    STRING_LITERAL_ATTR("rgb433", F_GAG),
    STRING_LITERAL_ATTR("rgb434", F_GAG),
    STRING_LITERAL_ATTR("rgb435", F_GAG),
    STRING_LITERAL_ATTR("rgb440", F_GAG),
    STRING_LITERAL_ATTR("rgb441", F_GAG),
    STRING_LITERAL_ATTR("rgb442", F_GAG),
    STRING_LITERAL_ATTR("rgb443", F_GAG),
    STRING_LITERAL_ATTR("rgb444", F_GAG),
    STRING_LITERAL_ATTR("rgb445", F_GAG),
    STRING_LITERAL_ATTR("rgb450", F_GAG),
    STRING_LITERAL_ATTR("rgb451", F_GAG),
    STRING_LITERAL_ATTR("rgb452", F_GAG),
    STRING_LITERAL_ATTR("rgb453", F_GAG),
    STRING_LITERAL_ATTR("rgb454", F_GAG),
    STRING_LITERAL_ATTR("rgb455", F_GAG),
    STRING_LITERAL_ATTR("rgb500", F_GAG),
    STRING_LITERAL_ATTR("rgb501", F_GAG),
    STRING_LITERAL_ATTR("rgb502", F_GAG),
    STRING_LITERAL_ATTR("rgb503", F_GAG),
    STRING_LITERAL_ATTR("rgb504", F_GAG),
    STRING_LITERAL_ATTR("rgb505", F_GAG),
    STRING_LITERAL_ATTR("rgb510", F_GAG),
    STRING_LITERAL_ATTR("rgb511", F_GAG),
    STRING_LITERAL_ATTR("rgb512", F_GAG),
    STRING_LITERAL_ATTR("rgb513", F_GAG),
    STRING_LITERAL_ATTR("rgb514", F_GAG),
    STRING_LITERAL_ATTR("rgb515", F_GAG),
    STRING_LITERAL_ATTR("rgb520", F_GAG),
    STRING_LITERAL_ATTR("rgb521", F_GAG),
    STRING_LITERAL_ATTR("rgb522", F_GAG),
    STRING_LITERAL_ATTR("rgb523", F_GAG),
    STRING_LITERAL_ATTR("rgb524", F_GAG),
    STRING_LITERAL_ATTR("rgb525", F_GAG),
    STRING_LITERAL_ATTR("rgb530", F_GAG),
    STRING_LITERAL_ATTR("rgb531", F_GAG),
    STRING_LITERAL_ATTR("rgb532", F_GAG),
    STRING_LITERAL_ATTR("rgb533", F_GAG),
    STRING_LITERAL_ATTR("rgb534", F_GAG),
    STRING_LITERAL_ATTR("rgb535", F_GAG),
    STRING_LITERAL_ATTR("rgb540", F_GAG),
    STRING_LITERAL_ATTR("rgb541", F_GAG),
    STRING_LITERAL_ATTR("rgb542", F_GAG),
    STRING_LITERAL_ATTR("rgb543", F_GAG),
    STRING_LITERAL_ATTR("rgb544", F_GAG),
    STRING_LITERAL_ATTR("rgb545", F_GAG),
    STRING_LITERAL_ATTR("rgb550", F_GAG),
    STRING_LITERAL_ATTR("rgb551", F_GAG),
    STRING_LITERAL_ATTR("rgb552", F_GAG),
    STRING_LITERAL_ATTR("rgb553", F_GAG),
    STRING_LITERAL_ATTR("rgb554", F_GAG),
    STRING_LITERAL("rgb555"),

    STRING_LITERAL("gray0"),
    STRING_LITERAL_ATTR("gray1", F_GAG),
    STRING_LITERAL_ATTR("gray2", F_GAG),
    STRING_LITERAL_ATTR("gray3", F_GAG),
    STRING_LITERAL_ATTR("gray4", F_GAG),
    STRING_LITERAL_ATTR("gray5", F_GAG),
    STRING_LITERAL_ATTR("gray6", F_GAG),
    STRING_LITERAL_ATTR("gray7", F_GAG),
    STRING_LITERAL_ATTR("gray8", F_GAG),
    STRING_LITERAL_ATTR("gray9", F_GAG),
    STRING_LITERAL_ATTR("gray10", F_GAG),
    STRING_LITERAL_ATTR("gray11", F_GAG),
    STRING_LITERAL_ATTR("gray12", F_GAG),
    STRING_LITERAL_ATTR("gray13", F_GAG),
    STRING_LITERAL_ATTR("gray14", F_GAG),
    STRING_LITERAL_ATTR("gray15", F_GAG),
    STRING_LITERAL_ATTR("gray16", F_GAG),
    STRING_LITERAL_ATTR("gray17", F_GAG),
    STRING_LITERAL_ATTR("gray18", F_GAG),
    STRING_LITERAL_ATTR("gray19", F_GAG),
    STRING_LITERAL_ATTR("gray20", F_GAG),
    STRING_LITERAL_ATTR("gray21", F_GAG),
    STRING_LITERAL_ATTR("gray22", F_GAG),
    STRING_LITERAL("gray23"),
#endif

    STRING_NULL };


void init_attrs(void)
{
    Var *var;

    for (var = special_var; var->val.name; var++) {
	if (var->func == ch_attr)
	    ch_attr(var);
    }
}

/* Return the result of combining adj into base.  If adj has the 'x' attr,
 * discard base.  If adj has any colors, they override colors in base.  Other
 * attrs are OR'd.
 */
attr_t adj_attr(attr_t base, attr_t adj)
{
    /* XXX BUG:  User should be able to change hiliteattr, do a /recall, and
     * see old hilited lines displayed with the new hiliteattr.  Interpreting
     * F_HILITE here breaks that feature.  But the interpretation is necessary
     * to make colors override correctly when hiliteattr includes a color.
     */
    if (base & F_HILITE) {
	base &= ~F_HILITE;
	base |= hiliteattr;
    }
    if (adj & F_HILITE) {
	adj &= ~F_HILITE;
	adj |= hiliteattr;
    }

    if (adj & F_EXCLUSIVE)
	return adj;
    if (base & adj & F_FGCOLOR)
	base &= ~F_FGCOLORMASK;
    if (base & adj & F_BGCOLOR)
	base &= ~F_BGCOLORMASK;
    return base | adj;
}

/* convert attr string to bitfields */
const char *parse_attrs(const char *str, attr_t *attrp, int delimiter)
{
    int color, len;
    const char *name;
    char buf[16];
    char reject[3] = {',', '\0', '\0'};

    reject[1] = delimiter;
    *attrp = 0;

    if (!str) return "";

    while (*str && *str != delimiter) {
        ++str;
        switch(str[-1]) {
        case ',':  /* skip */             break;
        case 'n':  *attrp |= F_NONE;      break;
        case 'x':  *attrp |= F_EXCLUSIVE; break;
        case 'G':  *attrp |= F_NOHISTORY; break;
        case 'L':  *attrp |= F_NOLOG;     break;
        case 'A':  *attrp |= F_NOACTIVITY;break;
        case 'g':  *attrp |= F_GAG;       break;
        case 'u':  *attrp |= F_UNDERLINE; break;
        case 'r':  *attrp |= F_REVERSE;   break;
        case 'f':  *attrp |= F_FLASH;     break;
        case 'd':  *attrp |= F_DIM;       break;
        case 'B':  *attrp |= F_BOLD;      break;
        case 'b':  *attrp |= F_BELL;      break;
        case 'h':  *attrp |= F_HILITE;    break;
        case 'E':  *attrp |= error_attr;    break;
        case 'W':  *attrp |= warning_attr;  break;
        case 'I':  *attrp |= info_attr;     break;
        case 'C':
	    len = strcspn(str, reject);
            if (str[len] && len < sizeof(buf)) {
                name = strncpy(buf, str, len);
                buf[len] = '\0';
                str += len;
            } else {
                name = str;
                while (*str) ++str;
            }
	    if (strncmp(name, "bg", 2) == 0) {
		if ((color = enum2int(name+2, 0, enum_color, "bgcolor")) < 0)
		    return NULL;
		*attrp = adj_attr(*attrp, bgcolor2attr(color));
	    } else {
		if ((color = enum2int(name, 0, enum_color, "color")) < 0)
		    return NULL;
		*attrp = adj_attr(*attrp, fgcolor2attr(color));
	    }
            break;
        default:
            eprintf("invalid display attribute '%c'", str[-1]);
            return NULL;
        }
    }
    return str;
}

int ch_status_attr(Var *var)
{
    if (!ch_attr(var)) return 0;
    update_status_line(NULL);
    return 1;
}

int ch_attr(Var *var)
{
    Value *val;
    const char *str;
    attr_t attr;

    if (var->val.type & ~(TYPE_STR|TYPE_ATTR)) {
	/* extremely unlikely, so we don't bother to deal with it correctly,
	 * just do enough that we don't crash. */
	internal_error(__FILE__, __LINE__,
	    "ch_attr: variable '%s' has type %04x",
	    var->val.name, var->val.type);
	return 0;
    }
    if (!(val = getvarval(var)) || !(str = valstd(val))) {
        var->val.u.attr = 0;
    } else if (parse_attrs(str, &attr, 0)) {
        var->val.u.attr = attr;
    } else {
	var->val.type &= ~TYPE_ATTR;
        return 0;
    }
    var->val.type |= TYPE_ATTR;
    return 1;
}


static void set_attr(String *line, int offset, attr_t *starting,
    attr_t current)
{
    /* starting_attrs is set by the attrs parameter and/or codes at the
     * beginning of the line.  If no visible mid-line changes occur, there is
     * no need to allocate line->charattrs (which would increase the size of
     * the line by ~5x).  Note that a trailing attribute change is considered
     * a mid-line change; this is sub-optimal, but unprompt() depends on it
     * (it expects prompt->attrs to be the original starting attributes).
     */
    if (!line->charattrs) {
	if (line->len == 0) {
	    /* start of visible line */
	    *starting = current;
	} else if (*starting != current) {
	    /* First mid-line attr change. */
	    check_charattrs(line, line->len, *starting,
		__FILE__, __LINE__);
	}
    }
    if (line->charattrs)
	while (offset < line->len)
	    line->charattrs[offset++] = current;
}

#define ANSI_CSI        (char)0233    /* ANSI terminal Command Sequence Intro */

/* Interpret embedded codes from a subset of ansi codes:
 * ansi attribute/color codes are converted to tf character or line attrs;
 * tabs are expanded (if %expand_tabs is on); all other codes are ignored.
 * (EMUL_DEBUG was handled in handle_socket_input())
 */
String *decode_ansi(const char *s, attr_t attrs, int emul, attr_t *final_attrs)
{
    String *dst;
    int i, colorstate = 0;
    attr_t starting_attrs = attrs;

    if (emul == EMUL_RAW || emul == EMUL_DEBUG) {
	if (final_attrs) *final_attrs = attrs;
	return Stringnew(s, -1, attrs);
    }

    dst = Stringnew(NULL, 1, 0);

    for ( ; *s; s++) {
        if ((emul >= EMUL_ANSI_STRIP) &&
            (*s == ANSI_CSI || (s[0] == '\033' && s[1] == '[' && s++)))
        {
	    /* Start with current value of attrs, and collect attributes from
	     * ANSI codes.  But we don't write to attrs directly, in case this
	     * turns out to not be an "m" command. */
	    attr_t new = attrs;
            if (!*s) break;            /* in case code got truncated */
            do {
                s++;
                i = strtoint(s, &s);
		if (colorstate < 0) {
		    /* ignoring everything after error */
		} else if (colorstate % 10 == 8) {
		    if (i == 5) colorstate++;
		    else colorstate = -1; /* error */
		} else if (colorstate % 10 == 9) {
		    if (i < 0 || i > 255)
			colorstate = -1; /* error */
		    else {
#if NCOLORS == 256
			if (colorstate / 10 == 3)
			    new = (new & ~F_FGCOLORMASK) | fgcolor2attr(i);
			else
			    new = (new & ~F_BGCOLORMASK) | bgcolor2attr(i);
#endif
			colorstate = 0;
		    }
                } else if (!i || emul < EMUL_ANSI_ATTR) {
                    new = 0;
                } else if (i >= 30 && i <= 37) {
                    new = (new & ~F_FGCOLORMASK) | fgcolor2attr(i - 30);
                } else if (i >= 40 && i <= 47) {
                    new = (new & ~F_BGCOLORMASK) | bgcolor2attr(i - 40);
                } else if (i >= 90 && i <= 97) { /* not really ANSI */
                    new = (new & ~F_FGCOLORMASK) | fgcolor2attr(i - 90 + 8);
                } else if (i >= 100 && i <= 107) { /* not really ANSI */
                    new = (new & ~F_BGCOLORMASK) | bgcolor2attr(i - 100 + 8);
                } else if (i == 38 || i == 48) { /* not really ANSI */
		    colorstate = i;
		    /* Subsequences of the form "38;5;N" or "48;5;N" describe
		     * fg or bg xterm 256-color controls, where 0<=N<=255.
		     * Once we see the start of such a subsequence, any
		     * deviation from the correct form invalidates the rest
		     * of the control sequence.
		     */
                } else switch (i) {
                    case 1:   new |= F_BOLD;          break;
                    case 4:   new |= F_UNDERLINE;     break;
                    case 5:   new |= F_FLASH;         break;
                    case 7:   new |= F_REVERSE;       break;
                    case 21:  new &= ~F_BOLD;         break;
                    case 22:  new &= ~(F_BOLD|F_DIM); break;
                    case 24:  new &= ~F_UNDERLINE;    break;
                    case 25:  new &= ~F_FLASH;        break;
                    case 27:  new &= ~F_REVERSE;      break;
                    default:  /* ignore it */         break;
                }
            } while (s[0] == ';' && s[1]);

            if (!*s) {			/* in case code got truncated */
		break;
	    } else if (*s == '?') {	/* ignore ESC [ ? alnum */
                if (!*++s) break;
            } else if (*s == 'm') {	/* attribute command */
                attrs = new;
            } /* ignore any other CSI command */

        } else if ((emul >= EMUL_ANSI_STRIP) && (*s == '\033')) {
            /* ignore ESC # digit, ESC ( alnum, ESC ) alnum, and ESC alnum. */
            if (!*++s) break;
            if (*s == '(' || *s == ')' || *s == '#')
                if (!*++s) break;

        } else if (is_print(*s) || *s == '\t') {
	    int orig_len = dst->len;
	    if (*s == '\t' && expand_tabs) {
		Stringnadd(dst, ' ', tabsize - dst->len % tabsize);
	    } else {
		Stringadd(dst, *s);
	    }
	    set_attr(dst, orig_len, &starting_attrs, attrs);

        } else if (*s == '\b') {
	    /* bug: doesn't handle expanded tabs */
	    if (dst->len > 0)
		Stringtrunc(dst, dst->len - 1);

        } else if (*s == '\07') {
            dst->attrs |= F_BELL;
        }
    }

    if (!dst->charattrs) {
        /* No mid-line changes, so apply starting_attrs to entire line */
        dst->attrs |= starting_attrs;
    } else {
        dst->charattrs[dst->len] = attrs;
    }

    if (final_attrs) *final_attrs = attrs;
    return dst;
}

/* Convert embedded '@' codes to internal character or line attrs. */
String *decode_attr(const conString *src, attr_t attrs, int offset)
{
    const char *s;
    String *dst;
    int off;
    attr_t new;
    attr_t starting_attrs;
    attr_t orig_attrs;
    const cattr_t *orig_charattrs = src->charattrs;

    dst = Stringnew(NULL, src->len, 0);
    if (!src->data) return dst;
    starting_attrs = dst->attrs = adj_attr(src->attrs, attrs);

    for (s = src->data + offset; *s; s++) {
        if (s[0] == '@' && s[1] == '{') {
            s+=2;
            if ((off = (*s == '~'))) s++;
            s = parse_attrs(s, &new, '}');
            if (!s) goto decode_attr_error;
            if (*s != '}') {
                eprintf("unmatched @{");
                goto decode_attr_error;
            }
            if (new & F_BELL && !off) dst->attrs |= F_BELL;
	    new &= ~F_BELL;
            if (new & F_FGCOLOR) attrs &= ~F_FGCOLORMASK;
            if (new & F_BGCOLOR) attrs &= ~F_BGCOLORMASK;
            if (new & F_NONE) attrs = 0;
            if (off) attrs &= ~new;
            else attrs |= new;

        } else {
	    orig_attrs = orig_charattrs ? orig_charattrs[s - src->data] : 0;
            Stringadd(dst, *s);
            set_attr(dst, dst->len - 1, &starting_attrs, adj_attr(orig_attrs, attrs));
            if (s[0] == '@' && s[1] == '@')
                s++;
        }
    }

    if (!dst->charattrs) {
        /* No mid-line changes, so apply starting_attrs to entire line */
        dst->attrs |= starting_attrs;
    } else {
        dst->charattrs[dst->len] = attrs;
    }
    return dst;

decode_attr_error:
    dst->links++;
    Stringfree(dst);
    return NULL;
}

/* appends string representation of attrs to buffer */
String *attr2str(String *buffer, attr_t attrs)
{
    if (attrs & F_NONE)       Stringadd(buffer, 'n');
    if (attrs & F_EXCLUSIVE)  Stringadd(buffer, 'x');
    if (attrs & F_GAG)        Stringadd(buffer, 'g');
    if (attrs & F_NOHISTORY)  Stringadd(buffer, 'G');
    if (attrs & F_NOLOG)      Stringadd(buffer, 'L');
    if (attrs & F_NOACTIVITY) Stringadd(buffer, 'A');
    if (attrs & F_UNDERLINE)  Stringadd(buffer, 'u');
    if (attrs & F_REVERSE)    Stringadd(buffer, 'r');
    if (attrs & F_FLASH)      Stringadd(buffer, 'f');
    if (attrs & F_DIM)        Stringadd(buffer, 'd');
    if (attrs & F_BOLD)       Stringadd(buffer, 'B');
    if (attrs & F_BELL)       Stringadd(buffer, 'b');
    if (attrs & F_HILITE)     Stringadd(buffer, 'h');
    if (attrs & F_FGCOLOR)
        SStringcat(Stringadd(buffer, 'C'), &enum_color[attr2fgcolor(attrs)]);
    if (attrs & F_BGCOLOR) {
        if (attrs & F_FGCOLOR) Stringadd(buffer, ',');
        SStringcat(Stringcat(buffer, "Cbg"), &enum_color[attr2bgcolor(attrs)]);
    }
    return buffer;
}

String *encode_attr(const conString *str, int offset)
{
    attr_t oldattrs = 0, attrs;
    int i;
    String *new;
    
    new = Stringnew(NULL, str->len+1, 0);
    if (!str->charattrs) {
	if (str->attrs)
	    Stringadd(attr2str(Stringcat(new, "@{"), str->attrs), '}');
	for (i = offset; i < str->len; i++) {
	    Stringadd(new, str->data[i]);
	    if (str->data[i] == '@')
		Stringadd(new, '@');
	}
	if (str->attrs)
	    Stringcat(new, "@{n}");
    } else {
	for (i = offset; i < str->len; i++) {
	    attrs = adj_attr(str->attrs, str->charattrs[i]);
	    if ((attrs ^ oldattrs) & F_HWRITE) {
		if (!(attrs & F_HWRITE)) {
		    /* no attrs */
		    Stringcat(new, "@{n}");
		} else if (((oldattrs & ~attrs) & F_ENCODE) == 0) {
		    /* new attrs can be added to old attrs */
		    attr_t added = attrs & ~(oldattrs & F_SIMPLE);
		    if (((attrs ^ oldattrs) & F_FGCOLORS) == 0) /* fg same? */
			added &= ~F_FGCOLORS; /* skip fg */
		    if (((attrs ^ oldattrs) & F_BGCOLORS) == 0) /* bg same? */
			added &= ~F_BGCOLORS; /* skip bg */
		    Stringadd(attr2str(Stringcat(new, "@{"), added & F_HWRITE),
			'}');
		} else {
		    /* attrs are different */
		    Stringadd(attr2str(Stringcat(new, "@{n"), attrs & F_HWRITE),
			'}');
		}
	    }
	    Stringadd(new, str->data[i]);
	    if (str->data[i] == '@')
		Stringadd(new, '@');
	    oldattrs = attrs;
	}
	if (attrs & F_HWRITE)
	    Stringcat(new, "@{n}");
    }
    return new;
}

static String *attr2ansi(String *str, attr_t attrs)
{
#define semi()	if (str->len > orig_len) Stringadd(str,';')
    int orig_len = str->len;
    int color;

    if (attrs & F_HILITE)    attrs |= hiliteattr;

    if (attrs & F_BOLD)      { semi(); Stringcat(str, "1"); }
    if (attrs & F_UNDERLINE) { semi(); Stringcat(str, "4"); }
    if (attrs & F_FLASH)     { semi(); Stringcat(str, "5"); }
    if (attrs & F_REVERSE)   { semi(); Stringcat(str, "7"); }

    if (attrs & F_FGCOLOR) {
	semi();
	color = attr2fgcolor(attrs);
	if (color < 8)
	    Sappendf(str, "%d", color + 30);
	else if (color < 16) /* not really ANSI */
	    Sappendf(str, "%d", color - 8 + 90);
	else /* not really ANSI */
	    Sappendf(str, "38;5;%d", color);
    }
    if (attrs & F_BGCOLOR) {
	semi();
	color = attr2bgcolor(attrs);
	if (color < 8)
	    Sappendf(str, "%d", color + 40);
	else if (color < 16) /* not really ANSI */
	    Sappendf(str, "%d", color - 8 + 100);
	else /* not really ANSI */
	    Sappendf(str, "48;5;%d", color);
    }
#undef semi
    return str;
}

String *encode_ansi(const conString *str, int offset)
{
    attr_t oldattrs = 0, attrs;
    int i;
    String *new;
    
    new = Stringnew(NULL, str->len+1, 0);
    if (!str->charattrs) {
	if (str->attrs)
	    Stringadd(attr2ansi(Stringcat(new, "\033["), str->attrs), 'm');
	Stringcat(new, str->data);
	if (str->attrs)
	    Stringcat(new, "\033[m");
    } else {
	for (i = offset; i < str->len; i++) {
	    attrs = adj_attr(str->attrs, str->charattrs[i]);
	    if ((attrs ^ oldattrs) & F_HWRITE) {
		if (!(attrs & F_HWRITE)) {
		    /* no attrs */
		    Stringcat(new, "\033[m");
		} else if (((oldattrs & ~attrs) & F_ENCODE) == 0) {
		    /* new attrs can be added to old attrs */
		    attr_t added = attrs & ~(oldattrs & F_SIMPLE);
		    if (((attrs ^ oldattrs) & F_FGCOLORS) == 0) /* fg same? */
			added &= ~F_FGCOLORS; /* skip fg */
		    if (((attrs ^ oldattrs) & F_BGCOLORS) == 0) /* bg same? */
			added &= ~F_BGCOLORS; /* skip bg */
		    Stringadd(attr2ansi(Stringcat(new, "\033["),
			added & F_HWRITE), 'm');
		} else {
		    /* attrs are different */
		    Stringadd(attr2ansi(Stringcat(new, "\033[;"),
			attrs & F_HWRITE), 'm');
		}
	    }
	    Stringadd(new, str->data[i]);
	    oldattrs = attrs;
	}
	if (attrs & F_HWRITE)
	    Stringcat(new, "\033[m");
    }
    return new;
}

