/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: enumlist.h,v 35000.22 2007/01/13 23:12:39 kkeys Exp $ */

/* It may not be easy to read, but it keeps the constants and the array in the
 * same place, so they can't get out of sync.
 */

#ifndef ENUMEXTERN
# define ENUMEXTERN
#endif

bicode(enum,		static conString enum_bamf[] = )
{
bicode(BAMF_OFF,	STRING_LITERAL("off")),
bicode(BAMF_UNTER,	STRING_LITERAL("on")),
bicode(BAMF_OLD,	STRING_LITERAL("old")),
bicode(BAMF_COUNT,	STRING_NULL)
};

bicode(enum,		static conString enum_meta[] = )
{
bicode(META_OFF,	STRING_LITERAL("off")),
bicode(META_ON,		STRING_LITERAL("on")),
bicode(META_NONPRINT,	STRING_LITERAL("nonprint")),
bicode(META_COUNT,	STRING_NULL)
};

bicode(enum,		static conString enum_textdiv[] = )
{
bicode(TEXTDIV_OFF,	STRING_LITERAL("off")),
bicode(TEXTDIV_ON,	STRING_LITERAL("on")),
bicode(TEXTDIV_ALWAYS,	STRING_LITERAL("always")),
bicode(TEXTDIV_CLEAR,	STRING_LITERAL("clear")),
bicode(TEXTDIV_COUNT,	STRING_NULL)
};

bicode(enum,		static conString enum_emul[] = )
{
bicode(EMUL_RAW,	STRING_LITERAL("raw")),
bicode(EMUL_PRINT,	STRING_LITERAL("print")),
bicode(EMUL_ANSI_STRIP,	STRING_LITERAL("ansi_strip")),
bicode(EMUL_ANSI_ATTR,	STRING_LITERAL("ansi_attr")),
bicode(EMUL_DEBUG,	STRING_LITERAL("debug")),
bicode(EMUL_COUNT,	STRING_NULL)
};

ENUMEXTERN conString enum_match[]
bicode(; enum,       = )
{
/* do not reorder these, it would break macros that use the number */
bicode(MATCH_SIMPLE,	STRING_LITERAL("simple")),
bicode(MATCH_GLOB,	STRING_LITERAL("glob")),
bicode(MATCH_REGEXP,	STRING_LITERAL("regexp")),
bicode(MATCH_SUBSTR,	STRING_LITERAL("substr")),
bicode(MATCH_COUNT,	STRING_NULL)
};

ENUMEXTERN conString enum_status[]
bicode(; typedef enum,       = )
{
bicode(STAT_MORE,	STRING_LITERAL("more")),
bicode(STAT_WORLD,	STRING_LITERAL("world")),
bicode(STAT_READ,	STRING_LITERAL("read")),
bicode(STAT_ACTIVE,	STRING_LITERAL("active")),
bicode(STAT_LOGGING,	STRING_LITERAL("log")),
bicode(STAT_MAIL,	STRING_LITERAL("mail")),
bicode(STAT_CLOCK,	STRING_LITERAL("clock")),
bicode(STAT_COUNT,	STRING_NULL),
bicode(STAT_NONE = -1,	STRING_NULL)
}
bicode( stat_id_t;, ; )

ENUMEXTERN conString enum_eol[]
bicode(; enum,    = )
{
bicode(EOL_LF,		STRING_LITERAL("LF")),
bicode(EOL_CR,		STRING_LITERAL("CR")),
bicode(EOL_CRLF,	STRING_LITERAL("CRLF")),
bicode(EOL_COUNT,	STRING_NULL)
};

#undef ENUMEXTERN
#undef bicode
