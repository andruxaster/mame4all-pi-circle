/***************************************************************************

  Magic Card II (Impera, 1996) - Fun World / TAB / Impera "series 7000" hardware

  Back-port to the MAME 0.37b5 driver API for MAME4ALL (Circle bare-metal build).
  Based on the hardware description in the current MAME funworld driver
  (driver by Roberto Fresca and Peter Ferrie, BSD-3-Clause).

  Hardware:
    R65C02 @ 2 MHz (16 MHz / 8), NMI from the MC6845 VSYNC
    2x MC6821 PIA (inputs), AY-3-8910 @ 2 MHz (lamps on its ports - not emulated)
    2 KB NVRAM (0000-07FF), video RAM 4000-4FFF, colour RAM 5000-5FFF
    4bpp 4x8 tiles, 112x34 tilemap, 82S147 colour PROM

  The MC6845 and the PIAs are reduced to what the game needs: the CRTC
  registers are just latched, and the PIAs return the input ports through
  their data/DDR registers.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

#define FW_COLS		112
#define FW_ROWS		34

static unsigned char *fw_videoram;
static unsigned char *fw_colorram;
static unsigned char *fw_nvram;
static size_t fw_nvram_size;
static unsigned char fw_dirty[0x1000];
static unsigned char fw_crtc_addr, fw_crtc_regs[32];


/***************************************************************************
  Video
***************************************************************************/

/*  - bits -
    76543210
    GGBBBRRR  (R/B: 1K, 470, 220 ohm; G: 470, 220 ohm; 100 ohm pull-down) */
void funworld_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable, const unsigned char *color_prom)
{
	int i;
	for (i = 0; i < Machine->drv->total_colors; i++)
	{
		int c = color_prom[i];
		int r = 33 * ((c >> 0) & 1) + 71 * ((c >> 1) & 1) + 151 * ((c >> 2) & 1);
		int b = 33 * ((c >> 3) & 1) + 71 * ((c >> 4) & 1) + 151 * ((c >> 5) & 1);
		int g = 75 * ((c >> 6) & 1) + 160 * ((c >> 7) & 1);
		*palette++ = r > 255 ? 255 : r;
		*palette++ = g > 255 ? 255 : g;
		*palette++ = b > 255 ? 255 : b;
	}
	for (i = 0; i < Machine->drv->color_table_len; i++)
		colortable[i] = i;
}

static WRITE_HANDLER( funworld_videoram_w )
{
	if (fw_videoram[offset] != data)
	{
		fw_videoram[offset] = data;
		fw_dirty[offset] = 1;
	}
}

static WRITE_HANDLER( funworld_colorram_w )
{
	if (fw_colorram[offset] != data)
	{
		fw_colorram[offset] = data;
		fw_dirty[offset] = 1;
	}
}

static int funworld_vh_start(void)
{
	if ((tmpbitmap = bitmap_alloc(FW_COLS * 4, FW_ROWS * 8)) == 0)
		return 1;
	memset(fw_dirty, 1, sizeof fw_dirty);
	return 0;
}

static void funworld_vh_stop(void)
{
	bitmap_free(tmpbitmap);
	tmpbitmap = 0;
}

static void funworld_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	int offs;

	if (full_refresh)
		memset(fw_dirty, 1, sizeof fw_dirty);

	for (offs = 0; offs < FW_COLS * FW_ROWS; offs++)
	{
		if (fw_dirty[offs])
		{
			int code = fw_videoram[offs] + ((fw_colorram[offs] & 0x0f) << 8);
			int color = fw_colorram[offs] >> 4;
			int sx = offs % FW_COLS, sy = offs / FW_COLS;

			fw_dirty[offs] = 0;
			drawgfx(tmpbitmap, Machine->gfx[0], code, color, 0, 0, sx * 4, sy * 8,
					0, TRANSPARENCY_NONE, 0);
		}
	}

	copybitmap(bitmap, tmpbitmap, 0, 0, 0, 0, &Machine->visible_area, TRANSPARENCY_NONE, 0);
}


/***************************************************************************
  MC6845 (registers latched only) and MC6821 PIAs (input ports only)
***************************************************************************/

static WRITE_HANDLER( crtc_address_w ) { fw_crtc_addr = data & 0x1f; }
static WRITE_HANDLER( crtc_register_w ) { fw_crtc_regs[fw_crtc_addr] = data; }
static READ_HANDLER( crtc_register_r ) { return fw_crtc_regs[fw_crtc_addr]; }

struct fw_pia
{
	unsigned char ddr[2], out[2], ctl[2];
};
static struct fw_pia fw_pias[2];

static int pia_read(int which, int offset)
{
	struct fw_pia *p = &fw_pias[which];
	int side = (offset >> 1) & 1;

	if (offset & 1)
		return p->ctl[side] & 0x3f;		/* no IRQ flags */

	if (p->ctl[side] & 0x04)
	{
		int in = readinputport(which * 2 + side);
		return (in & ~p->ddr[side]) | (p->out[side] & p->ddr[side]);
	}
	return p->ddr[side];
}

static void pia_write(int which, int offset, int data)
{
	struct fw_pia *p = &fw_pias[which];
	int side = (offset >> 1) & 1;

	if (offset & 1)
		p->ctl[side] = data & 0x3f;
	else if (p->ctl[side] & 0x04)
		p->out[side] = data;
	else
		p->ddr[side] = data;
}

static READ_HANDLER( pia0_r ) { return pia_read(0, offset); }
static READ_HANDLER( pia1_r ) { return pia_read(1, offset); }
static WRITE_HANDLER( pia0_w ) { pia_write(0, offset, data); }
static WRITE_HANDLER( pia1_w ) { pia_write(1, offset, data); }

static void funworld_init_machine(void)
{
	memset(fw_pias, 0, sizeof fw_pias);
	fw_crtc_addr = 0;
}


/***************************************************************************
  NVRAM - defaults come from the dumped NVRAM image (REGION_USER2)
***************************************************************************/

static void funworld_nvram_handler(void *file, int read_or_write)
{
	if (read_or_write)
		osd_fwrite(file, fw_nvram, fw_nvram_size);
	else
	{
		if (file)
			osd_fread(file, fw_nvram, fw_nvram_size);
		else
			memcpy(fw_nvram, memory_region(REGION_USER2), fw_nvram_size);
	}
}


/***************************************************************************
  Memory map (magicrd2)
***************************************************************************/

static struct MemoryReadAddress magicrd2_readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x0800, 0x0803, pia0_r },
	{ 0x0a00, 0x0a03, pia1_r },
	{ 0x0c00, 0x0c00, AY8910_read_port_0_r },
	{ 0x0e01, 0x0e01, crtc_register_r },
	{ 0x2c00, 0x2cff, MRA_RAM },	/* protection range */
	{ 0x3600, 0x36ff, MRA_RAM },
	{ 0x3c00, 0x3cff, MRA_RAM },
	{ 0x4000, 0x4fff, MRA_RAM },
	{ 0x5000, 0x5fff, MRA_RAM },
	{ 0x6000, 0xffff, MRA_ROM },
	{ -1 }
};

static struct MemoryWriteAddress magicrd2_writemem[] =
{
	{ 0x0000, 0x07ff, MWA_RAM, &fw_nvram, &fw_nvram_size },
	{ 0x0800, 0x0803, pia0_w },
	{ 0x0a00, 0x0a03, pia1_w },
	{ 0x0c00, 0x0c00, AY8910_control_port_0_w },
	{ 0x0c01, 0x0c01, AY8910_write_port_0_w },
	{ 0x0e00, 0x0e00, crtc_address_w },
	{ 0x0e01, 0x0e01, crtc_register_w },
	{ 0x2c00, 0x2cff, MWA_RAM },
	{ 0x3600, 0x36ff, MWA_RAM },
	{ 0x3c00, 0x3cff, MWA_RAM },
	{ 0x4000, 0x4fff, funworld_videoram_w, &fw_videoram },
	{ 0x5000, 0x5fff, funworld_colorram_w, &fw_colorram },
	{ 0x6000, 0xffff, MWA_ROM },
	{ -1 }
};


/***************************************************************************
  Inputs  (port order: PIA0 A, PIA0 B, PIA1 A, PIA1 B)

  Keyboard: Z X C V B = Hold 1-5 (Hold 2 = Low, Hold 4 = High, Hold 5 = Stake),
            1 = Start, N = Clear/Take, 5/6 = coins, 9/0 = Service 1/2,
            Q = Remote (key in), W = Key out, H = Hopper switch
  Gamepad:  A B X Y LB = Hold 1-5, Start = Start, Down = Clear/Take, Select = Coin 1
***************************************************************************/

INPUT_PORTS_START( magicrd2 )
	PORT_START	/* IN0 */
	PORT_BITX( 0x01, IP_ACTIVE_LOW, IPT_SERVICE3, "Remote", KEYCODE_Q, IP_JOY_NONE )
	PORT_BITX( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1, "Hold 1", KEYCODE_Z, IP_JOY_DEFAULT )
	PORT_BITX( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN, "Clear / Take", KEYCODE_N, IP_JOY_DEFAULT )
	PORT_BITX( 0x08, IP_ACTIVE_LOW, IPT_START1, "Start", KEYCODE_1, IP_JOY_DEFAULT )
	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_BUTTON5, "Hold 5 / Stake", KEYCODE_B, IP_JOY_DEFAULT )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_SERVICE1, "Service 1", KEYCODE_9, IP_JOY_NONE )
	PORT_BITX( 0x40, IP_ACTIVE_LOW, IPT_SERVICE2, "Service 2", KEYCODE_0, IP_JOY_NONE )
	PORT_BITX( 0x80, IP_ACTIVE_LOW, IPT_BUTTON4, "Hold 4 / High", KEYCODE_V, IP_JOY_DEFAULT )

	PORT_START	/* IN1 */
	PORT_BITX( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2, "Hold 2 / Low", KEYCODE_X, IP_JOY_DEFAULT )
	PORT_BITX( 0x02, IP_ACTIVE_LOW, IPT_BUTTON3, "Hold 3", KEYCODE_C, IP_JOY_DEFAULT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_SERVICE, "Hopper SW", KEYCODE_H, IP_JOY_NONE )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX( 0x80, IP_ACTIVE_LOW, IPT_SERVICE4, "Key Out", KEYCODE_W, IP_JOY_NONE )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xfe, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW */
	PORT_DIPNAME( 0x01, 0x01, "State" )
	PORT_DIPSETTING(    0x00, "Keyboard Test" )
	PORT_DIPSETTING(    0x01, "Play" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, "5 Points/Coin" )
	PORT_DIPSETTING(    0x04, "10 Points/Coin" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, "5 Points/Coin" )
	PORT_DIPSETTING(    0x08, "10 Points/Coin" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Joker" )
	PORT_DIPSETTING(    0x00, "With Joker" )
	PORT_DIPSETTING(    0x20, "Without Joker" )
	PORT_DIPNAME( 0x40, 0x00, "Hold" )
	PORT_DIPSETTING(    0x00, "Auto Hold" )
	PORT_DIPSETTING(    0x40, "No Auto Hold" )
	PORT_DIPNAME( 0x80, 0x00, "Payout" )
	PORT_DIPSETTING(    0x00, "Hopper" )
	PORT_DIPSETTING(    0x80, "Manual Payout SW" )
INPUT_PORTS_END


/***************************************************************************
  Graphics / machine
***************************************************************************/

static struct GfxLayout charlayout =
{
	4, 8,
	RGN_FRAC(1,2),
	4,
	{ RGN_FRAC(0,2), RGN_FRAC(0,2) + 4, RGN_FRAC(1,2), RGN_FRAC(1,2) + 4 },
	{ 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*4*2
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout, 0, 16 },
	{ -1 }
};

static struct AY8910interface ay8910_interface =
{
	1,
	2000000,	/* 16 MHz / 8 */
	{ 50 },
	{ 0 },
	{ 0 },
	{ 0 },	/* port A: lamps */
	{ 0 }	/* port B: lamps */
};

static struct MachineDriver machine_driver_magicrd2 =
{
	{
		{
			CPU_M65C02,
			2000000,	/* 16 MHz / 8 */
			magicrd2_readmem, magicrd2_writemem, 0, 0,
			nmi_interrupt, 1	/* MC6845 VSYNC */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,
	funworld_init_machine,

	/* video hardware */
	FW_COLS*4, FW_ROWS*8, { 0, FW_COLS*4-56-1, 0, FW_ROWS*8-1 },
	gfxdecodeinfo,
	256, 256,
	funworld_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	funworld_vh_start,
	funworld_vh_stop,
	funworld_vh_screenrefresh,

	/* sound hardware */
	0, 0, 0, 0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	},

	funworld_nvram_handler
};


/***************************************************************************
  ROMs
***************************************************************************/

ROM_START( magicrd2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* magicard.004 has extra code */
	ROM_LOAD( "magicard.004", 0x0000, 0x8000, 0xf6e948b8 )

	ROM_REGION( 0x10000, REGION_USER1 )	/* magicard.01: both 32K halves identical */
	ROM_LOAD( "magicard.01", 0x0000, 0x10000, 0xc94767d4 )

	ROM_REGION( 0x10000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mc2gr2.bin", 0x0000, 0x8000, 0x733da697 )
	ROM_LOAD( "mc2gr1.bin", 0x8000, 0x8000, 0x2406b8d2 )

	ROM_REGION( 0x0800, REGION_USER2 )	/* default NVRAM */
	ROM_LOAD( "magicrd2_nvram.bin", 0x0000, 0x0800, 0x343b3162 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "mc2-82s147.bin", 0x0000, 0x0200, 0xaa91cc35 )
ROM_END

static void init_magicrd2(void)
{
	memcpy(memory_region(REGION_CPU1) + 0x8000, memory_region(REGION_USER1), 0x8000);
}

GAME( 1996, magicrd2, 0, magicrd2, magicrd2, magicrd2, ROT0, "Impera", "Magic Card II (Bulgarian)" )
