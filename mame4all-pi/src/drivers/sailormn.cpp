/***************************************************************************

	Pretty Soldier Sailor Moon (Banpresto, 1995)

	Cave-style hardware (68000 + Z80, YM2151, 2x OKIM6295, 93C46 EEPROM).
	Ported from the Cave driver of MAME 0.78 (Luca Elia) to the 0.37b5 API.
	Video is drawn by its own renderer here: three 512x512 layers (8x8 or
	16x16 tiles, row scroll / row select, layer 2 with 6 bit tiles and a
	banked lower half) and 1024 zooming sprites with encrypted graphics.

	ROMs: current MAME set "sailormn" (bpsm945a.u45 etc).

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/eeprom.h"

/* ------------------------------------------------------------------------ */
/* State                                                                    */
/* ------------------------------------------------------------------------ */

static unsigned char *sm_videoregs;
static unsigned char *sm_vram[3], *sm_vctrl[3];
static unsigned char *sm_spriteram;
static int sm_tilebank;

static UINT8 vblank_irq, unknown_irq, agallet_vblank_irq;

static struct { int len; UINT8 data[32]; } soundbuf;
static int sm_soundlatch;
static UINT8 z80_ram[0x2000];

#define VREG(n)		READ_WORD(&sm_videoregs[(n) * 2])
#define VCTRL(l,n)	READ_WORD(&sm_vctrl[l][(n) * 2])
#define VRAMW(l,o)	READ_WORD(&sm_vram[l][(o) & 0x7ffe])	/* byte offset */

/* decoded graphics: one byte per pixel */
static UINT8 *tiles_8bpp[3];		/* 8x8 elements, 64 bytes each */
static UINT8 *tiles_blank[3];		/* 1 = element is fully transparent */
static int tiles_count[3];

/* ------------------------------------------------------------------------ */
/* Interrupts                                                               */
/* ------------------------------------------------------------------------ */

static void update_irq_state(void)
{
	cpu_set_irq_line(0, 1, (vblank_irq || unknown_irq) ? ASSERT_LINE : CLEAR_LINE);
}

static void sm_build_sprites(void);

static void sm_vblank_start(int param)
{
	vblank_irq = 1;
	update_irq_state();
	sm_build_sprites();
	agallet_vblank_irq = 1;
}

static void sm_vblank_end(int param)
{
	agallet_vblank_irq = 0;
}

#define TIME_VBLANK_IRQ	2000

static int sm_interrupt(void)
{
	timer_set(TIME_IN_USEC(17376 - TIME_VBLANK_IRQ), 0, sm_vblank_start);
	timer_set(TIME_IN_USEC(17376 - TIME_VBLANK_IRQ + 2000), 0, sm_vblank_end);
	return ignore_interrupt();
}

static READ_HANDLER( sm_irq_cause_r )
{
	int result = 0x0003;

	if (vblank_irq)		result ^= 0x01;
	if (unknown_irq)	result ^= 0x02;

	if (offset == 4)	vblank_irq = 0;
	if (offset == 6)	unknown_irq = 0;

	update_irq_state();

	/* the game waits for bit 2 to go 1 -> 0 once per frame */
	if (offset == 0)
	{
		result &= ~4;
		result |= (agallet_vblank_irq ? 0 : 4);
	}
	return result;
}

/* ------------------------------------------------------------------------ */
/* Sound communication                                                      */
/* ------------------------------------------------------------------------ */

static READ_HANDLER( soundflags_ack_r )
{
	return (soundbuf.len > 0) ? 0 : 2;
}

static WRITE_HANDLER( sound_cmd_w )
{
	sm_soundlatch = COMBINE_WORD(sm_soundlatch, data);
	cpu_set_nmi_line(1, PULSE_LINE);
	cpu_spinuntil_time(TIME_IN_USEC(50));	/* let the sound CPU reply */
}

static READ_HANDLER( soundlatch_ack_r )
{
	if (soundbuf.len > 0)
	{
		UINT8 data = soundbuf.data[0];
		memmove(soundbuf.data, soundbuf.data + 1, sizeof(soundbuf.data) - 1);
		soundbuf.len--;
		return data;
	}
	return 0xff;
}

static WRITE_HANDLER( soundlatch_ack_w )
{
	if (soundbuf.len < 32)
		soundbuf.data[soundbuf.len++] = data;
}

static READ_HANDLER( soundflags_r )		{ return 0; }
static READ_HANDLER( soundlatch_lo_r )	{ return sm_soundlatch & 0xff; }
static READ_HANDLER( soundlatch_hi_r )	{ return (sm_soundlatch >> 8) & 0xff; }

static READ_HANDLER( z80_ram_r )		{ return z80_ram[offset & 0x1fff]; }
static WRITE_HANDLER( z80_ram_w )		{ z80_ram[offset & 0x1fff] = data; }

static WRITE_HANDLER( sm_rombank_w )
{
	UINT8 *RAM = memory_region(REGION_CPU2);
	int bank = data & 0x1f;
	if (bank > 1) bank += 2;
	cpu_setbank(2, &RAM[0x4000 * bank]);
}

static void oki_bank(int region, int data)
{
	UINT8 *RAM = memory_region(region);
	int bank1 = (data >> 0) & 0xf;
	int bank2 = (data >> 4) & 0xf;
	if (Machine->sample_rate == 0 || RAM == 0) return;
	memcpy(RAM + 0x20000 * 0, RAM + 0x40000 + 0x20000 * bank1, 0x20000);
	memcpy(RAM + 0x20000 * 1, RAM + 0x40000 + 0x20000 * bank2, 0x20000);
}

static WRITE_HANDLER( sm_okibank0_w )	{ oki_bank(REGION_SOUND1, data); }
static WRITE_HANDLER( sm_okibank1_w )	{ oki_bank(REGION_SOUND2, data); }

/* ------------------------------------------------------------------------ */
/* EEPROM / inputs                                                          */
/* ------------------------------------------------------------------------ */

static struct EEPROM_interface eeprom_interface =
{
	6,				/* address bits */
	16,				/* data bits */
	"0110",			/* read */
	"0101",			/* write */
	"0111",			/* erase */
	0,				/* lock */
	"0100110000"	/* unlock */
};

/* factory defaults, last byte = region (0 Japan, 1 USA, 2 Europe, 3 HK, 4 Taiwan, 5 Korea) */
static UINT8 sm_default_eeprom[18] = {0xa5,0x00,0xa5,0x00,0xa5,0x00,0xa5,0x00,0xa5,0x01,0xa5,0x01,0xa5,0x04,0xa5,0x01,0xa5,0x02};

static void sm_nvram_handler(void *file, int read_or_write)
{
	if (read_or_write)
		EEPROM_save(file);
	else
	{
		EEPROM_init(&eeprom_interface);
		if (file) EEPROM_load(file);
		else EEPROM_set_data(sm_default_eeprom, sizeof(sm_default_eeprom));
	}
}

static WRITE_HANDLER( sm_eeprom_w )
{
	if ((data & 0xff000000) == 0)	/* MSB written */
	{
		int bank = (data & 0x0100) ? 1 : 0;
		sm_tilebank = bank;

		coin_lockout_w(1, ~data & 0x8000);
		coin_lockout_w(0, ~data & 0x4000);
		coin_counter_w(1, data & 0x2000);
		coin_counter_w(0, data & 0x1000);

		EEPROM_write_bit(data & 0x0800);
		EEPROM_set_cs_line((data & 0x0200) ? CLEAR_LINE : ASSERT_LINE);
		EEPROM_set_clock_line((data & 0x0400) ? ASSERT_LINE : CLEAR_LINE);
	}
}

static READ_HANDLER( sm_input0_r )	{ return readinputport(0); }
static READ_HANDLER( sm_input1_r )	{ return readinputport(1) | ((EEPROM_read_bit() & 1) << 11); }

static void sm_init_machine(void)
{
	soundbuf.len = 0;
	sm_soundlatch = 0;
	vblank_irq = unknown_irq = agallet_vblank_irq = 0;
	sm_tilebank = 0;
	memset(z80_ram, 0, sizeof(z80_ram));
	sm_rombank_w(0, 0);
}

/* ------------------------------------------------------------------------ */
/* Memory maps                                                              */
/* ------------------------------------------------------------------------ */

static struct MemoryReadAddress sm_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM			},
	{ 0x100000, 0x10ffff, MRA_BANK1			},	/* RAM */
	{ 0x200000, 0x3fffff, MRA_ROM			},
	{ 0x408000, 0x40bfff, paletteram_word_r	},	/* palette */
	{ 0x500000, 0x507fff, MRA_BANK3			},	/* sprites */
	{ 0x508000, 0x50ffff, MRA_BANK4			},
	{ 0x600000, 0x600001, sm_input0_r		},
	{ 0x600002, 0x600003, sm_input1_r		},
	{ 0x800000, 0x807fff, MRA_BANK5			},	/* layer 0 */
	{ 0x880000, 0x887fff, MRA_BANK6			},	/* layer 1 */
	{ 0x900000, 0x907fff, MRA_BANK7			},	/* layer 2 */
	{ 0xa00000, 0xa00005, MRA_BANK8			},	/* layer controls */
	{ 0xa80000, 0xa80005, MRA_BANK9			},
	{ 0xb00000, 0xb00005, MRA_BANK10		},
	{ 0xb80000, 0xb80007, sm_irq_cause_r	},
	{ 0xb8006c, 0xb8006d, soundflags_ack_r	},
	{ 0xb8006e, 0xb8006f, soundlatch_ack_r	},
	{ -1 }
};

static struct MemoryWriteAddress sm_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM			},
	{ 0x100000, 0x10ffff, MWA_BANK1			},
	{ 0x200000, 0x3fffff, MWA_ROM			},
	{ 0x408000, 0x40bfff, paletteram_xGGGGGRRRRRBBBBB_word_w, &paletteram },
	{ 0x500000, 0x507fff, MWA_BANK3, &sm_spriteram, &spriteram_size },
	{ 0x508000, 0x50ffff, MWA_BANK4			},
	{ 0x700000, 0x700001, sm_eeprom_w		},
	{ 0x800000, 0x807fff, MWA_BANK5, &sm_vram[0] },
	{ 0x880000, 0x887fff, MWA_BANK6, &sm_vram[1] },
	{ 0x900000, 0x907fff, MWA_BANK7, &sm_vram[2] },
	{ 0xa00000, 0xa00005, MWA_BANK8, &sm_vctrl[0] },
	{ 0xa80000, 0xa80005, MWA_BANK9, &sm_vctrl[1] },
	{ 0xb00000, 0xb00005, MWA_BANK10, &sm_vctrl[2] },
	{ 0xb8006e, 0xb8006f, sound_cmd_w		},
	{ 0xb80000, 0xb8007f, MWA_BANK11, &sm_videoregs },
	{ -1 }
};

static struct MemoryReadAddress sm_sound_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM	},
	{ 0x4000, 0x7fff, MRA_BANK2	},
	{ 0xc000, 0xffff, z80_ram_r	},
	{ -1 }
};

static struct MemoryWriteAddress sm_sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM	},
	{ 0xc000, 0xffff, z80_ram_w	},
	{ -1 }
};

static struct IOReadPort sm_sound_readport[] =
{
	{ 0x20, 0x20, soundflags_r				},
	{ 0x30, 0x30, soundlatch_lo_r			},
	{ 0x40, 0x40, soundlatch_hi_r			},
	{ 0x51, 0x51, YM2151_status_port_0_r	},
	{ 0x60, 0x60, OKIM6295_status_0_r		},
	{ 0x80, 0x80, OKIM6295_status_1_r		},
	{ -1 }
};

static struct IOWritePort sm_sound_writeport[] =
{
	{ 0x00, 0x00, sm_rombank_w				},
	{ 0x10, 0x10, soundlatch_ack_w			},
	{ 0x50, 0x50, YM2151_register_port_0_w	},
	{ 0x51, 0x51, YM2151_data_port_0_w		},
	{ 0x60, 0x60, OKIM6295_data_0_w			},
	{ 0x70, 0x70, sm_okibank0_w				},
	{ 0x80, 0x80, OKIM6295_data_1_w			},
	{ 0xc0, 0xc0, sm_okibank1_w				},
	{ -1 }
};

/* ------------------------------------------------------------------------ */
/* Input ports                                                              */
/* ------------------------------------------------------------------------ */

INPUT_PORTS_START( sailormn )
	PORT_START	/* IN0 - Player 1 */
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER1 )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER1 )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER1 )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT_IMPULSE( 0x0100, IP_ACTIVE_LOW, IPT_COIN1, 6 )
	PORT_BITX( 0x0200, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT(  0xfc00, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 - Player 2 */
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 )
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 )
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER2 )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER2 )
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_BUTTON3        | IPF_PLAYER2 )
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT_IMPULSE( 0x0100, IP_ACTIVE_LOW, IPT_COIN2, 6 )
	PORT_BIT(  0x0200, IP_ACTIVE_LOW,  IPT_SERVICE1 )
	PORT_BIT(  0x0400, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT(  0x0800, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* EEPROM bit */
	PORT_BIT(  0xf000, IP_ACTIVE_LOW,  IPT_UNKNOWN )
INPUT_PORTS_END

/* ------------------------------------------------------------------------ */
/* Video                                                                    */
/* ------------------------------------------------------------------------ */

/*	Palette (0x2000 colors) as addressed by the original colortable:
	sprites   color*16 + pen          (4 bit)
	layer 0   0x400 + color*16 + pen  (4 bit)
	layer 1   0x800 + color*16 + pen  (4 bit)
	layer 2   0xc00 + (color&15)*64 + pen  (6 bit)
	background 0x7f0 */

struct sm_sprite
{
	int priority, flipx, flipy;
	const UINT8 *pen_data;
	int line_offset;
	int pal_base;
	int tile_width, tile_height;
	int total_width, total_height;
	int x, y, xcount0, ycount0;
	int zoomx_re, zoomy_re;
};

#define MAX_SPRITES 0x400
static struct sm_sprite *sprites;
static int num_sprites;
static UINT16 *sprite_zbuf;		/* sprite index + 1 of the frontmost sprite pixel */
static int clip_l, clip_r, clip_t, clip_b;	/* clip_r / clip_b exclusive */

static void sm_build_sprites(void)
{
	const UINT8 *base_gfx = memory_region(REGION_GFX1);
	int code_max = memory_region_length(REGION_GFX1) / (16*16);
	int bank = VREG(4) & 1;
	const unsigned char *source = sm_spriteram + (spriteram_size / 2) * bank;
	const unsigned char *finish = source + (spriteram_size / 2);
	struct sm_sprite *sprite = sprites;
	int glob_flipx = VREG(0) & 0x8000;
	int glob_flipy = VREG(1) & 0x8000;
	int max_x = Machine->drv->screen_width;
	int max_y = Machine->drv->screen_height;

	if (!sprites || !base_gfx) return;

	for (; source < finish; source += 0x10)
	{
		int x, y, attr, code, zoomx, zoomy, size, flipx, flipy;

		x		= (READ_WORD(&source[0x0]) & 0x3ff) << 8;
		y		= (READ_WORD(&source[0x2]) & 0x3ff) << 8;
		attr	= READ_WORD(&source[0x4]);
		code	= READ_WORD(&source[0x6]) + ((attr & 3) << 16);
		zoomx	= READ_WORD(&source[0x8]);
		zoomy	= READ_WORD(&source[0xa]);
		size	= READ_WORD(&source[0xc]);

		sprite->tile_width	= ((size >> 8) & 0x1f) * 16;
		sprite->tile_height	= ((size >> 0) & 0x1f) * 16;
		if (!sprite->tile_width || !sprite->tile_height || !zoomx || !zoomy) continue;

		code %= code_max;
		sprite->pen_data = base_gfx + (16*16) * code;
		/* keep the whole sprite inside the region */
		if (sprite->pen_data + sprite->tile_width * sprite->tile_height > base_gfx + memory_region_length(REGION_GFX1))
			continue;

		flipx = attr & 0x0008;
		flipy = attr & 0x0004;

		sprite->total_width  = (sprite->tile_width  * zoomx) / 0x100;
		sprite->total_height = (sprite->tile_height * zoomy) / 0x100;

		if (sprite->total_width <= 1)
		{
			sprite->total_width = 1;
			sprite->zoomx_re = sprite->tile_width << 16;
			sprite->xcount0 = sprite->zoomx_re / 2;
			x -= 0x80;
		}
		else
		{
			sprite->zoomx_re = 0x1000000 / zoomx;
			sprite->xcount0 = sprite->zoomx_re - 1;
		}

		if (sprite->total_height <= 1)
		{
			sprite->total_height = 1;
			sprite->zoomy_re = sprite->tile_height << 16;
			sprite->ycount0 = sprite->zoomy_re / 2;
			y -= 0x80;
		}
		else
		{
			sprite->zoomy_re = 0x1000000 / zoomy;
			sprite->ycount0 = sprite->zoomy_re - 1;
		}

		x >>= 8;
		y >>= 8;
		if (flipx && (zoomx != 0x100)) x += sprite->tile_width - sprite->total_width;
		if (flipy && (zoomy != 0x100)) y += sprite->tile_height - sprite->total_height;

		if (x > 0x1ff) x -= 0x400;
		if (y > 0x1ff) y -= 0x400;

		if (x + sprite->total_width <= 0 || x >= max_x || y + sprite->total_height <= 0 || y >= max_y)
			continue;

		sprite->priority	= (attr & 0x0030) >> 4;
		sprite->line_offset	= sprite->tile_width;
		sprite->pal_base	= ((attr >> 8) & 0x3f) * 16;

		if (glob_flipx) { x = max_x - x - sprite->total_width;  flipx = !flipx; }
		if (glob_flipy) { y = max_y - y - sprite->total_height; flipy = !flipy; }

		sprite->x = x;
		sprite->y = y;
		sprite->flipx = flipx ? 1 : 0;
		sprite->flipy = flipy ? 1 : 0;

		sprite++;
	}
	num_sprites = sprite - sprites;
}

/* zooming sprite blitter (MAME 0.78 do_blit_zoom16_cave_zb, also used unzoomed) */
static void blit_sprite(struct osd_bitmap *bitmap, const struct sm_sprite *sprite, UINT16 pri_sp)
{
	int x1, x2, y1, y2, dx, dy;
	int xcount0 = 0x10000 + sprite->xcount0, ycount0 = 0x10000 + sprite->ycount0;
	const UINT16 *pens = Machine->pens + sprite->pal_base;

	if (sprite->flipx)
	{
		x2 = sprite->x;
		x1 = x2 + sprite->total_width;
		dx = -1;
		if (x2 < clip_l) x2 = clip_l;
		if (x1 > clip_r)
		{
			xcount0 += (x1 - clip_r) * sprite->zoomx_re;
			x1 = clip_r;
			while ((xcount0 & 0xffff) >= sprite->zoomx_re) { xcount0 += sprite->zoomx_re; x1--; }
		}
		if (x2 >= x1) return;
		x1--; x2--;
	}
	else
	{
		x1 = sprite->x;
		x2 = x1 + sprite->total_width;
		dx = 1;
		if (x1 < clip_l)
		{
			xcount0 += (clip_l - x1) * sprite->zoomx_re;
			x1 = clip_l;
			while ((xcount0 & 0xffff) >= sprite->zoomx_re) { xcount0 += sprite->zoomx_re; x1++; }
		}
		if (x2 > clip_r) x2 = clip_r;
		if (x1 >= x2) return;
	}
	if (sprite->flipy)
	{
		y2 = sprite->y;
		y1 = y2 + sprite->total_height;
		dy = -1;
		if (y2 < clip_t) y2 = clip_t;
		if (y1 > clip_b)
		{
			ycount0 += (y1 - clip_b) * sprite->zoomy_re;
			y1 = clip_b;
			while ((ycount0 & 0xffff) >= sprite->zoomy_re) { ycount0 += sprite->zoomy_re; y1--; }
		}
		if (y2 >= y1) return;
		y1--; y2--;
	}
	else
	{
		y1 = sprite->y;
		y2 = y1 + sprite->total_height;
		dy = 1;
		if (y1 < clip_t)
		{
			ycount0 += (clip_t - y1) * sprite->zoomy_re;
			y1 = clip_t;
			while ((ycount0 & 0xffff) >= sprite->zoomy_re) { ycount0 += sprite->zoomy_re; y1++; }
		}
		if (y2 > clip_b) y2 = clip_b;
		if (y1 >= y2) return;
	}

	{
		const UINT8 *pen_data = sprite->pen_data - 1 - sprite->line_offset;
		const UINT8 *pen_end = memory_region(REGION_GFX1) + memory_region_length(REGION_GFX1);
		int width = Machine->drv->screen_width;
		int x, y;
		int ycount = ycount0;

		for (y = y1; y != y2; y += dy)
		{
			if (ycount & 0xffff0000)
			{
				int xcount = xcount0;
				const UINT8 *source;
				UINT16 *dest = (UINT16 *)bitmap->line[y];
				UINT16 *zbf = sprite_zbuf + y * width;

				pen_data += sprite->line_offset * (ycount >> 16);
				ycount &= 0xffff;
				source = pen_data;
				if (source + sprite->line_offset >= pen_end) return;
				for (x = x1; x != x2; x += dx)
				{
					if (xcount & 0xffff0000)
					{
						UINT8 pen;
						source += xcount >> 16;
						xcount &= 0xffff;
						pen = *source;
						if (pen && zbf[x] <= pri_sp)
						{
							dest[x] = pens[pen];
							zbf[x] = pri_sp;
						}
					}
					xcount += sprite->zoomx_re;
				}
			}
			ycount += sprite->zoomy_re;
		}
	}
}

static void draw_sprites(struct osd_bitmap *bitmap, int priority)
{
	int i;
	for (i = 0; i < num_sprites; i++)
		if (sprites[i].priority == priority)
			blit_sprite(bitmap, &sprites[i], i + 1);
}

/*	Layers: 512x512, tile RAM holds a 16x16 map at +0 and an 8x8 map at
	+0x4000 (4 bytes per tile: pri:2 color:6 code:24). Row scroll / row
	select values come from tile RAM +0x1000, 4 bytes per line. */

#define LAYERS_OFFS_X	0x13
#define LAYERS_OFFS_Y	(-0x12)
#define ROW_OFFS		(-1)

static void draw_layer(struct osd_bitmap *bitmap, int layer, int priority)
{
	int v0 = VCTRL(layer, 0), v1 = VCTRL(layer, 1);
	int tiledim = v1 & 0x2000;
	int offs_x = LAYERS_OFFS_X - (tiledim ? (1 + layer) : (1 + layer + 8));
	int sx = v0 - VREG(0) - offs_x;
	int sy = v1 - VREG(1) - LAYERS_OFFS_Y;
	int rowscroll = v0 & 0x4000, rowselect = v1 & 0x4000;
	const UINT8 *gfx = tiles_8bpp[layer];
	const UINT8 *blank = tiles_blank[layer];
	int count = tiles_count[layer];
	const UINT16 *pens = Machine->pens;
	int pal_base = layer == 0 ? 0x400 : layer == 1 ? 0x800 : 0xc00;
	int y;

	if (!gfx) return;

	for (y = clip_t; y < clip_b; y++)
	{
		int srcy, srcx, x;
		int line = (sy + ROW_OFFS + y) * 4;
		UINT16 *dest = (UINT16 *)bitmap->line[y];

		if (rowselect)	srcy = VRAMW(layer, 0x1002 + (line & 0x7ff));
		else			srcy = sy + y;
		srcy &= 511;

		srcx = sx + clip_l;
		if (rowscroll)	srcx += VRAMW(layer, 0x1000 + (line & 0x7ff));

		x = clip_l;
		while (x < clip_r)
		{
			int px = srcx & 511;
			int run = 8 - (px & 7);			/* pixels left in this 8x8 element */
			UINT32 code;
			int color, pri, elem;

			if (run > clip_r - x) run = clip_r - x;

			if (tiledim)
			{
				int t = (px >> 4) + (srcy >> 4) * 32;
				code = (VRAMW(layer, t * 4) << 16) | VRAMW(layer, t * 4 + 2);
				elem = (code & 0x00ffffff) * 4 + ((px >> 3) & 1) + ((srcy >> 3) & 1) * 2;
			}
			else
			{
				int t = (px >> 3) + (srcy >> 3) * 64;
				code = (VRAMW(layer, 0x4000 + t * 4) << 16) | VRAMW(layer, 0x4000 + t * 4 + 2);
				elem = code & 0x00ffffff;
				if (layer == 2 && elem < 0x10000 && sm_tilebank) elem += 0x40000;
			}
			pri = code >> 30;
			color = (code >> 24) & 0x3f;

			if (pri == priority)
			{
				elem %= count;
				if (!blank[elem])
				{
					const UINT8 *src = gfx + elem * 64 + (srcy & 7) * 8 + (px & 7);
					const UINT16 *pal = pens + pal_base + (layer == 2 ? (color & 15) * 64 : color * 16);
					int i;
					for (i = 0; i < run; i++)
					{
						UINT8 pen = src[i];
						if (pen) dest[x + i] = pal[pen];
					}
				}
			}
			x += run;
			srcx += run;
		}
	}
}

static void decode_tiles(void)
{
	int layer;

	for (layer = 0; layer < 3; layer++)
	{
		int region = REGION_GFX2 + layer;
		const UINT8 *rom = memory_region(region);
		int len = memory_region_length(region);
		int n, e, i;

		if (layer < 2)	n = len / 32;		/* 4 bit: 32 bytes per 8x8 */
		else			n = (len / 2) / 32;	/* 4 bit plane + unpacked 2 bit plane */

		tiles_count[layer] = n;
		tiles_8bpp[layer] = (UINT8 *)malloc(n * 64);
		tiles_blank[layer] = (UINT8 *)malloc(n);
		if (!tiles_8bpp[layer] || !tiles_blank[layer] || !rom) continue;

		for (e = 0; e < n; e++)
		{
			UINT8 *dst = tiles_8bpp[layer] + e * 64;
			const UINT8 *lo = rom + e * 32;
			int any = 0;
			for (i = 0; i < 64; i++)
			{
				int pix = (i & 1) ? (lo[i >> 1] & 0x0f) : (lo[i >> 1] >> 4);
				if (layer == 2)
				{
					/* 2 bit part, as the 4 bit layout MAME unpacks it to */
					int nib = e * 64 + i;
					const UINT8 *hi = rom + len / 2;
					pix |= ((hi[nib >> 2] >> ((nib & 3) * 2)) & 3) << 4;
				}
				dst[i] = pix;
				any |= pix;
			}
			tiles_blank[layer][e] = any ? 0 : 1;
		}
	}
}

static void sm_vh_stop(void);

static int sm_vh_start(void)
{
	int r;

	sprites = (struct sm_sprite *)malloc(MAX_SPRITES * sizeof(struct sm_sprite));
	sprite_zbuf = (UINT16 *)malloc(Machine->drv->screen_width * Machine->drv->screen_height * sizeof(UINT16));
	num_sprites = 0;
	if (!sprites || !sprite_zbuf) { sm_vh_stop(); return 1; }

	decode_tiles();
	for (r = 0; r < 3; r++)
		if (!tiles_8bpp[r] || !tiles_blank[r]) { sm_vh_stop(); return 1; }

	/* the layer ROMs are not needed any more */
	for (r = 0; r < MAX_MEMORY_REGIONS; r++)
	{
		int type = Machine->memory_region_type[r] & ~REGIONFLAG_MASK;
		if (Machine->memory_region[r] && type >= REGION_GFX2 && type <= REGION_GFX4)
		{
			free(Machine->memory_region[r]);
			Machine->memory_region[r] = 0;
		}
	}
	return 0;
}

static void sm_vh_stop(void)
{
	int r;
	for (r = 0; r < 3; r++)
	{
		free(tiles_8bpp[r]);	tiles_8bpp[r] = 0;
		free(tiles_blank[r]);	tiles_blank[r] = 0;
	}
	free(sprites);		sprites = 0;
	free(sprite_zbuf);	sprite_zbuf = 0;
	num_sprites = 0;
}

static void sm_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	int pri, pri2, layer, y, x;
	UINT16 bg = Machine->pens[0x7f0];

	clip_l = Machine->visible_area.min_x;
	clip_r = Machine->visible_area.max_x + 1;
	clip_t = Machine->visible_area.min_y;
	clip_b = Machine->visible_area.max_y + 1;

	for (y = clip_t; y < clip_b; y++)
	{
		UINT16 *d = (UINT16 *)bitmap->line[y];
		for (x = clip_l; x < clip_r; x++) d[x] = bg;
	}
	memset(sprite_zbuf, 0, Machine->drv->screen_width * Machine->drv->screen_height * sizeof(UINT16));

	/*	Tiles and sprites are ordered by priority (0 back, 3 front), sprites
		below tiles of the same priority. Tiles of the same priority are
		ordered by the priority of their layer, then by layer number. */
	for (pri = 0; pri <= 3; pri++)
	{
		draw_sprites(bitmap, pri);
		for (pri2 = 0; pri2 <= 3; pri2++)
			for (layer = 0; layer < 3; layer++)
			{
				int c2 = VCTRL(layer, 2);
				if ((c2 & 0x0003) != pri2 || (c2 & 0x0010)) continue;
				draw_layer(bitmap, layer, pri);
			}
	}
}

/* ------------------------------------------------------------------------ */
/* Machine driver                                                           */
/* ------------------------------------------------------------------------ */

static void irqhandler(int irq)
{
	cpu_set_irq_line(1, 0, irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM2151interface ym2151_interface =
{
	1,
	16000000/4,
	{ YM3012_VOL(30,MIXER_PAN_LEFT,30,MIXER_PAN_RIGHT) },
	{ irqhandler },
	{ 0 }
};

static struct OKIM6295interface okim6295_interface =
{
	2,
	{ 16000, 16000 },
	{ REGION_SOUND1, REGION_SOUND2 },
	{ 100, 100 }
};

static struct MachineDriver machine_driver_sailormn =
{
	{
		{
			CPU_M68000,
			16000000,
			sm_readmem, sm_writemem, 0, 0,
			sm_interrupt, 1
		},
		{
			CPU_Z80,
			8000000,
			sm_sound_readmem, sm_sound_writemem, sm_sound_readport, sm_sound_writeport,
			ignore_interrupt, 1		/* NMI on sound command, IRQ from the YM2151 */
		},
	},
	15625/271.5, DEFAULT_60HZ_VBLANK_DURATION,
	1,
	sm_init_machine,

	/* video hardware */
	320+1, 240, { 0+1, 320+1-1, 0, 240-1 },
	0,
	0x2000, 0,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	sm_vh_start,
	sm_vh_stop,
	sm_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{ SOUND_YM2151,   &ym2151_interface },
		{ SOUND_OKIM6295, &okim6295_interface }
	},

	sm_nvram_handler
};

/* ------------------------------------------------------------------------ */
/* ROMs                                                                     */
/* ------------------------------------------------------------------------ */

ROM_START( sailormn )
	ROM_REGION( 0x400000, REGION_CPU1 )		/* 68000 code */
	ROM_LOAD_WIDE_SWAP( "bpsm945a.u45", 0x000000, 0x080000, 0x898c9515 )
	ROM_LOAD_WIDE_SWAP( "bpsm.u46",     0x200000, 0x200000, 0x32084e80 )

	ROM_REGION( 0x88000, REGION_CPU2 )		/* Z80 code */
	ROM_LOAD( "bpsm945a.u9",  0x00000, 0x08000, 0x438de548 )
	ROM_CONTINUE(             0x10000, 0x78000 )

	ROM_REGION( 0x400000 * 2, REGION_GFX1 )	/* sprites (encrypted, unpacked to 8 bit) */
	ROM_LOAD( "bpsm.u76", 0x000000, 0x200000, 0xa243a5ba )
	ROM_LOAD( "bpsm.u77", 0x200000, 0x200000, 0x5179a4ac )

	ROM_REGION( 0x200000, REGION_GFX2 )		/* layer 0 */
	ROM_LOAD( "bpsm.u53", 0x000000, 0x200000, 0xb9b15f83 )

	ROM_REGION( 0x200000, REGION_GFX3 )		/* layer 1 */
	ROM_LOAD( "bpsm.u54", 0x000000, 0x200000, 0x8f00679d )

	ROM_REGION( (5*0x200000)*2, REGION_GFX4 )	/* layer 2: 4 bit part, then 2 bit part */
	ROM_LOAD( "bpsm.u57", 0x000000, 0x200000, 0x86be7b63 )
	ROM_LOAD( "bpsm.u58", 0x200000, 0x200000, 0xe0bba83b )
	ROM_LOAD( "bpsm.u62", 0x400000, 0x200000, 0xa1e3bfac )
	ROM_LOAD( "bpsm.u61", 0x600000, 0x200000, 0x6a014b52 )
	ROM_LOAD( "bpsm.u60", 0x800000, 0x200000, 0x992468c0 )
	ROM_LOAD( "bpsm.u65", 0xa00000, 0x200000, 0xf60fb7b5 )
	ROM_LOAD( "bpsm.u64", 0xc00000, 0x200000, 0x6559d31c )
	ROM_LOAD( "bpsm.u63", 0xe00000, 0x100000, 0xd57a56b4 )	/* halves identical */
	ROM_CONTINUE(         0xe00000, 0x100000 )

	ROM_REGION( 0x240000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )	/* OKI #0, first 0x40000 = banked window */
	ROM_LOAD( "bpsm.u48", 0x040000, 0x200000, 0x498e4ed1 )

	ROM_REGION( 0x240000, REGION_SOUND2 | REGIONFLAG_SOUNDONLY )	/* OKI #1 */
	ROM_LOAD( "bpsm.u47", 0x040000, 0x080000, 0x0f2901b9 )
	ROM_RELOAD(           0x0c0000, 0x080000 )
	ROM_RELOAD(           0x140000, 0x080000 )
ROM_END

#define BITSWAP24_(val,B23,B22,B21,B20,B19,B18,B17,B16,B15,B14,B13,B12,B11,B10,B9,B8,B7,B6,B5,B4,B3,B2,B1,B0) \
	((((val) >> (B23)) & 1) << 23) | ((((val) >> (B22)) & 1) << 22) | ((((val) >> (B21)) & 1) << 21) | ((((val) >> (B20)) & 1) << 20) | \
	((((val) >> (B19)) & 1) << 19) | ((((val) >> (B18)) & 1) << 18) | ((((val) >> (B17)) & 1) << 17) | ((((val) >> (B16)) & 1) << 16) | \
	((((val) >> (B15)) & 1) << 15) | ((((val) >> (B14)) & 1) << 14) | ((((val) >> (B13)) & 1) << 13) | ((((val) >> (B12)) & 1) << 12) | \
	((((val) >> (B11)) & 1) << 11) | ((((val) >> (B10)) & 1) << 10) | ((((val) >> ( B9)) & 1) <<  9) | ((((val) >> ( B8)) & 1) <<  8) | \
	((((val) >> ( B7)) & 1) <<  7) | ((((val) >> ( B6)) & 1) <<  6) | ((((val) >> ( B5)) & 1) <<  5) | ((((val) >> ( B4)) & 1) <<  4) | \
	((((val) >> ( B3)) & 1) <<  3) | ((((val) >> ( B2)) & 1) <<  2) | ((((val) >> ( B1)) & 1) <<  1) | ((((val) >> ( B0)) & 1) <<  0)

void init_sailormn(void)
{
	UINT8 *src = memory_region(REGION_GFX1);
	int len = memory_region_length(REGION_GFX1);
	UINT8 *buffer;
	int i;

	/* decrypt sprites (only the loaded first half matters, bits 23-20 stay) */
	buffer = (UINT8 *)malloc(len / 2);
	if (buffer)
	{
		for (i = 0; i < len / 2; i++)
			buffer[i ^ 0x950c4] = src[BITSWAP24_(i,23,22,21,20,15,10,12,6,11,1,13,3,16,17,2,5,14,7,18,8,4,19,9,0)];
		memcpy(src, buffer, len / 2);
		free(buffer);
	}

	/* 4 bit -> 8 bit sprites, low nibble first */
	{
		UINT8 *s = src + len / 2 - 1;
		UINT8 *d = src + len - 1;
		while (d > s)
		{
			UINT8 data = *s--;
			*d-- = data >> 4;
			*d-- = data & 0xf;
		}
	}
}

GAME( 1995, sailormn, 0, sailormn, sailormn, sailormn, ROT0_16BIT, "Banpresto", "Pretty Soldier Sailor Moon (95/03/22B)" )
