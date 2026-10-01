/*************************************************************************
 *  TinyFugue - programmable mud client
 *  MSDP command tokenizer (flex).
 ************************************************************************/

#ifndef MSDP_TOK_H
#define MSDP_TOK_H

const char **msdp_tok(const char *);
void msdp_tok_free(const char **);

#endif /* MSDP_TOK_H */
