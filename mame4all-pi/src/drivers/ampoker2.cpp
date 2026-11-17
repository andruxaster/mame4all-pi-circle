/***************************************************************************

  American Poker II (Novomatic, 1990)

  Back-port to the MAME 0.37b5 driver API for MAME4ALL (Circle bare-metal build).
  Based on the hardware description in the current MAME ampoker2 driver
  (driver by Roberto Fresca, BSD-3-Clause).

  Hardware:
    Z80 @ 3 MHz (6 MHz / 2), NMI @ 1536 Hz
    AY-3-8910 @ 1.5 MHz (ports 38/39 write, 3A read)
    2 KB NVRAM (C000-C7FF), 4 KB video RAM (E000-EFFF, 64x32 cells x 2 bytes)
    2bpp 8x8 tiles, 82S147 colour PROM (512 entries, RRRGGGBB)

  Lamps, counters and hopper are not emulated.

  First boot (empty NVRAM) shows "INIT MACHINE". To init the machine:
    1) turn ON the Operator key (9),
    2) keep the Door key (O) pressed until the Operator mode appears,
    3) turn OFF the Operator key (9).

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static unsigned char *ampoker2_videoram;
static unsigned char *ampoker2_nvram;
static size_t ampoker2_nvram_size;
static unsigned char ampoker2_dirty[64 * 32];


/***************************************************************************
  Video
***************************************************************************/

/*  - bits -
    76543210
    RRRGGGBB  (R/G: 1K, 470, 220 ohm; B: 470, 220 ohm; no pull-down) */
void ampoker2_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable, const unsigned char *color_prom)
{
	int i;
	for (i = 0; i < Machine->drv->total_colors; i++)
	{
		int c = color_prom[i];
		int r = 33 * ((c >> 5) & 1) + 71 * ((c >> 6) & 1) + 151 * ((c >> 7) & 1);
		int g = 33 * ((c >> 2) & 1) + 71 * ((c >> 3) & 1) + 151 * ((c >> 4) & 1);
		int b = 81 * ((c >> 0) & 1) + 174 * ((c >> 1) & 1);
		*palette++ = r > 255 ? 255 : r;
		*palette++ = g > 255 ? 255 : g;
		*palette++ = b > 255 ? 255 : b;
	}
	for (i = 0; i < Machine->drv->color_table_len; i++)
		colortable[i] = i;
}

static WRITE_HANDLER( ampoker2_videoram_w )
{
	if (ampoker2_videoram[offset] != data)
	{
		ampoker2_videoram[offset] = data;
		ampoker2_dirty[offset >> 1] = 1;
	}
}

static int ampoker2_vh_start(void)
{
	if ((tmpbitmap = bitmap_alloc(Machine->drv->screen_width, Machine->drv->screen_height)) == 0)
		return 1;
	memset(ampoker2_dirty, 1, sizeof ampoker2_dirty);
	return 0;
}

static void ampoker2_vh_stop(void)
{
	bitmap_free(tmpbitmap);
	tmpbitmap = 0;
}

static void ampoker2_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	int offs;

	if (full_refresh)
		memset(ampoker2_dirty, 1, sizeof ampoker2_dirty);

	for (offs = 0; offs < 64 * 32; offs++)
	{
		if (ampoker2_dirty[offs])
		{
			int attr = ampoker2_videoram[offs * 2 + 1];
			int code = ampoker2_videoram[offs * 2] + 256 * (attr & 0x03);
			int color = attr >> 1;
			int sx = offs % 64, sy = offs / 64;

			ampoker2_dirty[offs] = 0;
			drawgfx(tmpbitmap, Machine->gfx[0], code, color, 0, 0, sx * 8, sy * 8,
					0, TRANSPARENCY_NONE, 0);
		}
	}

	copybitmap(bitmap, tmpbitmap, 0, 0, 0, 0, &Machine->visible_area, TRANSPARENCY_NONE, 0);
}


/***************************************************************************
  NVRAM
***************************************************************************/

static void ampoker2_nvram_handler(void *file, int read_or_write)
{
	if (read_or_write)
		osd_fwrite(file, ampoker2_nvram, ampoker2_nvram_size);
	else
	{
		if (file)
			osd_fread(file, ampoker2_nvram, ampoker2_nvram_size);
		else
			memset(ampoker2_nvram, 0, ampoker2_nvram_size);
	}
}


/***************************************************************************
  Memory maps
***************************************************************************/

static WRITE_HANDLER( ampoker2_nop_w )
{
	/* ports 30-36: lamps, counters, hopper - not emulated */
}

/* port 37 bit 3 low = watchdog refresh (MAME 0.37 watchdog: 2 s instead of the real 200 ms) */
static WRITE_HANDLER( ampoker2_watchdog_w )
{
	if ((data & 0x08) == 0)
		watchdog_reset_w(0, 0);
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xe000, 0xefff, MRA_RAM },
	{ -1 }
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM, &ampoker2_nvram, &ampoker2_nvram_size },
	{ 0xe000, 0xefff, ampoker2_videoram_w, &ampoker2_videoram },
	{ -1 }
};

static struct IOReadPort readport[] =
{
	{ 0x10, 0x10, input_port_0_r },
	{ 0x11, 0x11, input_port_1_r },
	{ 0x12, 0x12, input_port_2_r },
	{ 0x13, 0x13, input_port_3_r },
	{ 0x14, 0x14, input_port_4_r },
	{ 0x15, 0x15, input_port_5_r },
	{ 0x16, 0x16, input_port_6_r },
	{ 0x17, 0x17, input_port_7_r },
	{ 0x3a, 0x3a, AY8910_read_port_0_r },
	{ -1 }
};

static struct IOWritePort writeport[] =
{
	{ 0x08, 0x0f, ampoker2_nop_w },
	{ 0x21, 0x21, ampoker2_nop_w },
	{ 0x30, 0x36, ampoker2_nop_w },
	{ 0x37, 0x37, ampoker2_watchdog_w },
	{ 0x38, 0x38, AY8910_control_port_0_w },
	{ 0x39, 0x39, AY8910_write_port_0_w },
	{ -1 }
};


/***************************************************************************
  Inputs

  Keyboard: Z X C V B = Hold 1-5, 1 = Deal/Take, N = Bet/Red, M = Black,
            Q = Payout, 5/6/7/8 = coins, 9 = Operator key, 0 = Supervisor key,
            O = Door, 4 = Clear credits
  Gamepad:  A B X Y LB = Hold 1-5, RB = Bet/Red, Left = Black, Down = Payout,
            Start = Deal/Take, Select = Coin 1
***************************************************************************/

#define AP2_UNK_DIP(mask) \
	PORT_DIPNAME( mask, mask, DEF_STR( Unknown ) ) \
	PORT_DIPSETTING(    mask, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

INPUT_PORTS_START( ampoker2 )
	PORT_START	/* IN0 - port 10 */
	AP2_UNK_DIP( 0x01 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )		/* Hopper 1 */
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	AP2_UNK_DIP( 0x08 )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_COIN3, 2 )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1 - port 11 */
	AP2_UNK_DIP( 0x01 )
	PORT_BITX( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1, "Hold 1", KEYCODE_Z, IP_JOY_DEFAULT )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 | IPF_TOGGLE, "Operator Key", KEYCODE_9, IP_JOY_NONE )
	PORT_DIPNAME( 0x08, 0x08, "Remote Mode" )
	PORT_DIPSETTING(    0x08, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_BUTTON4, "Hold 4", KEYCODE_V, IP_JOY_DEFAULT )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN2 - port 12 */
	AP2_UNK_DIP( 0x01 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )		/* RTS */
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE3, "Door Switch", KEYCODE_O, IP_JOY_NONE )
	AP2_UNK_DIP( 0x08 )
	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN, "Payout", KEYCODE_Q, IP_JOY_DEFAULT )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN3 - port 13 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )		/* Hopper out */
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 | IPF_TOGGLE, "Supervisor Key", KEYCODE_0, IP_JOY_NONE )
	PORT_DIPNAME( 0x08, 0x08, "Remote Credits" )
	PORT_DIPSETTING(    0x08, "Cred x 100 (Mode 1) / x 20 (Mode 2)" )
	PORT_DIPSETTING(    0x00, "Cred x 50 (Mode 1) / Off (Mode 2)" )
	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT, "Black Card", KEYCODE_M, IP_JOY_DEFAULT )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN4 - port 14 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )		/* Hopper low */
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2, "Hold 2", KEYCODE_X, IP_JOY_DEFAULT )
	PORT_DIPNAME( 0x08, 0x08, "Auto Hold" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_START1, "Deal / Take", KEYCODE_1, IP_JOY_DEFAULT )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN5 - port 15 */
	AP2_UNK_DIP( 0x01 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )		/* Return line */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	AP2_UNK_DIP( 0x08 )
	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3, "Hold 3", KEYCODE_C, IP_JOY_DEFAULT )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN6 - port 16 */
	AP2_UNK_DIP( 0x01 )
	PORT_BITX( 0x02, IP_ACTIVE_LOW, IPT_BUTTON6, "Bet / Red Card", KEYCODE_N, IP_JOY_DEFAULT )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_BUTTON5, "Hold 5", KEYCODE_B, IP_JOY_DEFAULT )
	AP2_UNK_DIP( 0x08 )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN7 - port 17 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )		/* Remote credit */
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_COIN4, 2 )
	PORT_DIPNAME( 0x08, 0x08, "Jackpot" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_SERVICE4, "Clear Credits", KEYCODE_4, IP_JOY_NONE )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END


/***************************************************************************
  Graphics / machine
***************************************************************************/

static struct GfxLayout charlayout =
{
	8, 8,
	1024,
	2,
	{ 0, 4 },
	{ 0, 1, 2, 3, 8, 9, 10, 11 },
	{ 0*8, 2*8, 4*8, 6*8, 8*8, 10*8, 12*8, 14*8 },
	16*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout, 0, 128 },
	{ -1 }
};

static struct AY8910interface ay8910_interface =
{
	1,
	1500000,	/* 6 MHz / 4 */
	{ 40 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct MachineDriver machine_driver_ampoker2 =
{
	{
		{
			CPU_Z80,
			3000000,	/* 6 MHz / 2 */
			readmem, writemem, readport, writeport,
			ignore_interrupt, 1,
			nmi_interrupt, 1536
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	64*8, 32*8, { 20*8, 56*8-1, 2*8, 32*8-1 },
	gfxdecodeinfo,
	512, 512,
	ampoker2_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	ampoker2_vh_start,
	ampoker2_vh_stop,
	ampoker2_vh_screenrefresh,

	/* sound hardware */
	0, 0, 0, 0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	},

	ampoker2_nvram_handler
};


/***************************************************************************
  ROMs
***************************************************************************/

ROM_START( ampoker2 )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "poker9.002", 0x8000, 0x4000, 0xbfde5bce )

	ROM_REGION( 0x8000, REGION_USER1 )	/* the two 16K halves are swapped */
	ROM_LOAD( "poker9.003", 0x0000, 0x8000, 0xa31221fc )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "poker9.028", 0x0000, 0x4000, 0x65bccb40 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "82s147an.u48", 0x0000, 0x0200, 0x9bc8e543 )
ROM_END

static void init_ampoker2(void)
{
	unsigned char *cpu = memory_region(REGION_CPU1);
	unsigned char *src = memory_region(REGION_USER1);

	memcpy(cpu + 0x0000, src + 0x4000, 0x4000);
	memcpy(cpu + 0x4000, src + 0x0000, 0x4000);
}

GAME( 1990, ampoker2, 0, ampoker2, ampoker2, ampoker2, ROT0, "Novomatic", "American Poker II" )
