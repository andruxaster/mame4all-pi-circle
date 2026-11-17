/* neocrypt.h - Neo-Geo cartridge decryption helpers (see neocrypt.cpp) */
#ifndef NEOCRYPT_H
#define NEOCRYPT_H

/* gfx keys */
#define MSLUG3_GFX_KEY		0xad	/* CMC42 */
#define MSLUG4_GFX_KEY		0x31	/* CMC50 */
#define SAMSHO5_GFX_KEY		0x0f
#define SAMSHO5SP_GFX_KEY	0x0d
#define MSLUG5_GFX_KEY		0x19
#define SENGOKU3_GFX_KEY	0xfe	/* CMC42 */

int neo_cmc42_gfx_decrypt(UINT8 *rom, UINT32 rom_size, int extra_xor);
int neo_cmc50_gfx_decrypt(UINT8 *rom, UINT32 rom_size, int extra_xor);
void neo_sfix_decrypt(const UINT8 *rom, UINT32 rom_size, UINT8 *fixed, UINT32 fixed_size);
int neo_cmc50_m1_decrypt(UINT8 *crypt, UINT8 *audio, UINT32 audio_size);
void neo_pcm2_decrypt(UINT8 *ymrom, UINT32 ymsize, int value);
int neo_pcm2_swap(UINT8 *ymrom, UINT32 ymsize, int value);
int neo_samsho5_decrypt_68k(UINT8 *cpurom);
int neo_samsh5sp_decrypt_68k(UINT8 *cpurom);
int neo_mslug3_decrypt_68k(UINT8 *base);
UINT32 neo_mslug3_bank_base(int sel);
int neo_mslug5_decrypt_68k(UINT8 *rom);

#endif
