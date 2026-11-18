/***************************************************************************

	Capcom System 2 (CPS2)

	Written for MAME4ALL-pi (MAME 0.37b5 API) after the CPS2 driver and
	video code of MAME 0.78 / mame2003-plus (Paul Leaman, Andrew Ouwehand,
	Nicola Salmoria and others) and the opcode decryption of MAME 0.139
	(Andreas Naive, Nicola Salmoria, machine/cps2crpt.cpp).

	- 68000 opcodes are decrypted once at start; the Cyclone core fetches
	  code and PC-relative data from the decrypted copy (c68k_dec_base).
	- own renderer: 3 CPS-A tile layers + CPS2 sprites with the CPS2
	  sprite/layer priority scheme, drawn at vblank like VIDEO_UPDATE_BEFORE_VBLANK.
	- QSound Z80 and sound chip from the CPS1 driver (this file is included
	  at the end of drivers/cps1.cpp).

	This file is #included by drivers/cps1.cpp.

***************************************************************************/

#include "machine/eeprom.h"
#include "machine/cps2crpt.h"

extern unsigned char *c68k_dec_base;	/* cpu/m68000_cyclone/c68000.cpp */
extern unsigned int c68k_dec_limit;

#undef  CODE_SIZE
#define CPS2_CODE_SIZE	0x400000
#define QSOUND_SIZE		0x50000

/* ------------------------------------------------------------------------ */
/* Game keys (MAME 0.139 machine/cps2crpt.c)                                */
/* ------------------------------------------------------------------------ */

struct cps2_game_key
{
	const char *name;
	UINT32 key[2];
	UINT32 upper_limit;
};

static const struct cps2_game_key cps2_keys[] =
{
	{ "sfz2al",   { 0xf172c0d0,0x040621a6 }, 0x100000 },
	{ 0 }
};

/* ------------------------------------------------------------------------ */
/* State                                                                    */
/* ------------------------------------------------------------------------ */

#define CPS2_GFXRAM_SIZE	0x40000		/* 0x900000-0x92ffff, padded for the base masks */
#define CPS2_OBJ_SIZE		0x2000

static UINT8 *c2_gfxram;					/* CPS-A video RAM */
static UINT8 c2_output[0x100];			/* CPS-A/B registers 0x804100 */
static UINT8 c2_objregs[0x10];			/* CPS2 object registers 0x400000 */
static UINT8 *c2_objram[2];
static UINT8 *c2_obj_buffered;
static int c2_objram_bank;
static int c2_pri_ctrl;
static UINT8 *c2_decrypted;

static int c2_scancount, c2_scancalls, c2_scanline1, c2_scanline2;

#define OUTW(o)		READ_WORD(&c2_output[(o) & 0xfe])
#define OBJREG(o)	READ_WORD(&c2_objregs[(o) & 0x0e])

/* CPS-B registers of the CPS2 boards (NOBATTRY config in MAME) */
#define C2_LAYER_CONTROL	0x66
#define C2_MULT_FACTOR1		0x40
#define C2_MULT_FACTOR2		0x42
#define C2_MULT_RESULT_LO	0x44
#define C2_MULT_RESULT_HI	0x46

static void cps2_render(struct osd_bitmap *bitmap);

/* ------------------------------------------------------------------------ */
/* Memory handlers                                                          */
/* ------------------------------------------------------------------------ */

static READ_HANDLER( cps2_output_r )
{
	offset &= 0xfe;
	if (offset == C2_MULT_RESULT_LO)
		return (OUTW(C2_MULT_FACTOR1) * OUTW(C2_MULT_FACTOR2)) & 0xffff;
	if (offset == C2_MULT_RESULT_HI)
		return (OUTW(C2_MULT_FACTOR1) * OUTW(C2_MULT_FACTOR2)) >> 16;
	return OUTW(offset);
}

static WRITE_HANDLER( cps2_output_w )
{
	int v;
	offset &= 0xfe;
	v = COMBINE_WORD(OUTW(offset), data);
	WRITE_WORD(&c2_output[offset], v);
	if (offset == 0x50) c2_scanline1 = v & 0x1ff;
	if (offset == 0x52) c2_scanline2 = v & 0x1ff;
}

static READ_HANDLER( cps2_objregs_r )		{ return OBJREG(offset); }
static WRITE_HANDLER( cps2_objregs_w )		{ WRITE_WORD(&c2_objregs[offset & 0x0e], COMBINE_WORD(OBJREG(offset), data)); }

static READ_HANDLER( cps2_gfxram_r )		{ return READ_WORD(&c2_gfxram[offset]); }
static WRITE_HANDLER( cps2_gfxram_w )		{ WRITE_WORD(&c2_gfxram[offset], COMBINE_WORD(READ_WORD(&c2_gfxram[offset]), data)); }

/* object RAM: 0x700000 bank "1", 0x708000 bank "2", swapped by 0x8040e0 bit 0 */
static READ_HANDLER( cps2_objram1_r )	{ return READ_WORD(&c2_objram[c2_objram_bank & 1][offset & 0x1ffe]); }
static READ_HANDLER( cps2_objram2_r )	{ return READ_WORD(&c2_objram[(c2_objram_bank & 1) ^ 1][offset & 0x1ffe]); }
static WRITE_HANDLER( cps2_objram1_w )
{
	UINT8 *p = &c2_objram[c2_objram_bank & 1][offset & 0x1ffe];
	WRITE_WORD(p, COMBINE_WORD(READ_WORD(p), data));
}
static WRITE_HANDLER( cps2_objram2_w )
{
	UINT8 *p = &c2_objram[(c2_objram_bank & 1) ^ 1][offset & 0x1ffe];
	WRITE_WORD(p, COMBINE_WORD(READ_WORD(p), data));
}
static WRITE_HANDLER( cps2_objram_bank_w )
{
	if ((data & 0x00ff0000) == 0) c2_objram_bank = data & 1;
}

static struct EEPROM_interface cps2_eeprom_interface =
{
	6,		/* address bits */
	16,		/* data bits */
	"0110",	/* read command */
	"0101",	/* write command */
	"0111"	/* erase command */
};

static void cps2_nvram_handler(void *file, int read_or_write)
{
	if (read_or_write)
		EEPROM_save(file);
	else
	{
		EEPROM_init(&cps2_eeprom_interface);
		if (file) EEPROM_load(file);
	}
}

static READ_HANDLER( cps2_in0_r )	{ return readinputport(0); }
static READ_HANDLER( cps2_in1_r )	{ return readinputport(1); }
static READ_HANDLER( cps2_in2_r )	{ return (readinputport(2) & 0xfffe) | (EEPROM_read_bit() & 1); }
static READ_HANDLER( cps2_qsound_volume_r )	{ return 0xe021; }	/* no extra RAM, no network adapter */
static READ_HANDLER( cps2_kludge_r )	{ return 0xffff; }

static WRITE_HANDLER( cps2_eeprom_port_w )
{
	if ((data & 0xff000000) == 0)	/* MSB */
	{
		EEPROM_write_bit(data & 0x1000);
		EEPROM_set_clock_line((data & 0x2000) ? ASSERT_LINE : CLEAR_LINE);
		EEPROM_set_cs_line((data & 0x4000) ? CLEAR_LINE : ASSERT_LINE);
	}
	if ((data & 0x00ff0000) == 0)	/* LSB */
	{
		cpu_set_reset_line(1, (data & 0x0008) ? CLEAR_LINE : ASSERT_LINE);	/* Z80 reset */
		coin_counter_w(0, data & 0x0001);
		coin_counter_w(1, data & 0x0002);
		coin_lockout_w(0, ~data & 0x0010);
		coin_lockout_w(1, ~data & 0x0020);
	}
}

/* used by the old CPS2 graphics viewer in vidhrdw/cps1.cpp */
WRITE_HANDLER( cps2_qsound_sharedram_w )
{
	qsound_sharedram_w(offset, data);
}

static READ_HANDLER( cps2_qsound_r )	{ return qsound_sharedram_r(offset); }
static WRITE_HANDLER( cps2_qsound_w )
{
	if ((data & 0x00ff0000) == 0) qsound_sharedram_w(offset, data & 0xff);
}

static struct MemoryReadAddress cps2_readmem[] =
{
	{ 0x000000, 0x3fffff, MRA_ROM },
	{ 0x400000, 0x40000f, cps2_objregs_r },
	{ 0x618000, 0x619fff, cps2_qsound_r },
	{ 0x660000, 0x663fff, MRA_BANK2 },
	{ 0x664000, 0x664001, MRA_BANK3 },
	{ 0x700000, 0x701fff, cps2_objram1_r },
	{ 0x708000, 0x70ffff, cps2_objram2_r },	/* 0x708000 and mirrors */
	{ 0x800100, 0x8001ff, cps2_output_r },	/* mirror (sfa) */
	{ 0x804000, 0x804001, cps2_in0_r },
	{ 0x804010, 0x804011, cps2_in1_r },
	{ 0x804020, 0x804021, cps2_in2_r },
	{ 0x804030, 0x804031, cps2_qsound_volume_r },
	{ 0x8040b0, 0x8040b3, cps2_kludge_r },
	{ 0x804100, 0x8041ff, cps2_output_r },
	{ 0x900000, 0x92ffff, cps2_gfxram_r },
	{ 0xff0000, 0xffffff, MRA_BANK4 },
	{ -1 }
};

static struct MemoryWriteAddress cps2_writemem[] =
{
	{ 0x000000, 0x3fffff, MWA_ROM },
	{ 0x400000, 0x40000f, cps2_objregs_w },
	{ 0x618000, 0x619fff, cps2_qsound_w },
	{ 0x660000, 0x663fff, MWA_BANK2 },
	{ 0x664000, 0x664001, MWA_BANK3 },
	{ 0x700000, 0x701fff, cps2_objram1_w },
	{ 0x708000, 0x70ffff, cps2_objram2_w },
	{ 0x800100, 0x8001ff, cps2_output_w },
	{ 0x804040, 0x804041, cps2_eeprom_port_w },
	{ 0x8040a0, 0x8040a1, MWA_NOP },
	{ 0x8040e0, 0x8040e1, cps2_objram_bank_w },
	{ 0x804100, 0x8041ff, cps2_output_w },
	{ 0x900000, 0x92ffff, cps2_gfxram_w },
	{ 0xff0000, 0xffffff, MWA_BANK4 },
	{ -1 }
};

/* ------------------------------------------------------------------------ */
/* Interrupts: 262 lines, IRQ2 at vblank, IRQ4 at the raster lines           */
/* ------------------------------------------------------------------------ */

static void cps2_objram_latch(void)
{
	c2_pri_ctrl = OBJREG(0x04);
	memcpy(c2_obj_buffered, c2_objram[c2_objram_bank & 1], CPS2_OBJ_SIZE);	/* cps2_objbase() of MAME */
}

static int cps2_interrupt(void)
{
	int irq = ignore_interrupt();

	/* kludge to pass the sound board test with sound disabled */
	if (Machine->sample_rate == 0 && qsound_sharedram)
		qsound_sharedram[0xfff] = 0x77;

	if (c2_scancount >= 261)
	{
		c2_scancount = -1;
		c2_scancalls = 0;
	}
	c2_scancount++;

	if (OUTW(0x50) & 0x8000) WRITE_WORD(&c2_output[0x50], OUTW(0x50) & 0x1ff);
	if (OUTW(0x52) & 0x8000) WRITE_WORD(&c2_output[0x52], OUTW(0x52) & 0x1ff);

	if (c2_scanline1 == c2_scancount || (c2_scanline1 < c2_scancount && !c2_scancalls))
	{
		WRITE_WORD(&c2_output[0x50], 0);
		irq = 4;
		c2_pri_ctrl = OBJREG(0x04);
		c2_scancalls++;
	}
	if (c2_scanline2 == c2_scancount || (c2_scanline2 < c2_scancount && !c2_scancalls))
	{
		WRITE_WORD(&c2_output[0x52], 0);
		irq = 4;
		c2_pri_ctrl = OBJREG(0x04);
		c2_scancalls++;
	}

	if (c2_scancount == 256)	/* vblank */
	{
		WRITE_WORD(&c2_output[0x50], c2_scanline1);
		WRITE_WORD(&c2_output[0x52], c2_scanline2);
		if (irq == 4) cpu_cause_interrupt(0, 4);
		irq = 2;
		/* draw now, before the game updates the registers in vblank */
		if (!osd_skip_this_frame())
			cps2_render(Machine->scrbitmap);
		cps2_objram_latch();
	}
	return irq;
}

static void cps2_init_machine(void)
{
	c2_scancount = 0;
	c2_scancalls = 0;
	c2_scanline1 = c2_scanline2 = 262;
	c2_objram_bank = 0;
	c2_pri_ctrl = 0;
	memset(c2_output, 0, sizeof(c2_output));
	memset(c2_objregs, 0, sizeof(c2_objregs));
	/* defaults */
	WRITE_WORD(&c2_output[0x00], 0x9200);
	WRITE_WORD(&c2_output[0x02], 0x9000);
	WRITE_WORD(&c2_output[0x04], 0x9040);
	WRITE_WORD(&c2_output[0x06], 0x9080);
	WRITE_WORD(&c2_output[0x08], 0x9100);
	WRITE_WORD(&c2_output[0x0a], 0x90c0);
}

/* ------------------------------------------------------------------------ */
/* Video                                                                    */
/* ------------------------------------------------------------------------ */

static UINT16 *c2_old_palette;
static UINT8 *c2_pri;				/* per pixel: bit 0-2 = layers drawn, 31 = sprite */
static int c2_clip_l, c2_clip_r, c2_clip_t, c2_clip_b;	/* r, b exclusive */

static UINT8 *c2_vram_base(int reg, int boundary)
{
	int base = OUTW(reg) * 256;
	base &= ~(boundary - 1);
	return &c2_gfxram[base & 0x3ffff];
}

static void cps2_build_palette(void)
{
	UINT8 *pal = c2_vram_base(0x0a, 0x800);
	int i;

	for (i = 0; i < 4096; i++)
	{
		int p = READ_WORD(&pal[i * 2]);
		if (p != c2_old_palette[i])
		{
			int bright = 0x10 + (p >> 12);
			int r = ((p >> 8) & 0x0f) * bright * 0x11 / 0x1f;
			int g = ((p >> 4) & 0x0f) * bright * 0x11 / 0x1f;
			int b = ((p >> 0) & 0x0f) * bright * 0x11 / 0x1f;
			palette_change_color(i, r, g, b);
			c2_old_palette[i] = p;
		}
	}
}

/* layer: 0 = scroll1 (8x8), 1 = scroll2 (16x16), 2 = scroll3 (32x32) */
static void cps2_draw_layer(struct osd_bitmap *bitmap, int layer, int primask)
{
	static const int tsize[3] = { 8, 16, 32 };
	static const int elem_bytes[3] = { 64, 128, 512 };
	static const int line_bytes[3] = { 8, 8, 16 };
	static const int org[3] = { 4, 0, 0 };				/* 8x8 tiles: right half of a 16x16 row */
	static const int code_base[3] = { 4 * 0x8000, 4 * 0x4000, 4 * 0x1000 };
	static const int reg_base[3] = { 0x02, 0x04, 0x06 };
	static const int reg_sx[3] = { 0x0c, 0x10, 0x14 };
	static const int reg_sy[3] = { 0x0e, 0x12, 0x16 };
	const UINT8 *gfx = memory_region(REGION_GFX1);
	int nelem = memory_region_length(REGION_GFX1) / elem_bytes[layer];
	int ts = tsize[layer], mask = ts * 64 - 1;
	UINT8 *vram = c2_vram_base(reg_base[layer], 0x4000);
	int sx = OUTW(reg_sx[layer]), sy = OUTW(reg_sy[layer]);
	int videocontrol = OUTW(0x22);
	UINT8 *other = c2_vram_base(0x08, 0x800);
	int otheroffs = OUTW(0x20);
	const UINT16 *pens = Machine->pens + (32 * (layer + 1)) * 16;
	int width = Machine->drv->screen_width;
	int y;

	for (y = c2_clip_t; y < c2_clip_b; y++)
	{
		UINT16 *dest = (UINT16 *)bitmap->line[y];
		UINT8 *pri = c2_pri + y * width;
		int ty = (y + sy) & mask;
		int tx, x = c2_clip_l;
		int row = ty / ts, ry = ty % ts;

		tx = x + sx;
		if (layer == 1 && (videocontrol & 0x01))		/* line scroll */
			tx += READ_WORD(&other[((y + otheroffs) & 0x3ff) * 2]);

		while (x < c2_clip_r)
		{
			int px = tx & mask;
			int col = px / ts, rx = px % ts;
			int run = ts - rx;
			int off, code, attr, color, flipx, flipy, i, r;
			const UINT8 *src;

			if (run > c2_clip_r - x) run = c2_clip_r - x;

			switch (layer)
			{
				case 0:  off = (row & 0x1f) + ((col & 0x3f) << 5) + ((row & 0x20) << 6); break;
				case 1:  off = (row & 0x0f) + ((col & 0x3f) << 4) + ((row & 0x30) << 6); break;
				default: off = (row & 0x07) + ((col & 0x3f) << 3) + ((row & 0x38) << 6); break;
			}
			code = READ_WORD(&vram[(off * 4) & 0x3ffe]);
			attr = READ_WORD(&vram[(off * 4 + 2) & 0x3ffe]);
			color = (attr & 0x1f) * 16;
			flipx = attr & 0x20;
			flipy = attr & 0x40;

			r = flipy ? ts - 1 - ry : ry;
			src = gfx + ((code + code_base[layer]) % nelem) * elem_bytes[layer] + r * line_bytes[layer] + org[layer];

			for (i = 0; i < run; i++)
			{
				int c = flipx ? ts - 1 - (rx + i) : rx + i;
				int pen = (src[c >> 1] >> ((c & 1) * 4)) & 0x0f;
				if (pen != 15)
				{
					dest[x + i] = pens[color + pen];
					pri[x + i] |= primask;
				}
			}
			x += run;
			tx += run;
		}
	}
}

static void cps2_draw_sprite(struct osd_bitmap *bitmap, int code, int color, int flipx, int flipy,
		int sx, int sy, int primask)
{
	const UINT8 *gfx = memory_region(REGION_GFX1);
	int nelem = memory_region_length(REGION_GFX1) / 128;
	const UINT8 *src = gfx + (code % nelem) * 128;
	const UINT16 *pens = Machine->pens + color * 16;
	int width = Machine->drv->screen_width;
	int x, y;

	if (sx >= c2_clip_r || sx + 16 <= c2_clip_l || sy >= c2_clip_b || sy + 16 <= c2_clip_t) return;

	for (y = 0; y < 16; y++)
	{
		int dy = sy + y;
		const UINT8 *row;
		UINT16 *dest;
		UINT8 *pri;
		if (dy < c2_clip_t || dy >= c2_clip_b) continue;
		row = src + (flipy ? 15 - y : y) * 8;
		dest = (UINT16 *)bitmap->line[dy];
		pri = c2_pri + dy * width;
		for (x = 0; x < 16; x++)
		{
			int dx = sx + x, c, pen;
			if (dx < c2_clip_l || dx >= c2_clip_r) continue;
			c = flipx ? 15 - x : x;
			pen = (row[c >> 1] >> ((c & 1) * 4)) & 0x0f;
			if (pen == 15) continue;
			if ((primask >> pri[dx]) & 1) continue;	/* hidden by a tile layer */
			dest[dx] = pens[pen];
			pri[dx] = 31;
		}
	}
}

static void cps2_draw_sprites(struct osd_bitmap *bitmap, int *primasks)
{
	const UINT8 *base = c2_obj_buffered;
	int xoffs = 64 - OBJREG(0x08);
	int yoffs = 16 - OBJREG(0x0a);
	int last = CPS2_OBJ_SIZE / 8 - 1;
	int i;

	/* find the end of list marker */
	for (i = 0; i < CPS2_OBJ_SIZE / 8; i++)
	{
		if (READ_WORD(&base[i * 8 + 2]) >= 0x8000 || READ_WORD(&base[i * 8 + 6]) >= 0xff00)
		{
			last = i - 1;
			break;
		}
	}

	for (i = last; i >= 0; i--)
	{
		int x = READ_WORD(&base[i * 8 + 0]);
		int y = READ_WORD(&base[i * 8 + 2]);
		int priority = (x >> 13) & 0x07;
		int code = READ_WORD(&base[i * 8 + 4]) + ((y & 0x6000) << 3);
		int colour = READ_WORD(&base[i * 8 + 6]);
		int col = colour & 0x1f;
		int pm = primasks[priority];

		if (colour & 0x80)
		{
			x += OBJREG(0x08);
			y += OBJREG(0x0a);
		}

		if (colour & 0xff00)
		{
			int nx = ((colour & 0x0f00) >> 8) + 1;
			int ny = ((colour & 0xf000) >> 12) + 1;
			int nxs, nys;

			for (nys = 0; nys < ny; nys++)
				for (nxs = 0; nxs < nx; nxs++)
				{
					int sx = (x + nxs * 16 + xoffs) & 0x3ff;
					int sy = (y + nys * 16 + yoffs) & 0x3ff;
					int c;
					if (colour & 0x40)
					{
						if (colour & 0x20) c = code + (nx - 1) - nxs + 0x10 * (ny - 1 - nys);
						else               c = code + nxs + 0x10 * (ny - 1 - nys);
					}
					else
					{
						if (colour & 0x20) c = code + (nx - 1) - nxs + 0x10 * nys;
						else               c = (code & ~0xf) + ((code + nxs) & 0xf) + 0x10 * nys;
					}
					cps2_draw_sprite(bitmap, c, col, colour & 0x20, colour & 0x40, sx, sy, pm);
				}
		}
		else
			cps2_draw_sprite(bitmap, code, col, colour & 0x20, colour & 0x40,
					(x + xoffs) & 0x3ff, (y + yoffs) & 0x3ff, pm);
	}
}

static void cps2_render(struct osd_bitmap *bitmap)
{
	int layercontrol = OUTW(C2_LAYER_CONTROL);
	static const int enable_mask[3] = { 0x02, 0x04, 0x08 };
	int l[4], lpri[4], primasks[8], i, y, x;
	int width = Machine->drv->screen_width;
	UINT16 bg;

	c2_clip_l = Machine->visible_area.min_x;
	c2_clip_r = Machine->visible_area.max_x + 1;
	c2_clip_t = Machine->visible_area.min_y;
	c2_clip_b = Machine->visible_area.max_y + 1;

	cps2_build_palette();
	bg = Machine->pens[4095];

	for (y = c2_clip_t; y < c2_clip_b; y++)
	{
		UINT16 *d = (UINT16 *)bitmap->line[y];
		for (x = c2_clip_l; x < c2_clip_r; x++) d[x] = bg;
		memset(c2_pri + y * width + c2_clip_l, 0, c2_clip_r - c2_clip_l);
	}

	/* order from back to front, 0 = sprites, 1-3 = tile layers */
	l[0] = (layercontrol >> 0x06) & 3;
	l[1] = (layercontrol >> 0x08) & 3;
	l[2] = (layercontrol >> 0x0a) & 3;
	l[3] = (layercontrol >> 0x0c) & 3;
	for (i = 0; i < 4; i++) lpri[i] = (c2_pri_ctrl >> (4 * l[i])) & 0x0f;

	/* take out the CPS1 sprite layer */
	if (l[0] == 0) { l[0] = l[1]; l[1] = 0; lpri[0] = lpri[1]; }
	if (l[1] == 0) { l[1] = l[2]; l[2] = 0; lpri[1] = lpri[2]; }
	if (l[2] == 0) { l[2] = l[3]; l[3] = 0; lpri[2] = lpri[3]; }

	{
		int mask0 = 0xaa, mask1 = 0xcc;
		if (lpri[0] > lpri[1]) mask0 &= ~0x88;
		if (lpri[0] > lpri[2]) mask0 &= ~0xa0;
		if (lpri[1] > lpri[2]) mask1 &= ~0xc0;

		primasks[0] = 0xff;
		for (i = 1; i < 8; i++)
		{
			if (i <= lpri[0] && i <= lpri[1] && i <= lpri[2])
			{
				primasks[i] = 0xfe;
				continue;
			}
			primasks[i] = 0;
			if (i <= lpri[0]) primasks[i] |= mask0;
			if (i <= lpri[1]) primasks[i] |= mask1;
			if (i <= lpri[2]) primasks[i] |= 0xf0;
		}
	}

	for (i = 0; i < 3; i++)
	{
		int layer = l[i] - 1;		/* 0..2 */
		if (layer < 0) continue;
		if (!(layercontrol & enable_mask[layer])) continue;
		cps2_draw_layer(bitmap, layer, 1 << i);
	}
	cps2_draw_sprites(bitmap, primasks);
}

static int c2_vh_start(void)
{
	c2_gfxram = (UINT8 *)malloc(CPS2_GFXRAM_SIZE);
	c2_objram[0] = (UINT8 *)malloc(CPS2_OBJ_SIZE);
	c2_objram[1] = (UINT8 *)malloc(CPS2_OBJ_SIZE);
	c2_obj_buffered = (UINT8 *)malloc(CPS2_OBJ_SIZE);
	c2_old_palette = (UINT16 *)malloc(4096 * sizeof(UINT16));
	c2_pri = (UINT8 *)malloc(Machine->drv->screen_width * Machine->drv->screen_height);
	if (!c2_gfxram || !c2_objram[0] || !c2_objram[1] || !c2_obj_buffered || !c2_old_palette || !c2_pri)
		return 1;
	memset(c2_gfxram, 0, CPS2_GFXRAM_SIZE);
	memset(c2_objram[0], 0, CPS2_OBJ_SIZE);
	memset(c2_objram[1], 0, CPS2_OBJ_SIZE);
	memset(c2_obj_buffered, 0, CPS2_OBJ_SIZE);
	memset(c2_old_palette, 0xff, 4096 * sizeof(UINT16));	/* force the first update */
	{
		int i;
		for (i = 0; i < 4096; i++) palette_change_color(i, 0, 0, 0);
	}
	return 0;
}

static void c2_vh_stop(void)
{
	free(c2_gfxram);		c2_gfxram = 0;
	free(c2_objram[0]);		c2_objram[0] = 0;
	free(c2_objram[1]);		c2_objram[1] = 0;
	free(c2_obj_buffered);	c2_obj_buffered = 0;
	free(c2_old_palette);	c2_old_palette = 0;
	free(c2_pri);			c2_pri = 0;
	c68k_dec_base = 0;
	c68k_dec_limit = 0;
	free(c2_decrypted);		c2_decrypted = 0;
}

static void c2_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	/* drawn at vblank by cps2_interrupt() */
}

/* ------------------------------------------------------------------------ */
/* Init                                                                     */
/* ------------------------------------------------------------------------ */

/* MAME: ROMX_LOAD(..., ROM_GROUPWORD | ROM_SKIP(6)) for groups of 4 ROMs.
   Here the ROMs are loaded one after the other and interleaved by word. */
static void cps2_interleave_gfx(const int *group_size, int groups)
{
	UINT8 *rgn = memory_region(REGION_GFX1);
	int g, base = 0;

	for (g = 0; g < groups; g++)
	{
		int size = group_size[g];		/* size of one ROM */
		UINT8 *tmp = (UINT8 *)malloc(size * 4);
		int r, i;
		if (!tmp) return;
		memcpy(tmp, rgn + base, size * 4);
		for (r = 0; r < 4; r++)
			for (i = 0; i < size; i += 2)
			{
				rgn[base + i * 4 + r * 2 + 0] = tmp[r * size + i + 0];
				rgn[base + i * 4 + r * 2 + 1] = tmp[r * size + i + 1];
			}
		free(tmp);
		base += size * 4;
	}
}

static void cps2_unshuffle(UINT8 *buf, int len)	/* len in 8 byte units */
{
	int i;
	UINT8 t[8];

	if (len == 2) return;
	len /= 2;
	cps2_unshuffle(buf, len);
	cps2_unshuffle(buf + len * 8, len);
	for (i = 0; i < len / 2; i++)
	{
		memcpy(t, buf + (len / 2 + i) * 8, 8);
		memcpy(buf + (len / 2 + i) * 8, buf + (len + i) * 8, 8);
		memcpy(buf + (len + i) * 8, t, 8);
	}
}

static void cps2_gfx_decode(void)
{
	UINT8 *gfx = memory_region(REGION_GFX1);
	int size = memory_region_length(REGION_GFX1);
	int i, j;

	for (i = 0; i < size; i += 0x200000)
		cps2_unshuffle(gfx + i, 0x200000 / 8);

	/* planar to packed 4 bit, pixel 0 in the low nibble */
	for (i = 0; i < size / 4; i++)
	{
		UINT32 src = gfx[4*i] | (gfx[4*i+1] << 8) | (gfx[4*i+2] << 16) | ((UINT32)gfx[4*i+3] << 24);
		UINT32 dwval = 0;
		for (j = 0; j < 8; j++)
		{
			int n = 0;
			UINT32 mask = (0x80808080 >> j) & src;
			if (mask & 0x000000ff) n |= 1;
			if (mask & 0x0000ff00) n |= 2;
			if (mask & 0x00ff0000) n |= 4;
			if (mask & 0xff000000) n |= 8;
			dwval |= n << (j * 4);
		}
		gfx[4*i  ] = dwval;
		gfx[4*i+1] = dwval >> 8;
		gfx[4*i+2] = dwval >> 16;
		gfx[4*i+3] = dwval >> 24;
	}
}

static void cps2_decrypt_progress(int percent)
{
	char b[40];
	sprintf(b, "Decrypting %d%%", percent);
	osd_display_loading_rom_message(b, 0, 0);
}

static void cps2_init_common(const int *group_size, int groups)
{
	const struct cps2_game_key *k;

	cps2_interleave_gfx(group_size, groups);
	cps2_gfx_decode();

	for (k = cps2_keys; k->name; k++)
		if (!strcmp(k->name, Machine->gamedrv->name)) break;
	if (k->name)
	{
		c2_decrypted = cps2_decrypt(memory_region(REGION_CPU1), memory_region_length(REGION_CPU1),
				k->key, k->upper_limit, cps2_decrypt_progress);
		c68k_dec_base = c2_decrypted;
		c68k_dec_limit = c2_decrypted ? k->upper_limit : 0;
	}
}

void init_sfz2al(void)
{
	static const int groups[2] = { 0x400000, 0x100000 };
	cps2_init_common(groups, 2);
}

/* ------------------------------------------------------------------------ */
/* Inputs                                                                   */
/* ------------------------------------------------------------------------ */

INPUT_PORTS_START( cps2_2p6b )
	PORT_START      /* IN0 (0x804000) */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN1 (0x804010) */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BIT( 0xffc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN2 (0x804020) */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_SPECIAL )   /* EEPROM bit */
	PORT_BITX(0x0002, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x00f8, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0c00, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

/* ------------------------------------------------------------------------ */
/* Machine                                                                  */
/* ------------------------------------------------------------------------ */

static struct MachineDriver machine_driver_cps2 =
{
	{
		{
			CPU_M68000,
			11800000,
			cps2_readmem, cps2_writemem, 0, 0,
			cps2_interrupt, 262		/* one call per line: raster IRQ4, vblank IRQ2 */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			8000000,
			qsound_readmem, qsound_writemem, 0, 0,
			0, 0,
			interrupt, 251			/* timed IRQ, 251 Hz */
		}
	},
	59.633333, 0,
	1,
	cps2_init_machine,

	/* video hardware */
	64*8, 32*8, { 8*8, (64-8)*8-1, 2*8, 30*8-1 },
	0,
	4096, 0,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	c2_vh_start,
	c2_vh_stop,
	c2_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{ SOUND_QSOUND, &qsound_interface }
	},

	cps2_nvram_handler
};

/* ------------------------------------------------------------------------ */
/* ROMs (current MAME sets)                                                 */
/* ------------------------------------------------------------------------ */

ROM_START( sfz2al )
	ROM_REGION( CPS2_CODE_SIZE, REGION_CPU1 )      /* 68000 code (encrypted opcodes) */
	ROM_LOAD_WIDE_SWAP( "szaa.03", 0x000000, 0x80000, 0x88e7023e )
	ROM_LOAD_WIDE_SWAP( "szaa.04", 0x080000, 0x80000, 0xae8ec36e )
	ROM_LOAD_WIDE_SWAP( "szaa.05", 0x100000, 0x80000, 0xf053a55e )
	ROM_LOAD_WIDE_SWAP( "szaa.06", 0x180000, 0x80000, 0xcfc0e7a8 )
	ROM_LOAD_WIDE_SWAP( "szaa.07", 0x200000, 0x80000, 0x5feb8b20 )
	ROM_LOAD_WIDE_SWAP( "szaa.08", 0x280000, 0x80000, 0x6eb6d412 )

	ROM_REGION( 0x1400000, REGION_GFX1 )	/* interleaved and decoded in init_sfz2al() */
	ROM_LOAD( "sza.13m",   0x0000000, 0x400000, 0x4d1f1f22 )
	ROM_LOAD( "sza.15m",   0x0400000, 0x400000, 0x19cea680 )
	ROM_LOAD( "sza.17m",   0x0800000, 0x400000, 0xe01b4588 )
	ROM_LOAD( "sza.19m",   0x0c00000, 0x400000, 0x0feeda64 )
	ROM_LOAD( "sza.14m",   0x1000000, 0x100000, 0x0560c6aa )
	ROM_LOAD( "sza.16m",   0x1100000, 0x100000, 0xae940f87 )
	ROM_LOAD( "sza.18m",   0x1200000, 0x100000, 0x4bc3c8bc )
	ROM_LOAD( "sza.20m",   0x1300000, 0x100000, 0x39e674c0 )

	ROM_REGION( QSOUND_SIZE, REGION_CPU2 ) /* QSound Z80 */
	ROM_LOAD( "sza.01",   0x00000, 0x08000, 0x1bc323cf )
	ROM_CONTINUE(         0x10000, 0x18000 )
	ROM_LOAD( "sza.02",   0x28000, 0x20000, 0xba6a5013 )

	ROM_REGION( 0x400000, REGION_SOUND1 ) /* QSound samples */
	ROM_LOAD( "sza.11m",   0x000000, 0x200000, 0xaa47a601 )
	ROM_LOAD( "sza.12m",   0x200000, 0x200000, 0x2237bc53 )
ROM_END

GAME( 1996, sfz2al, 0, cps2, cps2_2p6b, sfz2al, ROT0_16BIT, "Capcom", "Street Fighter Zero 2 Alpha (Asia 960826)" )
