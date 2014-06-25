/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 2000-2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: opcodes.h,v 35004.29 2007/01/13 23:12:39 kkeys Exp $ */

/*
 * Expression operators use the ASCII character as their opcode to be
 * mnemonic; other operators use characters just because that's the easiest
 * way to avoid colliding with the expression operator characters and
 * keep them in the 0x7F OPNUM_MASK space.
 */

/*        label     code  type  arg   flag */
/*        -----     ----  ----  ---   ---- */

/* 01-3F: expression operators.  Flag: has side effect */
defopcode(EQUAL    ,0x01, EXPR, INT,  0)     /*  ==  */
defopcode(NOTEQ    ,0x02, EXPR, INT,  0)     /*  !=  */
defopcode(GTE      ,0x03, EXPR, INT,  0)     /*  >=  */
defopcode(LTE      ,0x04, EXPR, INT,  0)     /*  <=  */
defopcode(STREQ    ,0x05, EXPR, INT,  0)     /*  =~  */
defopcode(STRNEQ   ,0x06, EXPR, INT,  0)     /*  !~  */
defopcode(MATCH    ,0x07, EXPR, INT,  0)     /*  =/  */
defopcode(NMATCH   ,0x08, EXPR, INT,  0)     /*  !/  */
defopcode(ASSIGN   ,':',  EXPR, INT,  SIDE)  /*  :=  */
defopcode(PREINC   ,0x0A, EXPR, INT,  SIDE)  /*  ++  */
defopcode(PREDEC   ,0x0B, EXPR, INT,  SIDE)  /*  --  */
defopcode(POSTINC  ,0x0C, EXPR, INT,  SIDE)  /*  ++  */
defopcode(POSTDEC  ,0x0D, EXPR, INT,  SIDE)  /*  --  */
defopcode(FUNC     ,0x0E, EXPR, INT,  SIDE)  /*  name(...)  */
defopcode(NOT      ,'!',  EXPR, INT,  0)     /*  !   */
defopcode(MUL      ,'*',  EXPR, INT,  0)     /*  *   */
defopcode(MULA     ,'*',  EXPR, INT,  SIDE)  /*  *=  */
defopcode(ADD      ,'+',  EXPR, INT,  0)     /*  +   */
defopcode(ADDA     ,'+',  EXPR, INT,  SIDE)  /*  +=  */
defopcode(SUB      ,'-',  EXPR, INT,  0)     /*  -   */
defopcode(SUBA     ,'-',  EXPR, INT,  SIDE)  /*  -=  */
defopcode(DIV      ,'/',  EXPR, INT,  0)     /*  /   */
defopcode(DIVA     ,'/',  EXPR, INT,  SIDE)  /*  /=  */
defopcode(LT       ,'<',  EXPR, INT,  0)     /*  <   */
defopcode(GT       ,'>',  EXPR, INT,  0)     /*  >   */

/* positional parameter substitution operators.  Flag: 0 push, 1 append */
defopcode(PPARM    ,'A', SUB,  INT,  0)     /* {3} positional param */
defopcode(APARM    ,'A', SUB,  INT,  APP)
defopcode(PXPARM   ,'B', SUB,  INT,  0)     /* {-3} complementary pos param */
defopcode(AXPARM   ,'B', SUB,  INT,  APP)
defopcode(PLPARM   ,'C', SUB,  INT,  0)     /* {L3} pos param, from end */
defopcode(ALPARM   ,'C', SUB,  INT,  APP)
defopcode(PLXPARM  ,'D', SUB,  INT,  0)     /* {-L3} comp pos param from end */
defopcode(ALXPARM  ,'D', SUB,  INT,  APP)
defopcode(PPARM_CNT,'E', SUB,  NONE, 0)     /* {#} pos. param. count */
defopcode(APARM_CNT,'E', SUB,  NONE, APP)
defopcode(PPARM_ALL,'F', SUB,  NONE, 0)     /* {*} all pos. params. */
defopcode(APARM_ALL,'F', SUB,  NONE, APP)
defopcode(PPARM_RND,'G', SUB,  NONE, 0)     /* {R} random pos. param. */
defopcode(APARM_RND,'G', SUB,  NONE, APP)

/* other substitution operators.  Flag: 0 push, 1 append */
defopcode(PRESULT  ,'P', SUB,  NONE, 0)     /* {?} user_result */
defopcode(ARESULT  ,'P', SUB,  NONE, APP)
defopcode(PREG     ,'Q', SUB,  INT,  0)     /* regexp captured string */
defopcode(AREG     ,'Q', SUB,  INT,  APP)
defopcode(PCMDSUB  ,'R', SUB,  NONE, 0)     /* output of a cmdsub */
defopcode(ACMDSUB  ,'R', SUB,  NONE, APP)
defopcode(PMAC     ,'S', SUB,  STRP, 0)     /* value of macro */
defopcode(AMAC     ,'S', SUB,  STRP, APP)
defopcode(PVAR     ,'T', SUB,  VALP, 0)     /* value of variable */
defopcode(AVAR     ,'T', SUB,  VALP, APP)
defopcode(PBUF     ,'U', SUB,  STRP, 0)	    /* buf */
/*defopcode(ABUF     ,'U', SUB,  STRP, APP)*/
defopcode(PCMDNAME ,'V', SUB, NONE, 0)	    /* {0} command name */
defopcode(ACMDNAME ,'V', SUB, NONE, APP)

/* jump operators.  Flag: negate condition */
defopcode(JZ       ,'0', JUMP, INT,  0)     /* jump if zero */
defopcode(JNZ      ,'0', JUMP, INT,  NEG)   /* jump if not zero */
defopcode(JRZ      ,'1', JUMP, INT,  0)     /* jump if user_result == 0 */
defopcode(JRNZ     ,'1', JUMP, INT,  NEG)   /* jump if user_result != 0 */
/*defopcode(JEMPTY   ,'2', JUMP, INT,  0)*/ /* jump if empty string */
defopcode(JNEMPTY  ,'2', JUMP, INT,  NEG)   /* jump if not empty string */
defopcode(JUMP     ,'3', JUMP, INT,  0)

/* control operators.  Flag: negate result. */
/* STRP defaults to buffer. */
defopcode(SEND     ,'a', CTRL, STRP, 0)     /* send string to server */
defopcode(EXECUTE  ,'b', CTRL, STRP, 0)     /* execute arbitrary cmd line */
defopcode(BUILTIN  ,'c', CTRL, CMDP, 0)     /* execute a resovled builtin */
defopcode(NBUILTIN ,'c', CTRL, CMDP, NEG)   /* execute a resovled builtin */
defopcode(COMMAND  ,'d', CTRL, CMDP, 0)     /* execute a resovled command */
defopcode(NCOMMAND ,'d', CTRL, CMDP, NEG)   /* execute a resovled command */
defopcode(MACRO    ,'e', CTRL, VALP, 0)     /* execute a macro cmd line */
defopcode(NMACRO   ,'e', CTRL, VALP, NEG)   /* execute a macro cmd line */
defopcode(ARG      ,'f', CTRL, STRP, 0)     /* arg for BUILTIN, COMMAND, SET */
defopcode(APPEND   ,'g', CTRL, STRP, 0)
defopcode(PUSHBUF  ,'h', CTRL, NONE, 0)
defopcode(POPBUF   ,'i', CTRL, NONE, 0)
defopcode(CMDSUB   ,'j', CTRL, NONE, 0)     /* new frame & tfout queue */
/*defopcode(POPFILE  ,'k', CTRL, CHAR, 0)*/ /* pop pointer to a tfile */
defopcode(PUSH     ,'l', CTRL, VALP, 0)
defopcode(POP      ,'m', CTRL, NONE, 0)
defopcode(DUP      ,'n', CTRL, INT,  0)
defopcode(RETURN   ,'o', CTRL, VALP, 0)
defopcode(RESULT   ,'p', CTRL, VALP, 0)
defopcode(TEST     ,'q', CTRL, VALP, 0)
defopcode(DONE     ,'s', CTRL, NONE, 0)
defopcode(ENDIF    ,'t', CTRL, NONE, 0)
defopcode(PIPE     ,'u', CTRL, NONE, 0)     /* pipe from this stmt to next */
defopcode(NOP      ,'v', CTRL, NONE, 0)
defopcode(EXPR     ,'w', CTRL, NONE, 0)
defopcode(LET      ,'x', CTRL, VALP, 0)	    /* set local variable */
defopcode(SET      ,'y', CTRL, VALP, 0)	    /* set global variable */
defopcode(SETENV   ,'z', CTRL, VALP, 0)	    /* set environment variable */

#undef defopcode
