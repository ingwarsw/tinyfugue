/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/

/* sorted by name */
/*	 Name		Pure	Arguments */
/*			(!used)	Min Max	  */

funccode(abs,		1,	1,  1),
funccode(acos,		1,	1,  1),
funccode(addworld,	0,	2,  9),
funccode(ascii,		1,	1,  1),
funccode(asin,		1,	1,  1),
funccode(atan,		1,	1,  1),
#if ENABLE_ATCP
funccode(atcp,		0,	1,  2),
#endif
funccode(char,		1,	1,  1),
funccode(columns,	0,	0,  0),
funccode(cos,		1,	1,  1),
funccode(cputime,	0,	0,  0),
funccode(decode_ansi,	1,	1,  1),
funccode(decode_attr,	1,	1,  3),
funccode(echo,		0,	1,  4),
funccode(encode_ansi,	1,	1,  1),
funccode(encode_attr,	1,	1,  1),
funccode(eval,		0,	1,  2),
funccode(exp,		1,	1,  1),
funccode(fake_recv,	0,	1,  3),
funccode(fg_world,	0,	0,  0),
funccode(filename,	0,	1,  1),
funccode(ftime,		0,	0,  2),
funccode(fwrite,	0,	2,  2),
funccode(gethostname,	0,	0,  0),
funccode(getopts,	0,	1,  2),
funccode(getpid,	1,	0,  0),
#if ENABLE_GMCP
funccode(gmcp,		0,	1,  2),
#endif
funccode(idle,		0,	0,  1),
funccode(is_connected,	0,	0,  1),
funccode(is_open,	0,	0,  1),
funccode(isatty,	1,	0,  0),
funccode(kbdel,		0,	1,  1),
funccode(kbgoto,	0,	1,  1),
funccode(kbhead,	0,	0,  0),
funccode(kblen,		0,	0,  0),
funccode(kbmatch,	0,	0,  1),
funccode(kbpoint,	0,	0,  0),
funccode(kbtail,	0,	0,  0),
funccode(kbwordleft,	0,	0,  1),
funccode(kbwordright,	0,	0,  1),
funccode(keycode,	0,	1,  1),
funccode(lines,		1,	0,  0),
funccode(ln,		1,	1,  1),
funccode(log10,		1,	1,  1),
funccode(mktime,	0,	1,  7), /* !pure: uses TZ */
funccode(mod,		1,	2,  2),
funccode(morepaused,	0,	0,  1),
funccode(morescroll,	0,	1,  1),
funccode(moresize,	0,	0,  2),
funccode(nactive,	0,	0,  1),
funccode(nlog,		0,	0,  0),
funccode(nmail,		0,	0,  0),
funccode(nread,		0,	0,  0),
#if ENABLE_OPTION102
funccode(option102,	0,	1,  2),
#endif
funccode(pad,		1,	1,  (unsigned)-1),
funccode(pow,		1,	2,  2),
funccode(prompt,	0,	1,  1),
#ifdef TFPYTHON
funccode(python,	0,	1,  1),
#endif
funccode(rand,		0,	0,  2),
funccode(read,		0,	0,  0),
funccode(regmatch,	0,	2,  2), /* !pure: sets Pn */
funccode(replace,	1,	3,  3),
funccode(send,		0,	1,  3),
funccode(sidle,		0,	0,  1),
funccode(sin,		1,	1,  1),
funccode(sqrt,		1,	1,  1),
funccode(status_fields,	1,	0,  1),
funccode(status_width,	1,	1,  1),
funccode(strcat,	1,	1,  (unsigned)-1),
funccode(strchr,	1,	2,  3),
funccode(strcmp,	1,	2,  2),
funccode(strcmpattr,	1,	2,  2),
funccode(strip_attr,	1,	1,  1),
funccode(strlen,	1,	1,  1),
funccode(strncmp,	1,	3,  3),
funccode(strrchr,	1,	2,  3),
funccode(strrep,	1,	2,  2),
funccode(strstr,	1,	2,  3),
funccode(substitute,	1,	1,  3),
funccode(substr,	1,	2,  3),
funccode(systype,	1,	0,  0),
funccode(tan,		1,	1,  1),
funccode(test,		0,	1,  1),
funccode(tfclose,	0,	1,  1),
funccode(tfflush,	0,	1,  2),
funccode(tfopen,	0,	0,  2),
funccode(tfread,	0,	1,  2),
funccode(tfreadable,	0,	1,  1),
funccode(tfwrite,	0,	1,  2),
funccode(time,		0,	0,  0),
funccode(tolower,	1,	1,  2),
funccode(toupper,	1,	1,  2),
funccode(trunc,		1,	1,  1),
funccode(whatis,	1,	1,  1),
funccode(winlines,	1,	0,  0),
funccode(world_info,	0,	0,  2)
