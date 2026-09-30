/*************************************************************************
 *  TinyFugue - programmable mud client
 *  MSDP (Mud Server Data Protocol) support.
 *
 *  See https://tintin.mudhalla.net/protocols/msdp/
 ************************************************************************/

#ifndef MSDP_H
#define MSDP_H

/* MSDP byte values (sub-negotiation payload markers) */
#define MSDP_VAR		1
#define MSDP_VAL		2
#define MSDP_TABLE_OPEN		3
#define MSDP_TABLE_CLOSE	4
#define MSDP_ARRAY_OPEN		5
#define MSDP_ARRAY_CLOSE	6

/*
 * Encode a human readable MSDP command (e.g. `LIST=COMMANDS` or
 * `REPORT={ HEALTH MANA }`) into the raw MSDP sub-negotiation payload
 * (without the surrounding IAC SB MSDP ... IAC SE framing).  The returned
 * conString is owned by an internal static buffer and is only valid until
 * the next call.
 */
extern conString *msdp_encode(const char *cmd);

/*
 * Decode a raw MSDP sub-negotiation payload (the bytes between
 * IAC SB MSDP and IAC SE, i.e. starting at the first MSDP_VAR) into a
 * flat, human readable string suitable for passing to the H_MSDP hook.
 * The result is stored in `out`.
 */
extern void msdp_decode(const char *p, int len, String *out);

#endif /* MSDP_H */
