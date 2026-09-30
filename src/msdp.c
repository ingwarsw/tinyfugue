/*************************************************************************
 *  TinyFugue - programmable mud client
 *  MSDP (Mud Server Data Protocol) support.
 *
 *  Outgoing MSDP commands are built from a human readable syntax by
 *  msdp_encode() and sent via the msdp() function (see socket.c).
 *
 *  Incoming MSDP sub-negotiations are turned into a flat, human readable
 *  string by msdp_decode() and delivered to scripts through the H_MSDP
 *  hook (see socket.c).  No variables are set implicitly; parsing of the
 *  data is left to the user's macros / Lua / Python code, matching the
 *  design of the existing GMCP/ATCP support.
 ************************************************************************/

#include "tfconfig.h"

#if ENABLE_MSDP

#include "port.h"
#include "tf.h"
#include "dstring.h"
#include "msdp.h"
#include "msdp-tok.h"

/* MSDP shares these telnet constants with socket.c */
#ifndef TN_IAC
# define TN_IAC		((char)255)
# define TN_SE		((char)240)
# define TN_SB		((char)250)
#endif
#ifndef TN_MSDP
# define TN_MSDP	((char)69)
#endif

conString *msdp_encode(const char *cmd)
{
	STATIC_BUFFER(buf);
	const char **tokens;
	const char **r;
	int array_depth = 0;
	int state = MSDP_VAR;

	Stringtrunc(buf, 0);
	Stringadd(buf, TN_IAC);
	Stringadd(buf, TN_SB);
	Stringadd(buf, TN_MSDP);

	tokens = msdp_tok(cmd);
	for (r = tokens; *r; r++) {
		const char *c = *r;
		if (c[1] == 0) { /* single character: [ ] { } = */
			switch (c[0]) {
			case '[': Stringadd(buf, MSDP_TABLE_OPEN); state = MSDP_VAR; continue;
			case ']': Stringadd(buf, MSDP_TABLE_CLOSE); state = MSDP_VAR; continue;
			/*
			 * Inside an array {}, elements are values (MSDP_VAL),
			 * not variables, per the MSDP spec.  Set state=MSDP_VAL
			 * so each element is prefixed with MSDP_VAL.
			 */
			case '{': Stringadd(buf, MSDP_ARRAY_OPEN); state = MSDP_VAL; array_depth++; continue;
			case '}': Stringadd(buf, MSDP_ARRAY_CLOSE); state = MSDP_VAR; if (array_depth) array_depth--; continue;
			case '=': Stringadd(buf, MSDP_VAL); state = MSDP_VAL; continue;
			case ' ': continue; /* eat spaces */
			default: break;	    /* single char identifier, fall through */
			}
		}
		if (state == MSDP_VAL && array_depth)
			/* array element: prefix each with MSDP_VAL, stay in array */
			Stringadd(buf, MSDP_VAL);
		else if (state == MSDP_VAR)
			Stringadd(buf, MSDP_VAR);
		else
			state = MSDP_VAR;
		if (c[0] == '"' && c[strlen(c) - 1] == '"')
			Stringncat(buf, c + 1, strlen(c) - 2);
		else
			Stringcat(buf, c);
	}
	msdp_tok_free(tokens);

	Stringadd(buf, TN_IAC);
	Stringadd(buf, TN_SE);
	return CS(buf);
}

/*
 * Decode a raw MSDP payload into a flat string.  The syntax produced
 * mirrors the input syntax accepted by msdp_encode():
 *     VAR VALUE               -> VAR=VALUE
 *     table                   -> [ ... ]
 *     array                   -> { ... }
 * Values are separated from their variable name by '=', and nested
 * structures are delimited with [ ] / { }.  This gives scripts an easily
 * parseable, whitespace separated representation without imposing any
 * particular data model.
 */
void msdp_decode(const char *p, int len, String *out)
{
	const char *pend = p + len;
	int array_depth = 0;

	Stringtrunc(out, 0);

	while (p < pend) {
		const char *c;
		switch (*p) {
		case MSDP_VAR:
			if (out->len) Stringadd(out, ' ');
			c = p + 1;
			while (c < pend && (unsigned char)*c > MSDP_ARRAY_CLOSE
			       && *c != TN_IAC)
				c++;
			Stringncat(out, p + 1, c - p - 1);
			p = c;
			break;
		case MSDP_VAL:
			/*
			 * A value belonging to a variable is joined with '=';
			 * values that are bare array elements are separated by
			 * whitespace (matching the input syntax of msdp_encode).
			 */
			if (array_depth)
				Stringadd(out, ' ');
			else
				Stringadd(out, '=');
			c = p + 1;
			while (c < pend && (unsigned char)*c > MSDP_ARRAY_CLOSE
			       && *c != TN_IAC)
				c++;
			Stringncat(out, p + 1, c - p - 1);
			p = c;
			break;
		case MSDP_TABLE_OPEN:  Stringcat(out, " ["); p++; break;
		case MSDP_TABLE_CLOSE: Stringcat(out, " ]"); p++; break;
		case MSDP_ARRAY_OPEN:  Stringcat(out, " {"); p++; array_depth++; break;
		case MSDP_ARRAY_CLOSE: Stringcat(out, " }"); p++; if (array_depth) array_depth--; break;
		case TN_IAC:
			if (p + 1 < pend && p[1] == TN_SE)
				p = pend;
			else
				p++;
			break;
		default:
			p++;
		}
	}
}

#endif /* ENABLE_MSDP */
