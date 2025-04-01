#include "tfconfig.h"
#include "port.h"
#include "tf.h"

#include "telopt.h"
#include "search.h"
#include "world.h"
#include "history.h"
#include "socket.h"
#include "msdp-tok.h"
#include "tfio.h"
#include "msdp.h"
#define MSDP_VERBOSE	1
#define MSDP_LOOP		2

struct _msdp_commands {
	const char * cmd;
	int id;
	int flags;
};

static const struct _msdp_commands msdp_commands[] = {
	{ "LIST", 1, 0 },
	{ "GET", 2, 0 },
	{ "REPORT", 3, 0 },
};

conString * msdp_encode(const char *);

int send_raw(conString *, const char * world);

struct Value *handle_msdp_command(String * args, int offset) {
	// const char buf[] = "NAME=\"DAN\" ROOM=[ NAME=\"The void\" COORDINATES=[ X=\"0\" Y=\"0\" Z=\"0\" ] ITEMS={ \"FOO\" \"BAR\" \"BAZ\" } ] HP=\"32\"";
	// const char buf[] = "TEST={ A B \"C D\" } FOO=BAR";
	STATIC_BUFFER(buf);
	Stringtrunc(buf, 0);
	SStringocat(buf, CS(args), offset);
	conString * encoded = msdp_encode(buf->data);
	if (msdp_dbg & MSDP_LOOP) {
		recv_msdp_sb(encoded->data+2, encoded->len-2);
	} else {
		send_raw(encoded, 0); // fixme handle world settings later
	}

	return newint(0);
}



conString * msdp_encode(const char * cmd) {
	STATIC_BUFFER(buf);
	Stringtrunc(buf, 0);
	Stringadd(buf, TN_IAC);
	Stringadd(buf, TN_SB);
	Stringadd(buf, TN_MSDP);
	const char ** tokens;
	const char ** r;
	int array_depth=0;
	tokens=msdp_tok(cmd);
	int state=MSDP_VAR;
	for(r=tokens; *r; r++) {
		const char * c=*r;
		if (c[1]==0) { // single character, []{}=
			switch(c[0]) {
			case '[': Stringadd(buf, MSDP_TABLE_OPEN); state=MSDP_VAR; continue;
			case ']': Stringadd(buf, MSDP_TABLE_CLOSE); state=MSDP_VAR; continue;
			case '{': Stringadd(buf, MSDP_ARRAY_OPEN); state=MSDP_VAR; array_depth++; continue;
			case '}': Stringadd(buf, MSDP_ARRAY_CLOSE); state=MSDP_VAR; if (array_depth) array_depth--; continue;
			case '=': Stringadd(buf, MSDP_VAL); state=MSDP_VAL; continue;
			case ' ': continue; // just eat spaces?
			default: break; // single char identifier, fallthrough
			}
		}
		if (state==MSDP_VAR)
			Stringadd(buf, MSDP_VAR);
		else
			state=MSDP_VAR;
		if (c[0] == '"' && c[strlen(c)-1]=='"')
			Stringncat(buf, c+1, strlen(c)-2);
		else
			Stringcat(buf, c);
	}
	msdp_tok_free(tokens);

	// and close it
	Stringadd(buf, TN_IAC);
	Stringadd(buf, TN_SE);

	return CS(buf);
}


int update_msdp_value(conString * name, conString * value) {
	int result;
	STATIC_BUFFER(trig);
	String * old_incoming_text;

	old_incoming_text = incoming_text;

	Stringcpy(trig, "MSDP ");
	SStringocat(trig, name, 6);
	Stringadd(trig, ' ');
	SStringcat(trig, value);

	incoming_text = trig;
	result = find_and_run_matches(NULL, -1, &incoming_text, xworld(),
		TRUE, 0);

	incoming_text = old_incoming_text;

	do_set(name->data, hash_string(name->data),
		value, 0, 0, 0);


	return result;
}

int recv_msdp_sb(const char *p, int olen) {
	STATIC_BUFFER(path);
	STATIC_BUFFER(value);
	STATIC_BUFFER(test);
	int namestack[10];
	int namepos=0;
	const char * pend=p+olen;
	int ret=0;
	int namedepth=0;
	int t;

#define NPUSH(x)	do { namestack[namepos++]=namedepth; namedepth=x; } while (0)
#define NPOP()		do { if (namepos) namedepth=namestack[--namepos]; } while (0)

	Stringcpy(test, "% MSDP recv ");
	Stringcpy(path, "MSDP");
	int setvalue=0;

	/* start with TN_MSDP */
	if (*p && *p == TN_MSDP) {
		int len;
		*p++;
		const char *c;
		int t;

		while (p<pend) {
			switch(*p) {
			case MSDP_VAR:
				c=p+1;
				while(c<pend && *c>MSDP_ARRAY_CLOSE && *c!=TN_IAC)
					c++;

				if (setvalue) {
					Sappendf(test, " Set %s to %s ", path->data,
							value->data);
					update_msdp_value(CS(path), CS(value));
					setvalue=0;
				}
				Stringtrunc(value, 0);
				Stringtrunc(path, namedepth + 4);
				Stringcat(path, "__");
				Stringncat(path, p+1, c-p-1);
				Stringadd(test, ' ');
				SStringcat(test, CS(path));
				p=c;
				break;


			case MSDP_VAL:
				Stringcat(test, "=");
				c=p+1;
				while(c<pend && *c>MSDP_ARRAY_CLOSE && *c!=TN_IAC)
					c++;

				if (c-p-1 > 0) {
					if (!setvalue) {
						t=0;
						Stringncpy(value, p+1, c-p-1);
						setvalue++;
					} else {
						t=value->len+1;
						Stringadd(value, ' ');
						Stringncat(value, p+1, c-p-1);
					}
					SStringocat(test, CS(value), t);
					Stringcat(test, " ");
				}
				p=c;
				break;
			case MSDP_TABLE_OPEN:
				Stringcat(test, " ["); p++;
				NPUSH(path->len-4);
				break;
			case MSDP_TABLE_CLOSE:
				Stringcat(test, "] "); p++;
				NPOP();
				break;
			case MSDP_ARRAY_OPEN:
				Stringcat(test, " {"); p++; break;
			case MSDP_ARRAY_CLOSE:
				Stringcat(test, "} "); p++; break;
			case TN_IAC:
				if (p[1] == TN_SE)
					p=pend;
				else
					p++;
				break;
			default:
				p++;
			}
		}
	} else {
		ret=-1;
	}
	if (setvalue) {
		Sappendf(test, " Set %s to %s ", path->data,
				value->data);
		update_msdp_value(CS(path), CS(value));
		setvalue=0;
	}

	if (msdp_dbg & MSDP_VERBOSE)
		tfputline(CS(test), tfout);

	return ret;
}
