/* TELNET special characters (RFC 854) */
#define TN_EOR		((char)239)	/* end of record (RFC 885) */
#define TN_SE		((char)240)	/* subnegotiation end */
#define TN_NOP		((char)241)	/* no operation */
#define TN_DATA_MARK	((char)242)	/* (not used) */
#define TN_BRK		((char)243)	/* break (not used) */
#define TN_IP		((char)244)	/* interrupt process (not used) */
#define TN_AO		((char)245)	/* abort output (not used) */
#define TN_AYT		((char)246)	/* are you there? (not used) */
#define TN_EC		((char)247)	/* erase character (not used) */
#define TN_EL		((char)248)	/* erase line (not used) */
#define TN_GA		((char)249)	/* go ahead */
#define TN_SB		((char)250)	/* subnegotiation begin */
#define TN_WILL		((char)251)	/* I offer to ~, or ack for DO */
#define TN_WONT		((char)252)	/* I will stop ~ing, or nack for DO */
#define TN_DO		((char)253)	/* Please do ~?, or ack for WILL */
#define TN_DONT		((char)254)	/* Stop ~ing!, or nack for WILL */
#define TN_IAC		((char)255)	/* telnet Is A Command character */

/* TELNET options (RFC 855) */		/* RFC# - description */
#define TN_BINARY	((char)0)	/*  856 - transmit binary */
#define TN_ECHO		((char)1)	/*  857 - echo */
#define TN_SGA		((char)3)	/*  858 - suppress GA (not used) */
#define TN_STATUS	((char)5)	/*  859 - (not used) */
#define TN_TIMING_MARK	((char)6)	/*  860 - (not used) */
#define TN_TTYPE	((char)24)	/* 1091 - terminal type */
#define TN_SEND_EOR	((char)25)	/*  885 - transmit EOR */
#define TN_NAWS		((char)31)	/* 1073 - negotiate about window size */
#define TN_TSPEED	((char)32)	/* 1079 - terminal speed (not used) */
#define TN_FLOWCTRL	((char)33)	/* 1372 - (not used) */
#define TN_LINEMODE	((char)34)	/* 1184 - (not used) */
#define TN_XDISPLOC	((char)35)	/* 1096 - (not used) */
#define TN_ENVIRON	((char)36)	/* 1408 - (not used) */
#define TN_AUTH		((char)37)	/* 1416 - (not used) */
#define TN_NEW_ENVIRON	((char)39)	/* 1572 - (not used) */
#define TN_CHARSET	((char)42)	/* 2066 - (not used) */
/* 85 & 86 are not standard.  See http://www.randomly.org/projects/MCCP/ */
#define TN_MSDP		((char)69)  /* http://tintin.sourceforge.net/msdp/ */
#define TN_MSSP		((char)70)  /* http://tintin.sourceforge.net/mssp/ */
#define TN_COMPRESS	((char)85)	/* MCCP v1 */
#define TN_COMPRESS2	((char)86)	/* MCCP v2 */
#define TN_MSP		((char)90)  /* MSP */
#define TN_MXP		((char)91)  /* MXP */
#define TN_ATCP     ((char)200) 
#define TN_GMCP     ((char)201) 
