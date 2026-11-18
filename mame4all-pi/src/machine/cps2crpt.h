#ifndef CPS2CRPT_H
#define CPS2CRPT_H
/* returns a malloc'ed copy of the program with decrypted opcodes (ROM region layout) */
UINT8 *cps2_decrypt(const UINT8 *rom, int length, const UINT32 *master_key, UINT32 upper_limit,
					void (*progress)(int percent));
#endif
