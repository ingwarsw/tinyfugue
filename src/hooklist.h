/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: hooklist.h,v 35000.24 2007/01/13 23:12:39 kkeys Exp $ */

/* This keeps the constants and the array in the same place
 * so they can't get out of sync.
 */

gencode(ACTIVITY,	HT_ALERT | HT_XSOCK),
gencode(BAMF,		HT_WORLD | HT_XSOCK),
gencode(BGTEXT,		0),
gencode(BGTRIG,		HT_ALERT | HT_XSOCK),
gencode(CONFAIL,	HT_WORLD | HT_XSOCK),
gencode(CONFLICT,	0),
gencode(CONNECT,	HT_WORLD | HT_XSOCK),
gencode(DISCONNECT,	HT_WORLD | HT_XSOCK),
gencode(ICONFAIL,	HT_WORLD | HT_XSOCK),
gencode(KILL,		0),
gencode(LOAD,		0),
gencode(LOADFAIL,	0),
gencode(LOG,		0),
gencode(LOGIN,		0),
gencode(MAIL,		HT_ALERT),
gencode(MORE,		0),
gencode(NOMACRO,	0),
gencode(PENDING,	HT_WORLD | HT_XSOCK),
gencode(PREACTIVITY,	0),
gencode(PROCESS,	0),
gencode(PROMPT,		0),
gencode(PROXY,		0),
gencode(REDEF,		0),
gencode(RESIZE,		0),
gencode(SEND,		0),
gencode(SHADOW,		0),
gencode(SHELL,		0),
gencode(SIGHUP,		0),
gencode(SIGTERM,	0),
gencode(SIGUSR1,	0),
gencode(SIGUSR2,	0),
gencode(WORLD,		HT_WORLD),
