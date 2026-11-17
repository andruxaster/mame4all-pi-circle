#include "../machine/neogeo.cpp"
#include "../machine/pd4990a.cpp"
#include "../vidhrdw/neogeo.cpp"
#include "../machine/neocrypt.cpp"

/***************************************************************************
	M.A.M.E. Neo Geo driver presented to you by the Shin Emu Keikaku team.

	The following people have all spent probably far too much time on this:

    AVDB
    Bryan McPhail
    Fuzz
    Ernesto Corvi
    Andrew Prime


	TODO :
            - What does 0x3c0006-7 *REALLY* do?


known problems, some might have already been fixed:

ssideki2:
locks up at random times

gpilots:
bad sound (clipping and such)

popbounc:
hangs after a few seconds of gameplay if sound is off. Fixed with a patch.

neoturf:
There appear to be some bad/inverted palette shifts on the scorecards that
appear when you double/triple bogie.

lastbld2:
hangs during the intermission between level 1 and 2. If you skip the intermission
before it hangs, it goes on playing.

Se anche Sidekick 3 e 4 usano la stessa tecnica, allora e' evidente che il
campo si vede male perche' gli emu Neo Geo attualmente usano un engine "tile
by tile" e non "line by line", ed e' lo stesso motivo per cui ad esempio in
Fighter's History i fondali che dovrebbero essere prospettici restano
immobili (e credo sia anche lo stesso motivo per cui in Zed Blade i fondali
sono ugualmente immobili).

2020 Baseball = When the batter gets hit by the pitcher the game resets,
only on  arcade mode, in console mode it works fine

Ninja Combat = The music stops after the charge move is used ( the
dragon)

Ninja Commando = The music stops a little after the intro

magdrop2
In a two player game if P2 wins a game, then BOTH players are shown to lose, If P1 wins then P1 is correctly shown as the winner!!


So far Ninja Combat, Ninja Commando, Alpha Mission 2, and Blues Journey, still have sound problems, and Burning Fight
quits playing right after I choose my character. Am I the only one with these problems? Please let me know cause im trying to FIX

Shock Trooper get stucked after killing the first final enemy

blazstar,
Although the sound is perfect, there is still a problem
with parallax, in level 2.
Actually, the speed of scrolling should be twice as
fast as now.

samsho4,
I've just played Samurai Showdown 4, the level with the coast in the
background, and the water animation seems much to fast. I've made this
observation in at least another NeoGeo game (don't remember which
one anymore).

Shock Troopers - this has bad bugs. The game stops scrolling when you kill
the mid-boss on any of the 3 starting levels. There are also some
sprite/collision problems in the Jungle route. I'm told MAME32 works, so
perhaps this can still be fixed for the DOS final.

rbffspec
at the beginning of Hong Kong level, sky abruptly disappears while it scrolls up.

lastblad
rom M1 could be bad (second half is mostly FF, missing music in some levels)

in the game Super Spy, the white dialog box that appers when you are in a room
and somebody is talking at you, is missing. the text is black, and it is hard
to read because all the background is very dark. the dialog box missing has white colour.

---2020 Super Baseball---
Game resets when batter is hit by pitch.

---Cyberlip---
Crashes during the demo display (reported but I don't see it)



doubledr,
garbage sound during the movie in attract mode

kof94,
possible wrong scroll at the beginning of Japan match (abruptly clipped ground)

tpgolf:
clipped tiles at the sides of the screen during scrolling

mahretsu:
the song should stop while you are playing. You can hear the real sound
  effects if you continue a lost game.




ncombat,
sound hangs when you use the power up

ncommand,
no music and reduced sound effects during gameplay

savagere,
BAD clipping during intro


=============================================================================

Points to note, known and proven information deleted from this map:

	0x3000001		Dipswitches
				bit 0 : Selftest
				bit 1 : Unknown (Unused ?) \ something to do with
				bit 2 : Unknown (Unused ?) / auto repeating keys ?
				bit 3 : \
				bit 4 :  | communication setting ?
				bit 5 : /
				bit 6 : free play
				bit 7 : stop mode ?

	0x320001		bit 0 : COIN 1
				bit 1 : COIN 2
				bit 2 : SERVICE
				bit 3 : UNKNOWN
				bit 4 : UNKNOWN
				bit 5 : UNKNOWN
				bit 6 : 4990 test pulse bit.
				bit 7 : 4990 data bit.

	0x380051		4990 control write register
				bit 0: C0
				bit 1: C1
				bit 2: C2
				bit 3-7: unused.

				0x02 = shift.
				0x00 = register hold.
				0x04 = ????.
				0x03 = time read (reset register).

	0x3c000c		IRQ acknowledge

	0x380011		Backup bank select

	0x3a0001		Enable display.
	0x3a0011		Disable display

	0x3a0003		Swap in Bios (0x80 bytes vector table of BIOS)
	0x3a0013		Swap in Rom  (0x80 bytes vector table of ROM bank)

	0x3a000d		lock backup ram
	0x3a001d		unlock backup ram

	0x3a000b		set game vector table (?)  mirror ?
	0x3a001b		set bios vector table (?)  mirror ?

	0x3a000c		Unknown	(ghost pilots)
	0x31001c		Unknown (ghost pilots)

	IO word read

	0x3c0002		return vidram word (pointed to by 0x3c0000)
	0x3c0006		?????.
	0x3c0008		shadow adress for 0x3c0000 (not confirmed).
	0x3c000a		shadow adress for 0x3c0002 (confirmed, see
							   Puzzle de Pon).
	IO word write

	0x3c0006		Unknown, set vblank counter (?)

	0x3c0008		shadow address for 0x3c0000	(not confirmed)
	0x3c000a		shadow address for 0x3c0002	(not confirmed)

	The Neo Geo contains an NEC 4990 Serial I/O calendar & clock.
	accesed through 0x320001, 0x380050, 0x280050 (shadow adress).
	A schematic for this device can be found on the NEC webpages.

******************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/pd4990a.h"
#include "cpu/z80/z80.h"


#define RASTER_LINES 261	/* guess! */
#define FIRST_VISIBLE_LINE 16
#define LAST_VISIBLE_LINE 239
#define RASTER_VBLANK_END (RASTER_LINES-(LAST_VISIBLE_LINE-FIRST_VISIBLE_LINE+1))


extern unsigned char *vidram;
extern unsigned char *neogeo_ram;
extern unsigned char *neogeo_sram;

WRITE_HANDLER( neogeo_sram_lock_w );
WRITE_HANDLER( neogeo_sram_unlock_w );
READ_HANDLER( neogeo_sram_r );
WRITE_HANDLER( neogeo_sram_w );
void neogeo_nvram_handler(void *file,int read_or_write);

extern int	memcard_status;
READ_HANDLER( 	neogeo_memcard_r );
WRITE_HANDLER( neogeo_memcard_w );



/* from vidhrdw/neogeo.c */
void neogeo_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void neogeo_vh_raster_partial_refresh(struct osd_bitmap *bitmap,int current_line);
void neogeo_vh_raster_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
int  neogeo_mvs_vh_start(void);
void neogeo_vh_stop(void);
WRITE_HANDLER( neogeo_paletteram_w );
READ_HANDLER( neogeo_paletteram_r );
WRITE_HANDLER( neogeo_setpalbank0_w );
WRITE_HANDLER( neogeo_setpalbank1_w );

WRITE_HANDLER( neo_board_fix_w );
WRITE_HANDLER( neo_game_fix_w );

WRITE_HANDLER( vidram_modulo_w );
WRITE_HANDLER( vidram_data_w );
WRITE_HANDLER( vidram_offset_w );

READ_HANDLER( vidram_data_r );
READ_HANDLER( vidram_modulo_r );

/* debug, used to 'see' the locations mapped in ROM space */
/* with the debugger */
READ_HANDLER( mish_vid_r );
WRITE_HANDLER( mish_vid_w );
/* end debug */


/* from machine/neogeo.c */
void neogeo_init_machine(void);
void init_neogeo(void);


/******************************************************************************/

unsigned int neogeo_frame_counter;
unsigned int neogeo_frame_counter_speed=4;

/******************************************************************************/

static int irq2_enable;

static int neogeo_interrupt(void)
{
	static int fc=0;


	/* Add a timer tick to the pd4990a */
	addretrace();

	/* Animation counter, 1 once per frame is too fast, every 4 seems good */
        if  (fc>=neogeo_frame_counter_speed) {
                fc=0;
                neogeo_frame_counter++;
        }
        fc++;

	if (irq2_enable) cpu_cause_interrupt(0,2);

	/* return a standard vblank interrupt */
	return 1;      /* vertical blank */
}

static int irq2enable,irq2start,irq2repeat=1000,irq2control;
static int lastirq2line = 1000;

static int neogeo_raster_interrupt(void)
{
	static int fc=0;
	int line = RASTER_LINES - cpu_getiloops();
static int raster_enable=1;

	if (line == RASTER_LINES)	/* vblank */
	{
		if (keyboard_pressed_memory(KEYCODE_F1)) raster_enable ^= 1;

		lastirq2line = 1000;

		/* Add a timer tick to the pd4990a */
		addretrace();

		/* Animation counter, 1 once per frame is too fast, every 4 seems good */
		if  (fc >= neogeo_frame_counter_speed)
		{
			fc=0;
			neogeo_frame_counter++;
		}
		fc++;

		if (osd_skip_this_frame()==0)
			neogeo_vh_raster_partial_refresh(Machine->scrbitmap,line-RASTER_VBLANK_END+FIRST_VISIBLE_LINE-1);

		/* return a standard vblank interrupt */
//logerror("trigger IRQ1\n");
		return 1;      /* vertical blank */
	}

	if (irq2enable)
	{
		if (line == irq2start || line == lastirq2line + irq2repeat)
		{
//			logerror("trigger IRQ2 at raster line %d (screen line %d)\n",line,line-RASTER_VBLANK_END+FIRST_VISIBLE_LINE);
			if (raster_enable && osd_skip_this_frame()==0)
				neogeo_vh_raster_partial_refresh(Machine->scrbitmap,line-RASTER_VBLANK_END+FIRST_VISIBLE_LINE-1);

			lastirq2line = line;

			return 2;
		}
	}

	return 0;
}


static int pending_command;
static int result_code;

/* Calendar, coins + Z80 communication */
static READ_HANDLER( timer_r )
{
	int res;


	int coinflip = read_4990_testbit();
	int databit = read_4990_databit();

//	logerror("CPU %04x - Read timer\n",cpu_get_pc());

	res = readinputport(4) ^ (coinflip << 6) ^ (databit << 7);

	if (Machine->sample_rate)
	{
		res |= result_code << 8;
		if (pending_command) res &= 0x7fff;
	}
	else
		res |= 0x0100;

	return res;
}

static WRITE_HANDLER( neo_z80_w )
{
	soundlatch_w(0,(data>>8)&0xff);
	pending_command = 1;
	cpu_cause_interrupt(1,Z80_NMI_INT);
	/* spin for a while to let the Z80 read the command (fixes hanging sound in pspikes2) */
	cpu_spinuntil_time(TIME_IN_USEC(50));
}



int neogeo_has_trackball;
static int ts;

static WRITE_HANDLER( trackball_select_w )
{
	ts = data & 1;
}

static READ_HANDLER( controller1_r )
{
	int res;

	if (neogeo_has_trackball)
		res = (readinputport(ts?7:0) << 8) + readinputport(3);
	else
	{
		res = (readinputport(0) << 8) + readinputport(3);

		if (readinputport(7) & 0x01) res &= 0xcfff;	/* A+B */
		if (readinputport(7) & 0x02) res &= 0x3fff;	/* C+D */
		if (readinputport(7) & 0x04) res &= 0x8fff;	/* A+B+C */
		if (readinputport(7) & 0x08) res &= 0x0fff;	/* A+B+C+D */
	}

	return res;
}
static READ_HANDLER( controller2_r )
{
	int res;

	res = (readinputport(1) << 8);

	if (!neogeo_has_trackball)
	{
		if (readinputport(7) & 0x10) res &= 0xcfff;	/* A+B */
		if (readinputport(7) & 0x20) res &= 0x3fff;	/* C+D */
		if (readinputport(7) & 0x40) res &= 0x8fff;	/* A+B+C */
		if (readinputport(7) & 0x80) res &= 0x0fff;	/* A+B+C+D */
	}

	return res;
}
static READ_HANDLER( controller3_r )
{
	if (memcard_status==0)
		return (readinputport(2) << 8);
	else
		return ((readinputport(2) << 8)&0x8FFF);
}
static READ_HANDLER( controller4_r ) { return readinputport(6); }

static WRITE_HANDLER( neo_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int bankaddress;


	if (memory_region_length(REGION_CPU1) <= 0x100000)
	{
//logerror("warning: bankswitch to %02x but no banks available\n",data);
		return;
	}

	data = data&0x7;
	bankaddress = (data+1)*0x100000;
	if (bankaddress >= memory_region_length(REGION_CPU1))
	{
//logerror("PC %06x: warning: bankswitch to empty bank %02x\n",cpu_get_pc(),data);
		bankaddress = 0x100000;
	}

	cpu_setbank(4,&RAM[bankaddress]);
}



/* TODO: Figure out how this really works! */
static READ_HANDLER( neo_control_r )
{
	int line,irq_bit;

	/*
		The format of this very important location is:  AAAA AAAA B??? CDDD

		A is most likely the video beam line, however from how it is used it
		  doesn't seem to be a 0-255 direct map: the top bit is often masked out.
		  I think the top bit of A is: (vblank OR irq2). sdodgeb loops waiting for
		  it to be 1; zedblade heavily depends on it to work correctly.
		B is used together with A in one place, so most likely video beam position
		  Maybe AAAAAAAAB is a 9-bit video line counter.
		  It is tested individually in many cases (e.g. samsho3) so it might not be
		  the low bit of the raster line.
		C is definitely a PAL/NTSC flag. Evidence:
		  1) trally changes the position of the speed indicator depending on
			 it (0 = lower 1 = higher).
		  2) samsho3 sets a variable to 60 when the bit is 0 and 50 when it's 1.
			 This is obviously the video refresh rate in Hz.
		  3) samsho3 sets another variable to 256 or 307. This could be the total
			 screen height (including vblank), or close to that.
		  Some game (e.g. lstbld2, samsho3) do this (or similar):
		  bclr    #$0, $3c000e.l
		  when the bit is set, so 3c000e (whose function is unknown) has to be
		  related
		D is unknown (counter of some kind, used in a couple of places).
		  in blazstar, this controls the background speed in level 2.
	*/
//logerror("PC %06x: neo_control_r\n",cpu_get_pc());

	line = RASTER_LINES - cpu_getiloops();
	irq_bit = (irq2enable && (line == irq2start || line == lastirq2line + irq2repeat)) ||
		(line == RASTER_LINES);

	return  ((cpu_getscanline() * 0x80) & 0x7f80)	/* scanline */
			| (irq_bit << 15)						/* vblank or irq2 */
			| (neogeo_frame_counter & 0x0007);		/* frame counter */

#if 0
//logerror("PC %06x: read 0x3c0006\n",cpu_get_pc());
	switch(neogeo_game_fix)
	{
		case 0:
			return (neogeo_frame_counter) & 0x0007;                 /* Blazing Star */
		case 1:
			if (cpu_get_pc() == 0x1b04) return 0x8000; /* Fix for Voltage Fighter */
		case 2:
			return 0x2000;          /* real bout 2 */
		case 3:
			return 0x80;            /* sam sho3 */
		case 4:
			return 0xb801;      /* overtop */
		case 5:
			return 0x7000; /* Fix for KOF97 */
		case 6:
			return 0x8000; /* Money Idol Exchanger */
		case 8:
			return 0xffff; /* Ninja Command */
		case 9:
			return 0x4000; /* KOF98 */
	}
	return(0x8000);              /* anything 0x8000 seems better than 0*/
#endif
}


int neogeo_irq2type;
static int irq2repeat_limit;

/* this does much more than this, but I'm not sure exactly what */
WRITE_HANDLER( neo_control_w )
{
    /* Games which definitely need IRQ2:
    neocup98
	ssideki4
	ssideki3
	sengoku2
	spinmast
	ridhero
	turfmast
    karnovr
	galaxyfg
	zedblade
	mslug2 (dunes at the beginning)
	*/

    /* Auto-Anim Speed Control? */
    if((data & 0xf0ff) == 0)
    {
		int speed = (data >> 8) & 0x0f;
        if (speed) neogeo_frame_counter_speed=speed;
    }

    if (data & 0x10)
		irq2enable = 1;
    else
    {
		irq2enable = 0;
		lastirq2line = 1000;
		return;
    }

    if (data & 0x40)
		lastirq2line = 1000;

    irq2control = data & 0xff;

	/* ssideki2, zedblade and turfmast seem to be the only games to not set these
	  bits, and also the only ones to have an irq2repeat > 8. Coincidence?
	  */
	if (data & 0xc0)
		irq2repeat_limit = 16;
	else
		irq2repeat_limit = 29;
}

static WRITE_HANDLER( neo_irq2pos_w )
{
	static int value;
	int line;

	if (offset)
	{
		value = (value & 0xffff0000) | data;
		if (neogeo_irq2type) return;
	}
	else
	{
		value = (value & 0x0000ffff) | (data << 16);
		if (!neogeo_irq2type) return;
	}

	line = value / 0x180 + 1;
	if (line <= irq2repeat_limit) irq2repeat = line;
	/* ugly kludge to align irq2start in all games */
	else irq2start = line + (neogeo_irq2type);
}


/******************************************************************************/

static struct MemoryReadAddress neogeo_readmem[] =
{
	{ 0x000000, 0x0fffff, MRA_ROM },   /* Rom bank 1 */
	{ 0x100000, 0x10ffff, MRA_BANK1 }, /* Ram bank 1 */
	{ 0x200000, 0x2fffff, MRA_BANK4 }, /* Rom bank 2 */

	{ 0x300000, 0x300001, controller1_r },
	{ 0x300080, 0x300081, controller4_r }, /* Test switch in here */
	{ 0x320000, 0x320001, timer_r }, /* Coins, Calendar, Z80 communication */
	{ 0x340000, 0x340001, controller2_r },
	{ 0x380000, 0x380001, controller3_r },
	{ 0x3c0000, 0x3c0001, vidram_data_r },	/* Baseball Stars */
	{ 0x3c0002, 0x3c0003, vidram_data_r },
	{ 0x3c0004, 0x3c0005, vidram_modulo_r },

	{ 0x3c0006, 0x3c0007, neo_control_r },
	{ 0x3c000a, 0x3c000b, vidram_data_r },	/* Puzzle de Pon */

	{ 0x400000, 0x401fff, neogeo_paletteram_r },
	{ 0x600000, 0x61ffff, mish_vid_r },
	{ 0x800000, 0x800fff, neogeo_memcard_r }, /* memory card */
	{ 0xc00000, 0xc1ffff, MRA_BANK3 }, /* system bios rom */
	{ 0xd00000, 0xd0ffff, neogeo_sram_r }, /* 64k battery backed SRAM */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress neogeo_writemem[] =
{
	{ 0x000000, 0x0fffff, MWA_ROM },    /* ghost pilots writes to ROM */
	{ 0x100000, 0x10ffff, MWA_BANK1 },
/*	{ 0x200000, 0x200fff, whp copies ROM data here. Why? Is there RAM in the banked ROM space? */
/* trally writes to 200000-200003 as well, probably looking for a serial link */
/* both games write to 0000fe before writing to 200000. The two things could be related. */
/* sidkicks reads and writes to several addresses in this range, using this for copy */
/* protection. Custom parts instead of the banked ROMs? */
//	{ 0x280050, 0x280051, write_4990_control_w },
	{ 0x2ffff0, 0x2fffff, neo_bankswitch_w },      /* NOTE THIS CHANGE TO END AT FF !!! */
	{ 0x300000, 0x300001, watchdog_reset_w },
	{ 0x320000, 0x320001, neo_z80_w },	/* Sound CPU */
	{ 0x380000, 0x380001, trackball_select_w },	/* Used by bios, unknown */
	{ 0x380030, 0x380031, MWA_NOP },    /* Used by bios, unknown */
	{ 0x380040, 0x380041, MWA_NOP },	/* Output leds */
	{ 0x380050, 0x380051, write_4990_control_w },
	{ 0x380060, 0x380063, MWA_NOP },	/* Used by bios, unknown */
	{ 0x3800e0, 0x3800e3, MWA_NOP },	/* Used by bios, unknown */

	{ 0x3a0000, 0x3a0001, MWA_NOP },
	{ 0x3a0010, 0x3a0011, MWA_NOP },
	{ 0x3a0002, 0x3a0003, MWA_NOP },
	{ 0x3a0012, 0x3a0013, MWA_NOP },
	{ 0x3a000a, 0x3a000b, neo_board_fix_w }, /* Select board FIX char rom */
	{ 0x3a001a, 0x3a001b, neo_game_fix_w },  /* Select game FIX char rom */
	{ 0x3a000c, 0x3a000d, neogeo_sram_lock_w },
	{ 0x3a001c, 0x3a001d, neogeo_sram_unlock_w },
	{ 0x3a000e, 0x3a000f, neogeo_setpalbank1_w },
	{ 0x3a001e, 0x3a001f, neogeo_setpalbank0_w },    /* Palette banking */

	{ 0x3c0000, 0x3c0001, vidram_offset_w },
	{ 0x3c0002, 0x3c0003, vidram_data_w },
	{ 0x3c0004, 0x3c0005, vidram_modulo_w },

	{ 0x3c0006, 0x3c0007, neo_control_w },  /* See level 2 of spinmasters, rowscroll data? */
	{ 0x3c0008, 0x3c000b, neo_irq2pos_w },  /* IRQ2 x/y position? */

	{ 0x3c000c, 0x3c000d, MWA_NOP },	/* IRQ acknowledge */
										/* 4 = IRQ 1 */
										/* 2 = IRQ 2 */
										/* 1 = IRQ 3 (does any game use this?) */
//	{ 0x3c000e, 0x3c000f, }, /* Unknown, see control_r */

	{ 0x400000, 0x401fff, neogeo_paletteram_w },
	{ 0x600000, 0x61ffff, mish_vid_w },	/* Debug only, not part of real NeoGeo */
	{ 0x800000, 0x800fff, neogeo_memcard_w },	/* mem card */
	{ 0xd00000, 0xd0ffff, neogeo_sram_w, &neogeo_sram }, /* 64k battery backed SRAM */
	{ -1 }  /* end of table */
};

/******************************************************************************/

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK5 },
	{ 0xc000, 0xdfff, MRA_BANK6 },
	{ 0xe000, 0xefff, MRA_BANK7 },
	{ 0xf000, 0xf7ff, MRA_BANK8 },
	{ 0xf800, 0xffff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0xf7ff, MWA_ROM },
	{ 0xf800, 0xffff, MWA_RAM },
	{ -1 }	/* end of table */
};


static READ_HANDLER( z80_port_r )
{
	static int bank[4];


#if 0
{
	char buf[80];
	sprintf(buf,"%05x %05x %05x %05x",bank[0],bank[1],bank[2],bank[3]);
	usrintf_showmessage(buf);
}
#endif

	switch (offset & 0xff)
	{
		case 0x00:
			pending_command = 0;
			return soundlatch_r(0);
			break;

		case 0x04:
			return YM2610_status_port_0_A_r(0);
			break;

		case 0x05:
			return YM2610_read_port_0_r(0);
			break;

		case 0x06:
			return YM2610_status_port_0_B_r(0);
			break;

		case 0x08:
			{
				unsigned char *RAM = memory_region(REGION_CPU2);
				bank[3] = 0x0800 * ((offset >> 8) & 0x7f);
				cpu_setbank(8,&RAM[bank[3]]);
				return 0;
				break;
			}

		case 0x09:
			{
				unsigned char *RAM = memory_region(REGION_CPU2);
				bank[2] = 0x1000 * ((offset >> 8) & 0x3f);
				cpu_setbank(7,&RAM[bank[2]]);
				return 0;
				break;
			}

		case 0x0a:
			{
				unsigned char *RAM = memory_region(REGION_CPU2);
				bank[1] = 0x2000 * ((offset >> 8) & 0x1f);
				cpu_setbank(6,&RAM[bank[1]]);
				return 0;
				break;
			}

		case 0x0b:
			{
				unsigned char *RAM = memory_region(REGION_CPU2);
				bank[0] = 0x4000 * ((offset >> 8) & 0x0f);
				cpu_setbank(5,&RAM[bank[0]]);
				return 0;
				break;
			}

		default:
//logerror("CPU #1 PC %04x: read unmapped port %02x\n",cpu_get_pc(),offset&0xff);
			return 0;
			break;
	}
}

static WRITE_HANDLER( z80_port_w )
{
	switch (offset & 0xff)
	{
		case 0x04:
			YM2610_control_port_0_A_w(0,data);
			break;

		case 0x05:
			YM2610_data_port_0_A_w(0,data);
			break;

		case 0x06:
			YM2610_control_port_0_B_w(0,data);
			break;

		case 0x07:
			YM2610_data_port_0_B_w(0,data);
			break;

		case 0x08:
			/* NMI enable / acknowledge? (the data written doesn't matter) */
			break;

		case 0x0c:
			result_code = data;
			break;

		case 0x18:
			/* NMI disable? (the data written doesn't matter) */
			break;

		default:
//logerror("CPU #1 PC %04x: write %02x to unmapped port %02x\n",cpu_get_pc(),data,offset&0xff);
			break;
	}
}

static struct IOReadPort neo_readio[] =
{
	{ 0x0000, 0xffff, z80_port_r },
	{ -1 }
};

static struct IOWritePort neo_writeio[] =
{
	{ 0x0000, 0xffff, z80_port_w },
	{ -1 }
};

/******************************************************************************/

INPUT_PORTS_START( neogeo )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON4 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )   /* Player 1 Start */
	PORT_BITX( 0x02, IP_ACTIVE_LOW, 0, "SELECT 1",KEYCODE_6, IP_JOY_NONE ) /* Player 1 Select */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )   /* Player 2 Start */
	PORT_BITX( 0x08, IP_ACTIVE_LOW, 0, "SELECT 2",KEYCODE_7, IP_JOY_NONE ) /* Player 2 Select */
	PORT_BIT( 0x30, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* memory card inserted */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* memory card write protection */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_DIPNAME( 0x01, 0x01, "Test Switch" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Coin Chutes?" )
	PORT_DIPSETTING(    0x00, "1?" )
	PORT_DIPSETTING(    0x02, "2?" )
	PORT_DIPNAME( 0x04, 0x04, "Autofire (in some games)" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x38, 0x38, "COMM Setting" )
	PORT_DIPSETTING(    0x38, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x30, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Freeze" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 ) /* Service */

	/* Fake  IN 5 */
	PORT_START
	PORT_DIPNAME( 0x03, 0x02,"Territory" )
	PORT_DIPSETTING(    0x00,"Japan" )
	PORT_DIPSETTING(    0x01,"USA" )
	PORT_DIPSETTING(    0x02,"Europe" )
	PORT_DIPNAME( 0x04, 0x04,"Machine Mode" )
	PORT_DIPSETTING(    0x00,"Home" )
	PORT_DIPSETTING(    0x04,"Arcade" )

	PORT_START      /* Test switch */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )  /* This bit is used.. */
	PORT_BITX( 0x80, IP_ACTIVE_LOW, 0, "Test Switch", KEYCODE_F2, IP_JOY_NONE )

	PORT_START      /* FAKE */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON5 | IPF_CHEAT )	/* A+B */
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON6 | IPF_CHEAT )	/* C+D */
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON7 | IPF_CHEAT )	/* A+B+C */
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON8 | IPF_CHEAT )	/* A+B+C+D */
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON5 | IPF_CHEAT | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON6 | IPF_CHEAT | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON7 | IPF_CHEAT | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON8 | IPF_CHEAT | IPF_PLAYER2 )
INPUT_PORTS_END

INPUT_PORTS_START( irrmaze )
	PORT_START      /* IN0 multiplexed */
	PORT_ANALOG( 0xff, 0x7f, IPT_TRACKBALL_X | IPF_REVERSE, 10, 20, 0, 0 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )   /* Player 1 Start */
	PORT_BITX( 0x02, IP_ACTIVE_LOW, 0, "SELECT 1",KEYCODE_6, IP_JOY_NONE ) /* Player 1 Select */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )   /* Player 2 Start */
	PORT_BITX( 0x08, IP_ACTIVE_LOW, 0, "SELECT 2",KEYCODE_7, IP_JOY_NONE ) /* Player 2 Select */
	PORT_BIT( 0x30, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* memory card inserted */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* memory card write protection */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_DIPNAME( 0x01, 0x01, "Test Switch" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Coin Chutes?" )
	PORT_DIPSETTING(    0x00, "1?" )
	PORT_DIPSETTING(    0x02, "2?" )
	PORT_DIPNAME( 0x04, 0x04, "Autofire (in some games)" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x38, 0x38, "COMM Setting" )
	PORT_DIPSETTING(    0x38, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x30, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Freeze" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 ) /* Service */

	/* Fake  IN 5 */
	PORT_START
	PORT_DIPNAME( 0x03, 0x02,"Territory" )
	PORT_DIPSETTING(    0x00,"Japan" )
	PORT_DIPSETTING(    0x01,"USA" )
	PORT_DIPSETTING(    0x02,"Europe" )
	PORT_DIPNAME( 0x04, 0x04,"Machine Mode" )
	PORT_DIPSETTING(    0x00,"Home" )
	PORT_DIPSETTING(    0x04,"Arcade" )

	PORT_START      /* Test switch */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )  /* This bit is used.. */
	PORT_BITX( 0x80, IP_ACTIVE_LOW, 0, "Test Switch", KEYCODE_F2, IP_JOY_NONE )

	PORT_START      /* IN0 multiplexed */
	PORT_ANALOG( 0xff, 0x7f, IPT_TRACKBALL_Y | IPF_REVERSE, 10, 20, 0, 0 )
INPUT_PORTS_END


/******************************************************************************/

/* character layout (same for all games) */
static struct GfxLayout charlayout =	/* All games */
{
	8,8,            /* 8 x 8 chars */
	4096,           /* 4096 in total */
	4,              /* 4 bits per pixel */
	{ 0, 1, 2, 3 },    /* planes are packed in a nibble */
	{ 33*4, 32*4, 49*4, 48*4, 1*4, 0*4, 17*4, 16*4 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	32*8    /* 32 bytes per char */
};

/* Placeholder and also reminder of how this graphic format is put together */
static struct GfxLayout dummy_mvs_tilelayout =
{
	16,16,   /* 16*16 sprites */
	20,
	4,
	{ 3*8, 1*8, 2*8, 0*8 },     /* plane offset */
	{ 64*8+7, 64*8+6, 64*8+5, 64*8+4, 64*8+3, 64*8+2, 64*8+1, 64*8+0,
			7, 6, 5, 4, 3, 2, 1, 0 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },
	128*8    /* every sprite takes 128 consecutive bytes */
};

static struct GfxDecodeInfo neogeo_mvs_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x000000, &charlayout, 0, 16 },
	{ REGION_GFX1, 0x020000, &charlayout, 0, 16 },
	{ REGION_GFX1, 0x000000, &dummy_mvs_tilelayout, 0, 256 },  /* Placeholder */
	{ -1 } /* end of array */
};

/******************************************************************************/

static void neogeo_sound_irq( int irq )
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

struct YM2610interface neogeo_ym2610_interface =
{
	1,
	8000000,
	{ MIXERG(30,MIXER_GAIN_4x,MIXER_PAN_CENTER) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ neogeo_sound_irq },
	{ REGION_SOUND2 },
	{ REGION_SOUND1 },
	{ YM3012_VOL(50,MIXER_PAN_LEFT,50,MIXER_PAN_RIGHT) }
};

/******************************************************************************/

static struct MachineDriver machine_driver_neogeo =
{
	{
		{
			CPU_M68000,
			12000000,
			neogeo_readmem,neogeo_writemem,0,0,
			neogeo_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU | CPU_16BIT_PORT,
			6000000,
			sound_readmem,sound_writemem,neo_readio,neo_writeio,
			ignore_interrupt,0
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,
	neogeo_init_machine,
	40*8, 32*8, { 1*8, 39*8-1, 2*8, 30*8-1 },
	neogeo_mvs_gfxdecodeinfo,
	4096,4096,
	0,

	/* please don't put VIDEO_SUPPRTS_16BIT in all games. It is stupid, because */
	/* most games don't need it. Only put it in games that use more than 256 colors */
	/* at the same time (and let the MAME team know about it) */
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	neogeo_mvs_vh_start,
	neogeo_vh_stop,
	neogeo_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2610,
			&neogeo_ym2610_interface,
		},
	},

	neogeo_nvram_handler
};

static struct MachineDriver machine_driver_raster =
{
	{
		{
			CPU_M68000,
			12000000,
			neogeo_readmem,neogeo_writemem,0,0,
			neogeo_raster_interrupt,RASTER_LINES
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU | CPU_16BIT_PORT,
			6000000,
			sound_readmem,sound_writemem,neo_readio,neo_writeio,
			ignore_interrupt,0
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,
	neogeo_init_machine,
	40*8, 32*8, { 1*8, 39*8-1, FIRST_VISIBLE_LINE, LAST_VISIBLE_LINE },
	neogeo_mvs_gfxdecodeinfo,
	4096,4096,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	neogeo_mvs_vh_start,
	neogeo_vh_stop,
	neogeo_vh_raster_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2610,
			&neogeo_ym2610_interface,
		},
	},

	neogeo_nvram_handler
};

/* Metal Slug 3: 512 KB fix layer data (4 banks), BIOS fix data after it */
static struct GfxLayout charlayout_bigfix =
{
	8,8,
	16384,
	4,
	{ 0, 1, 2, 3 },
	{ 33*4, 32*4, 49*4, 48*4, 1*4, 0*4, 17*4, 16*4 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	32*8
};

static struct GfxDecodeInfo neogeo_bigfix_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x000000, &charlayout_bigfix, 0, 16 },
	{ REGION_GFX1, 0x080000, &charlayout, 0, 16 },
	{ REGION_GFX1, 0x000000, &dummy_mvs_tilelayout, 0, 256 },  /* Placeholder */
	{ -1 }
};

static struct MachineDriver machine_driver_neobigfix =
{
	{
		{
			CPU_M68000,
			12000000,
			neogeo_readmem,neogeo_writemem,0,0,
			neogeo_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU | CPU_16BIT_PORT,
			6000000,
			sound_readmem,sound_writemem,neo_readio,neo_writeio,
			ignore_interrupt,0
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,
	neogeo_init_machine,
	40*8, 32*8, { 1*8, 39*8-1, 2*8, 30*8-1 },
	neogeo_bigfix_gfxdecodeinfo,
	4096,4096,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	neogeo_mvs_vh_start,
	neogeo_vh_stop,
	neogeo_vh_screenrefresh,
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2610,
			&neogeo_ym2610_interface,
		},
	},
	neogeo_nvram_handler
};


/******************************************************************************/

#define NEO_BIOS_SOUND_256K(name,sum) \
	ROM_REGION( 0x20000, REGION_USER1 ) \
	ROM_LOAD_WIDE_SWAP( "neo-geo.rom", 0x00000, 0x020000, 0x9036d879 ) \
	ROM_REGION( 0x40000, REGION_CPU2 ) \
	ROM_LOAD( "ng-sm1.rom", 0x00000, 0x20000, 0x97cf998b )	/* we don't use the BIOS anyway... */ \
	ROM_LOAD( name,         0x00000, 0x40000, sum )	/* so overwrite it with the real thing */

#define NEO_BIOS_SOUND_128K(name,sum) \
	ROM_REGION( 0x20000, REGION_USER1 ) \
	ROM_LOAD_WIDE_SWAP( "neo-geo.rom", 0x00000, 0x020000, 0x9036d879 ) \
	ROM_REGION( 0x40000, REGION_CPU2 ) \
	ROM_LOAD( "ng-sm1.rom", 0x00000, 0x20000, 0x97cf998b )	/* we don't use the BIOS anyway... */ \
	ROM_LOAD( name,         0x00000, 0x20000, sum )	/* so overwrite it with the real thing */

#define NEO_BIOS_SOUND_64K(name,sum) \
	ROM_REGION( 0x20000, REGION_USER1 ) \
	ROM_LOAD_WIDE_SWAP( "neo-geo.rom", 0x00000, 0x020000, 0x9036d879 ) \
	ROM_REGION( 0x40000, REGION_CPU2 ) \
	ROM_LOAD( "ng-sm1.rom", 0x00000, 0x20000, 0x97cf998b )	/* we don't use the BIOS anyway... */ \
	ROM_LOAD( name,         0x00000, 0x10000, sum )	/* so overwrite it with the real thing */

#define NEO_BIOS_SOUND_512K(name,sum) \
	ROM_REGION( 0x20000, REGION_USER1 ) \
	ROM_LOAD_WIDE_SWAP( "neo-geo.rom", 0x00000, 0x020000, 0x9036d879 ) \
	ROM_REGION( 0x80000, REGION_CPU2 ) \
	ROM_LOAD( name,         0x00000, 0x80000, sum )

#define NO_DELTAT_REGION

#define NEO_SFIX_128K(name,sum) \
	ROM_REGION( 0x40000, REGION_GFX1 ) \
	ROM_LOAD( name,           0x000000, 0x20000, sum ) \
	ROM_LOAD( "ng-sfix.rom",  0x020000, 0x20000, 0x354029fc )

#define NEO_SFIX_64K(name,sum) \
	ROM_REGION( 0x40000, REGION_GFX1 ) \
	ROM_LOAD( name,           0x000000, 0x10000, sum ) \
	ROM_LOAD( "ng-sfix.rom",  0x020000, 0x20000, 0x354029fc )

#define NEO_SFIX_32K(name,sum) \
	ROM_REGION( 0x40000, REGION_GFX1 ) \
	ROM_LOAD( name,           0x000000, 0x08000, sum ) \
	ROM_LOAD( "ng-sfix.rom",  0x020000, 0x20000, 0x354029fc )

/******************************************************************************/

ROM_START( nam1975 )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "001-p1.p1", 0x000000, 0x080000, 0xcc9fc951 )
	NEO_SFIX_128K( "001-s1.s1", 0x7988ba51 )
	NEO_BIOS_SOUND_256K( "001-m1.m1", 0xba874463 )
	ROM_REGION( 0x080000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "001-v11.v11", 0x000000, 0x080000, 0xa7c3d5e5 )
	ROM_REGION( 0x180000, REGION_SOUND2 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "001-v21.v21", 0x000000, 0x080000, 0x55e670b3 )
	ROM_LOAD( "001-v22.v22", 0x080000, 0x080000, 0xab0d8368 )
	ROM_LOAD( "001-v23.v23", 0x100000, 0x080000, 0xdf468e28 )
	ROM_REGION( 0x300000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "001-c1.c1", 0x0000000, 0x80000, 0x32ea98e1 )
	ROM_LOAD_GFX_ODD( "001-c2.c2", 0x0000000, 0x80000, 0xcbc4064c )
	ROM_LOAD_GFX_EVEN( "001-c3.c3", 0x0100000, 0x80000, 0x0151054c )
	ROM_LOAD_GFX_ODD( "001-c4.c4", 0x0100000, 0x80000, 0x0a32570d )
	ROM_LOAD_GFX_EVEN( "001-c5.c5", 0x0200000, 0x80000, 0x90b74cc2 )
	ROM_LOAD_GFX_ODD( "001-c6.c6", 0x0200000, 0x80000, 0xe62bed58 )
ROM_END

ROM_START( bstars )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "002-pg.p1", 0x000000, 0x080000, 0xc100b5f5 )
	NEO_SFIX_128K( "002-s1.s1", 0x1a7fd0c6 )
	NEO_BIOS_SOUND_256K( "002-m1.m1", 0x4ecaa4ee )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "002-v11.v11", 0x000000, 0x080000, 0xb7b925bd )
	ROM_LOAD( "002-v12.v12", 0x080000, 0x080000, 0x329f26fc )
	ROM_LOAD( "002-v13.v13", 0x100000, 0x080000, 0x0c39f3c8 )
	ROM_LOAD( "002-v14.v14", 0x180000, 0x080000, 0xc7e11c38 )
	ROM_REGION( 0x080000, REGION_SOUND2 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "002-v21.v21", 0x000000, 0x080000, 0x04a733d1 )
	ROM_REGION( 0x300000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "002-c1.c1", 0x0000000, 0x080000, 0xaaff2a45 )
	ROM_LOAD_GFX_ODD( "002-c2.c2", 0x0000000, 0x080000, 0x3ba0f7e4 )
	ROM_LOAD_GFX_EVEN( "002-c3.c3", 0x0100000, 0x080000, 0x96f0fdfa )
	ROM_LOAD_GFX_ODD( "002-c4.c4", 0x0100000, 0x080000, 0x5fd87f2f )
	ROM_LOAD_GFX_EVEN( "002-c5.c5", 0x0200000, 0x080000, 0x807ed83b )
	ROM_LOAD_GFX_ODD( "002-c6.c6", 0x0200000, 0x080000, 0x5a3cad41 )
ROM_END

ROM_START( tpgolf )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "003-p1.p1", 0x000000, 0x080000, 0xf75549ba )
	ROM_LOAD_WIDE_SWAP( "003-p2.p2", 0x080000, 0x080000, 0xb7809a8f )
	NEO_SFIX_128K( "003-s1.s1", 0x7b3eb9b1 )
	NEO_BIOS_SOUND_128K( "003-m1.m1", 0x4cc545e6 )
	ROM_REGION( 0x080000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "003-v11.v11", 0x000000, 0x080000, 0xff97f1cb )
	ROM_REGION( 0x200000, REGION_SOUND2 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "003-v21.v21", 0x000000, 0x080000, 0xd34960c6 )
	ROM_LOAD( "003-v22.v22", 0x080000, 0x080000, 0x9a5f58d4 )
	ROM_LOAD( "003-v23.v23", 0x100000, 0x080000, 0x30f53e54 )
	ROM_LOAD( "003-v24.v24", 0x180000, 0x080000, 0x5ba0f501 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "003-c1.c1", 0x0000000, 0x80000, 0x0315fbaf )
	ROM_LOAD_GFX_ODD( "003-c2.c2", 0x0000000, 0x80000, 0xb4c15d59 )
	ROM_LOAD_GFX_EVEN( "003-c3.c3", 0x0100000, 0x80000, 0x8ce3e8da )
	ROM_LOAD_GFX_ODD( "003-c4.c4", 0x0100000, 0x80000, 0x29725969 )
	ROM_LOAD_GFX_EVEN( "003-c5.c5", 0x0200000, 0x80000, 0x9a7146da )
	ROM_LOAD_GFX_ODD( "003-c6.c6", 0x0200000, 0x80000, 0x1e63411a )
	ROM_LOAD_GFX_EVEN( "003-c7.c7", 0x0300000, 0x80000, 0x2886710c )
	ROM_LOAD_GFX_ODD( "003-c8.c8", 0x0300000, 0x80000, 0x422af22d )
ROM_END

ROM_START( mahretsu )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "004-p1.p1", 0x000000, 0x080000, 0xfc6f53db )
	NEO_SFIX_128K( "004-s1.s1", 0x2bd05a06 )
	NEO_BIOS_SOUND_128K( "004-m1.m1", 0xc71fbb3b )
	ROM_REGION( 0x100000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "004-v11.v11", 0x000000, 0x080000, 0xb2fb2153 )
	ROM_LOAD( "004-v12.v12", 0x080000, 0x080000, 0x8503317b )
	ROM_REGION( 0x180000, REGION_SOUND2 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "004-v21.v21", 0x000000, 0x080000, 0x4999fb27 )
	ROM_LOAD( "004-v22.v22", 0x080000, 0x080000, 0x776fa2a2 )
	ROM_LOAD( "004-v23.v23", 0x100000, 0x080000, 0xb3e7eeea )
	ROM_REGION( 0x200000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "004-c1.c1", 0x0000000, 0x80000, 0xf1ae16bc )
	ROM_LOAD_GFX_ODD( "004-c2.c2", 0x0000000, 0x80000, 0xbdc13520 )
	ROM_LOAD_GFX_EVEN( "004-c3.c3", 0x0100000, 0x80000, 0x9c571a37 )
	ROM_LOAD_GFX_ODD( "004-c4.c4", 0x0100000, 0x80000, 0x7e81cb29 )
ROM_END

ROM_START( maglord )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "005-pg1.p1", 0x000000, 0x080000, 0xbd0a492d )
	NEO_SFIX_128K( "005-s1.s1", 0x1c5369a2 )
	NEO_BIOS_SOUND_256K( "005-m1.m1", 0x26259f0f )
	ROM_REGION( 0x080000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "005-v11.v11", 0x000000, 0x080000, 0xcc0455fd )
	ROM_REGION( 0x100000, REGION_SOUND2 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "005-v21.v21", 0x000000, 0x080000, 0xf94ab5b7 )
	ROM_LOAD( "005-v22.v22", 0x080000, 0x080000, 0x232cfd04 )
	ROM_REGION( 0x300000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "005-c1.c1", 0x0000000, 0x80000, 0x806aee34 )
	ROM_LOAD_GFX_ODD( "005-c2.c2", 0x0000000, 0x80000, 0x34aa9a86 )
	ROM_LOAD_GFX_EVEN( "005-c3.c3", 0x0100000, 0x80000, 0xc4c2b926 )
	ROM_LOAD_GFX_ODD( "005-c4.c4", 0x0100000, 0x80000, 0x9c46dcf4 )
	ROM_LOAD_GFX_EVEN( "005-c5.c5", 0x0200000, 0x80000, 0x69086dec )
	ROM_LOAD_GFX_ODD( "005-c6.c6", 0x0200000, 0x80000, 0xab7ac142 )
ROM_END

ROM_START( maglordh )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "005-p1.p1", 0x000000, 0x080000, 0x599043c5 )
	NEO_SFIX_128K( "005-s1.s1", 0x1c5369a2 )
	NEO_BIOS_SOUND_256K( "005-m1.m1", 0x26259f0f )
	ROM_REGION( 0x080000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "005-v11.v11", 0x000000, 0x080000, 0xcc0455fd )
	ROM_REGION( 0x100000, REGION_SOUND2 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "005-v21.v21", 0x000000, 0x080000, 0xf94ab5b7 )
	ROM_LOAD( "005-v22.v22", 0x080000, 0x080000, 0x232cfd04 )
	ROM_REGION( 0x300000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "005-c1.c1", 0x0000000, 0x80000, 0x806aee34 )
	ROM_LOAD_GFX_ODD( "005-c2.c2", 0x0000000, 0x80000, 0x34aa9a86 )
	ROM_LOAD_GFX_EVEN( "005-c3.c3", 0x0100000, 0x80000, 0xc4c2b926 )
	ROM_LOAD_GFX_ODD( "005-c4.c4", 0x0100000, 0x80000, 0x9c46dcf4 )
	ROM_LOAD_GFX_EVEN( "005-c5.c5", 0x0200000, 0x80000, 0x69086dec )
	ROM_LOAD_GFX_ODD( "005-c6.c6", 0x0200000, 0x80000, 0xab7ac142 )
ROM_END

ROM_START( ridhero )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "006-p1.p1", 0x000000, 0x080000, 0xd4aaf597 )
	NEO_SFIX_128K( "006-s1.s1", 0xeb5189f0 )
	NEO_BIOS_SOUND_256K( "006-m1.m1", 0x92e7b4fe )
	ROM_REGION( 0x100000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "006-v11.v11", 0x000000, 0x080000, 0xcdf74a42 )
	ROM_LOAD( "006-v12.v12", 0x080000, 0x080000, 0xe2fd2371 )
	ROM_REGION( 0x200000, REGION_SOUND2 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "006-v21.v21", 0x000000, 0x080000, 0x94092bce )
	ROM_LOAD( "006-v22.v22", 0x080000, 0x080000, 0x4e2cd7c3 )
	ROM_LOAD( "006-v23.v23", 0x100000, 0x080000, 0x069c71ed )
	ROM_LOAD( "006-v24.v24", 0x180000, 0x080000, 0x89fbb825 )
	ROM_REGION( 0x200000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "006-c1.c1", 0x0000000, 0x080000, 0x4a5c7f78 )
	ROM_LOAD_GFX_ODD( "006-c2.c2", 0x0000000, 0x080000, 0xe0b70ece )
	ROM_LOAD_GFX_EVEN( "006-c3.c3", 0x0100000, 0x080000, 0x8acff765 )
	ROM_LOAD_GFX_ODD( "006-c4.c4", 0x0100000, 0x080000, 0x205e3208 )
ROM_END

ROM_START( alpham2 )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "007-p1.p1", 0x000000, 0x080000, 0x5b266f47 )
	ROM_LOAD_WIDE_SWAP( "007-p2.p2", 0x080000, 0x020000, 0xeb9c1044 )
	NEO_SFIX_128K( "007-s1.s1", 0x85ec9acf )
	NEO_BIOS_SOUND_128K( "007-m1.m1", 0x28dfe2cd )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "007-v1.v1", 0x000000, 0x100000, 0xcd5db931 )
	ROM_LOAD( "007-v2.v2", 0x100000, 0x100000, 0x63e9b574 )
	ROM_REGION( 0x300000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "007-c1.c1", 0x0000000, 0x100000, 0x8fba8ff3 )
	ROM_LOAD_GFX_ODD( "007-c2.c2", 0x0000000, 0x100000, 0x4dad2945 )
	ROM_LOAD_GFX_EVEN( "007-c3.c3", 0x0200000, 0x080000, 0x68c2994e )
	ROM_LOAD_GFX_ODD( "007-c4.c4", 0x0200000, 0x080000, 0x7d588349 )
ROM_END

ROM_START( ncombat )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "009-p1.p1", 0x000000, 0x080000, 0xb45fcfbf )
	NEO_SFIX_128K( "009-s1.s1", 0xd49afee8 )
	NEO_BIOS_SOUND_128K( "009-m1.m1", 0xb5819863 )
	ROM_REGION( 0x180000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "009-v11.v11", 0x000000, 0x080000, 0xcf32a59c )
	ROM_LOAD( "009-v12.v12", 0x080000, 0x080000, 0x7b3588b7 )
	ROM_LOAD( "009-v13.v13", 0x100000, 0x080000, 0x505a01b5 )
	ROM_REGION( 0x080000, REGION_SOUND2 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "009-v21.v21", 0x000000, 0x080000, 0x365f9011 )
	ROM_REGION( 0x300000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "009-c1.c1", 0x0000000, 0x80000, 0x33cc838e )
	ROM_LOAD_GFX_ODD( "009-c2.c2", 0x0000000, 0x80000, 0x26877feb )
	ROM_LOAD_GFX_EVEN( "009-c3.c3", 0x0100000, 0x80000, 0x3b60a05d )
	ROM_LOAD_GFX_ODD( "009-c4.c4", 0x0100000, 0x80000, 0x39c2d039 )
	ROM_LOAD_GFX_EVEN( "009-c5.c5", 0x0200000, 0x80000, 0x67a4344e )
	ROM_LOAD_GFX_ODD( "009-c6.c6", 0x0200000, 0x80000, 0x2eca8b19 )
ROM_END

ROM_START( cyberlip )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "010-p1.p1", 0x000000, 0x080000, 0x69a6b42d )
	NEO_SFIX_128K( "010-s1.s1", 0x79a35264 )
	NEO_BIOS_SOUND_128K( "010-m1.m1", 0x8be3a078 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "010-v11.v11", 0x000000, 0x080000, 0x90224d22 )
	ROM_LOAD( "010-v12.v12", 0x080000, 0x080000, 0xa0cf1834 )
	ROM_LOAD( "010-v13.v13", 0x100000, 0x080000, 0xae38bc84 )
	ROM_LOAD( "010-v14.v14", 0x180000, 0x080000, 0x70899bd2 )
	ROM_REGION( 0x080000, REGION_SOUND2 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "010-v21.v21", 0x000000, 0x080000, 0x586f4cb2 )
	ROM_REGION( 0x300000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "010-c1.c1", 0x0000000, 0x80000, 0x8bba5113 )
	ROM_LOAD_GFX_ODD( "010-c2.c2", 0x0000000, 0x80000, 0xcbf66432 )
	ROM_LOAD_GFX_EVEN( "010-c3.c3", 0x0100000, 0x80000, 0xe4f86efc )
	ROM_LOAD_GFX_ODD( "010-c4.c4", 0x0100000, 0x80000, 0xf7be4674 )
	ROM_LOAD_GFX_EVEN( "010-c5.c5", 0x0200000, 0x80000, 0xe8076da0 )
	ROM_LOAD_GFX_ODD( "010-c6.c6", 0x0200000, 0x80000, 0xc495c567 )
ROM_END

ROM_START( superspy )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "011-p1.p1", 0x000000, 0x080000, 0xc7f944b5 )
	ROM_LOAD_WIDE_SWAP( "sp2.p2", 0x080000, 0x020000, 0x811a4faf )
	NEO_SFIX_128K( "011-s1.s1", 0xec5fdb96 )
	NEO_BIOS_SOUND_256K( "011-m1.m1", 0xca661f1b )
	ROM_REGION( 0x180000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "011-v11.v11", 0x000000, 0x100000, 0x5c674d5c )
	ROM_LOAD( "011-v12.v12", 0x100000, 0x080000, 0x9f513d5a )
	ROM_REGION( 0x080000, REGION_SOUND2 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "011-v21.v21", 0x000000, 0x080000, 0x426cd040 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "011-c1.c1", 0x0000000, 0x100000, 0xcae7be57 )
	ROM_LOAD_GFX_ODD( "011-c2.c2", 0x0000000, 0x100000, 0x9e29d986 )
	ROM_LOAD_GFX_EVEN( "011-c3.c3", 0x0200000, 0x100000, 0x14832ff2 )
	ROM_LOAD_GFX_ODD( "011-c4.c4", 0x0200000, 0x100000, 0xb7f63162 )
ROM_END

ROM_START( mutnat )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "014-p1.p1", 0x000000, 0x080000, 0x6f1699c8 )
	NEO_SFIX_128K( "014-s1.s1", 0x99419733 )
	NEO_BIOS_SOUND_128K( "014-m1.m1", 0xb6683092 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "014-v1.v1", 0x000000, 0x100000, 0x25419296 )
	ROM_LOAD( "014-v2.v2", 0x100000, 0x100000, 0x0de53d5e )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "014-c1.c1", 0x0000000, 0x100000, 0x5e4381bf )
	ROM_LOAD_GFX_ODD( "014-c2.c2", 0x0000000, 0x100000, 0x69ba4e18 )
	ROM_LOAD_GFX_EVEN( "014-c3.c3", 0x0200000, 0x100000, 0x890327d5 )
	ROM_LOAD_GFX_ODD( "014-c4.c4", 0x0200000, 0x100000, 0xe4002651 )
ROM_END

ROM_START( kotm )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "016-p1.p1", 0x000000, 0x080000, 0x1b818731 )
	ROM_LOAD_WIDE_SWAP( "016-p2.p2", 0x080000, 0x020000, 0x12afdc2b )
	NEO_SFIX_128K( "016-s1.s1", 0x1a2eeeb3 )
	NEO_BIOS_SOUND_128K( "016-m1.m1", 0x9da9ca10 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "016-v1.v1", 0x000000, 0x100000, 0x86c0a502 )
	ROM_LOAD( "016-v2.v2", 0x100000, 0x100000, 0x5bc23ec5 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "016-c1.c1", 0x0000000, 0x100000, 0x71471c25 )
	ROM_LOAD_GFX_ODD( "016-c2.c2", 0x0000000, 0x100000, 0x320db048 )
	ROM_LOAD_GFX_EVEN( "016-c3.c3", 0x0200000, 0x100000, 0x98de7995 )
	ROM_LOAD_GFX_ODD( "016-c4.c4", 0x0200000, 0x100000, 0x070506e2 )
ROM_END

ROM_START( sengoku )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "017-p1.p1", 0x000000, 0x080000, 0xf8a63983 )
	ROM_LOAD_WIDE_SWAP( "017-p2.p2", 0x080000, 0x020000, 0x3024bbb3 )
	NEO_SFIX_128K( "017-s1.s1", 0xb246204d )
	NEO_BIOS_SOUND_128K( "017-m1.m1", 0x9b4f34c6 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "017-v1.v1", 0x000000, 0x100000, 0x23663295 )
	ROM_LOAD( "017-v2.v2", 0x100000, 0x100000, 0xf61e6765 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "017-c1.c1", 0x0000000, 0x100000, 0xb4eb82a1 )
	ROM_LOAD_GFX_ODD( "017-c2.c2", 0x0000000, 0x100000, 0xd55c550d )
	ROM_LOAD_GFX_EVEN( "017-c3.c3", 0x0200000, 0x100000, 0xed51ef65 )
	ROM_LOAD_GFX_ODD( "017-c4.c4", 0x0200000, 0x100000, 0xf4f3c9cb )
ROM_END

ROM_START( sengokuh )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "017-hp1.p1", 0x000000, 0x080000, 0x33eccae0 )
	ROM_LOAD_WIDE_SWAP( "017-p2.p2", 0x080000, 0x020000, 0x3024bbb3 )
	NEO_SFIX_128K( "017-s1.s1", 0xb246204d )
	NEO_BIOS_SOUND_128K( "017-m1.m1", 0x9b4f34c6 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "017-v1.v1", 0x000000, 0x100000, 0x23663295 )
	ROM_LOAD( "017-v2.v2", 0x100000, 0x100000, 0xf61e6765 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "017-c1.c1", 0x0000000, 0x100000, 0xb4eb82a1 )
	ROM_LOAD_GFX_ODD( "017-c2.c2", 0x0000000, 0x100000, 0xd55c550d )
	ROM_LOAD_GFX_EVEN( "017-c3.c3", 0x0200000, 0x100000, 0xed51ef65 )
	ROM_LOAD_GFX_ODD( "017-c4.c4", 0x0200000, 0x100000, 0xf4f3c9cb )
ROM_END

ROM_START( burningf )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "018-p1.p1", 0x000000, 0x080000, 0x4092c8db )
	NEO_SFIX_128K( "018-s1.s1", 0x6799ea0d )
	NEO_BIOS_SOUND_128K( "018-m1.m1", 0x0c939ee2 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "018-v1.v1", 0x000000, 0x100000, 0x508c9ffc )
	ROM_LOAD( "018-v2.v2", 0x100000, 0x100000, 0x854ef277 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "018-c1.c1", 0x0000000, 0x100000, 0x25a25e9b )
	ROM_LOAD_GFX_ODD( "018-c2.c2", 0x0000000, 0x100000, 0xd4378876 )
	ROM_LOAD_GFX_EVEN( "018-c3.c3", 0x0200000, 0x100000, 0x862b60da )
	ROM_LOAD_GFX_ODD( "018-c4.c4", 0x0200000, 0x100000, 0xe2e0aff7 )
ROM_END

ROM_START( burningfh )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "018-hp1.p1", 0x000000, 0x080000, 0xddffcbf4 )
	NEO_SFIX_128K( "018-s1.s1", 0x6799ea0d )
	NEO_BIOS_SOUND_128K( "018-m1.m1", 0x0c939ee2 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "018-v1.v1", 0x000000, 0x100000, 0x508c9ffc )
	ROM_LOAD( "018-v2.v2", 0x100000, 0x100000, 0x854ef277 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "018-c1.c1", 0x0000000, 0x100000, 0x25a25e9b )
	ROM_LOAD_GFX_ODD( "018-c2.c2", 0x0000000, 0x100000, 0xd4378876 )
	ROM_LOAD_GFX_EVEN( "018-c3.c3", 0x0200000, 0x100000, 0x862b60da )
	ROM_LOAD_GFX_ODD( "018-c4.c4", 0x0200000, 0x100000, 0xe2e0aff7 )
ROM_END

ROM_START( lbowling )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "019-p1.p1", 0x000000, 0x080000, 0xa2de8445 )
	NEO_SFIX_128K( "019-s1.s1", 0x5fcdc0ed )
	NEO_BIOS_SOUND_128K( "019-m1.m1", 0xd568c17d )
	ROM_REGION( 0x100000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "019-v11.v11", 0x000000, 0x080000, 0x0fb74872 )
	ROM_LOAD( "019-v12.v12", 0x080000, 0x080000, 0x029faa57 )
	ROM_REGION( 0x080000, REGION_SOUND2 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "019-v21.v21", 0x000000, 0x080000, 0x2efd5ada )
	ROM_REGION( 0x100000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "019-c1.c1", 0x0000000, 0x080000, 0x4ccdef18 )
	ROM_LOAD_GFX_ODD( "019-c2.c2", 0x0000000, 0x080000, 0xd4dd0802 )
ROM_END

ROM_START( gpilots )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "020-p1.p1", 0x000000, 0x080000, 0xe6f2fe64 )
	ROM_LOAD_WIDE_SWAP( "020-p2.p2", 0x080000, 0x020000, 0xedcb22ac )
	NEO_SFIX_128K( "020-s1.s1", 0xa6d83d53 )
	NEO_BIOS_SOUND_128K( "020-m1.m1", 0x48409377 )
	ROM_REGION( 0x180000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "020-v11.v11", 0x000000, 0x100000, 0x1b526c8b )
	ROM_LOAD( "020-v12.v12", 0x100000, 0x080000, 0x4a9e6f03 )
	ROM_REGION( 0x080000, REGION_SOUND2 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "020-v21.v21", 0x000000, 0x080000, 0x7abf113d )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "020-c1.c1", 0x0000000, 0x100000, 0xbd6fe78e )
	ROM_LOAD_GFX_ODD( "020-c2.c2", 0x0000000, 0x100000, 0x5f4a925c )
	ROM_LOAD_GFX_EVEN( "020-c3.c3", 0x0200000, 0x100000, 0xd1e42fd0 )
	ROM_LOAD_GFX_ODD( "020-c4.c4", 0x0200000, 0x100000, 0xedde439b )
ROM_END

ROM_START( joyjoy )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "021-p1.p1", 0x000000, 0x080000, 0x39c3478f )
	NEO_SFIX_128K( "021-s1.s1", 0x6956d778 )
	NEO_BIOS_SOUND_256K( "021-m1.m1", 0x5a4be5e8 )
	ROM_REGION( 0x080000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "021-v11.v11", 0x000000, 0x080000, 0x66c1e5c4 )
	ROM_REGION( 0x080000, REGION_SOUND2 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "021-v21.v21", 0x000000, 0x080000, 0x8ed20a86 )
	ROM_REGION( 0x100000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "021-c1.c1", 0x0000000, 0x080000, 0x509250ec )
	ROM_LOAD_GFX_ODD( "021-c2.c2", 0x0000000, 0x080000, 0x09ed5258 )
ROM_END

ROM_START( bjourney )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "022-p1.p1", 0x000000, 0x100000, 0x6a2f6d4a )
	NEO_SFIX_128K( "022-s1.s1", 0x843c3624 )
	NEO_BIOS_SOUND_128K( "022-m1.m1", 0x8e1d4ab6 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "022-v11.v11", 0x000000, 0x100000, 0x2cb4ad91 )
	ROM_LOAD( "022-v22.v22", 0x100000, 0x100000, 0x65a54d13 )
	ROM_REGION( 0x300000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "022-c1.c1", 0x0000000, 0x100000, 0x4d47a48c )
	ROM_LOAD_GFX_ODD( "022-c2.c2", 0x0000000, 0x100000, 0xe8c1491a )
	ROM_LOAD_GFX_EVEN( "022-c3.c3", 0x0200000, 0x080000, 0x66e69753 )
	ROM_LOAD_GFX_ODD( "022-c4.c4", 0x0200000, 0x080000, 0x71bfd48a )
ROM_END

ROM_START( quizdais )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "023-p1.p1", 0x000000, 0x100000, 0xc488fda3 )
	NEO_SFIX_128K( "023-s1.s1", 0xac31818a )
	NEO_BIOS_SOUND_128K( "023-m1.m1", 0x2a2105e0 )
	ROM_REGION( 0x100000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "023-v1.v1", 0x000000, 0x100000, 0xa53e5bd3 )
	ROM_REGION( 0x200000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "023-c1.c1", 0x0000000, 0x100000, 0x2999535a )
	ROM_LOAD_GFX_ODD( "023-c2.c2", 0x0000000, 0x100000, 0x876a99e6 )
ROM_END

ROM_START( lresort )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "024-p1.p1", 0x000000, 0x080000, 0x89c4ab97 )
	NEO_SFIX_128K( "024-s1.s1", 0x5cef5cc6 )
	NEO_BIOS_SOUND_128K( "024-m1.m1", 0xcec19742 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "024-v1.v1", 0x000000, 0x100000, 0xefdfa063 )
	ROM_LOAD( "024-v2.v2", 0x100000, 0x100000, 0x3c7997c0 )
	ROM_REGION( 0x300000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "024-c1.c1", 0x0000000, 0x100000, 0x3617c2dc )
	ROM_LOAD_GFX_ODD( "024-c2.c2", 0x0000000, 0x100000, 0x3f0a7fd8 )
	ROM_LOAD_GFX_EVEN( "024-c3.c3", 0x0200000, 0x080000, 0xe9f745f8 )
	ROM_LOAD_GFX_ODD( "024-c4.c4", 0x0200000, 0x080000, 0x7382fefb )
ROM_END

ROM_START( eightman )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "025-p1.p1", 0x000000, 0x080000, 0x43344cb0 )
	NEO_SFIX_128K( "025-s1.s1", 0xa402202b )
	NEO_BIOS_SOUND_128K( "025-m1.m1", 0x9927034c )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "025-v1.v1", 0x000000, 0x100000, 0x4558558a )
	ROM_LOAD( "025-v2.v2", 0x100000, 0x100000, 0xc5e052e9 )
	ROM_REGION( 0x300000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "025-c1.c1", 0x0000000, 0x100000, 0x555e16a4 )
	ROM_LOAD_GFX_ODD( "025-c2.c2", 0x0000000, 0x100000, 0xe1ee51c3 )
	ROM_LOAD_GFX_EVEN( "025-c3.c3", 0x0200000, 0x080000, 0x0923d5b0 )
	ROM_LOAD_GFX_ODD( "025-c4.c4", 0x0200000, 0x080000, 0xe3eca67b )
ROM_END

ROM_START( minasan )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "027-p1.p1", 0x000000, 0x080000, 0xc8381327 )
	NEO_SFIX_128K( "027-s1.s1", 0xe5824baa )
	NEO_BIOS_SOUND_128K( "027-m1.m1", 0xadd5a226 )
	ROM_REGION( 0x100000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "027-v11.v11", 0x000000, 0x100000, 0x59ad4459 )
	ROM_REGION( 0x100000, REGION_SOUND2 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "027-v21.v21", 0x000000, 0x100000, 0xdf5b4eeb )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "027-c1.c1", 0x0000000, 0x100000, 0xd0086f94 )
	ROM_LOAD_GFX_ODD( "027-c2.c2", 0x0000000, 0x100000, 0xda61f5a6 )
	ROM_LOAD_GFX_EVEN( "027-c3.c3", 0x0200000, 0x100000, 0x08df1228 )
	ROM_LOAD_GFX_ODD( "027-c4.c4", 0x0200000, 0x100000, 0x54e87696 )
ROM_END

ROM_START( legendos )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "029-p1.p1", 0x000000, 0x080000, 0x9d563f19 )
	NEO_SFIX_128K( "029-s1.s1", 0xbcd502f0 )
	NEO_BIOS_SOUND_128K( "029-m1.m1", 0x6f2843f0 )
	ROM_REGION( 0x100000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "029-v1.v1", 0x000000, 0x100000, 0x85065452 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "029-c1.c1", 0x0000000, 0x100000, 0x2f5ab875 )
	ROM_LOAD_GFX_ODD( "029-c2.c2", 0x0000000, 0x100000, 0x318b2711 )
	ROM_LOAD_GFX_EVEN( "029-c3.c3", 0x0200000, 0x100000, 0x6bc52cb2 )
	ROM_LOAD_GFX_ODD( "029-c4.c4", 0x0200000, 0x100000, 0x37ef298c )
ROM_END

ROM_START( 2020bb )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "030-p1.p1", 0x000000, 0x080000, 0xd396c9cb )
	NEO_SFIX_128K( "030-s1.s1", 0x7015b8fc )
	NEO_BIOS_SOUND_128K( "030-m1.m1", 0x4cf466ec )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "030-v1.v1", 0x000000, 0x100000, 0xd4ca364e )
	ROM_LOAD( "030-v2.v2", 0x100000, 0x100000, 0x54994455 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "030-c1.c1", 0x0000000, 0x100000, 0x4f5e19bd )
	ROM_LOAD_GFX_ODD( "030-c2.c2", 0x0000000, 0x100000, 0xd6314bf0 )
	ROM_LOAD_GFX_EVEN( "030-c3.c3", 0x0200000, 0x100000, 0x47fddfee )
	ROM_LOAD_GFX_ODD( "030-c4.c4", 0x0200000, 0x100000, 0x780d1c4e )
ROM_END

ROM_START( 2020bbh )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "030-pg1.p1", 0x000000, 0x080000, 0x12d048d7 )
	NEO_SFIX_128K( "030-s1.s1", 0x7015b8fc )
	NEO_BIOS_SOUND_128K( "030-m1.m1", 0x4cf466ec )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "030-v1.v1", 0x000000, 0x100000, 0xd4ca364e )
	ROM_LOAD( "030-v2.v2", 0x100000, 0x100000, 0x54994455 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "030-c1.c1", 0x0000000, 0x100000, 0x4f5e19bd )
	ROM_LOAD_GFX_ODD( "030-c2.c2", 0x0000000, 0x100000, 0xd6314bf0 )
	ROM_LOAD_GFX_EVEN( "030-c3.c3", 0x0200000, 0x100000, 0x47fddfee )
	ROM_LOAD_GFX_ODD( "030-c4.c4", 0x0200000, 0x100000, 0x780d1c4e )
ROM_END

ROM_START( socbrawl )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "031-pg1.p1", 0x000000, 0x080000, 0x17f034a7 )
	NEO_SFIX_128K( "031-s1.s1", 0x4c117174 )
	NEO_BIOS_SOUND_128K( "031-m1.m1", 0xcb37427c )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "031-v1.v1", 0x000000, 0x100000, 0xcc78497e )
	ROM_LOAD( "031-v2.v2", 0x100000, 0x100000, 0xdda043c6 )
	ROM_REGION( 0x300000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "031-c1.c1", 0x0000000, 0x100000, 0xbd0a4eb8 )
	ROM_LOAD_GFX_ODD( "031-c2.c2", 0x0000000, 0x100000, 0xefde5382 )
	ROM_LOAD_GFX_EVEN( "031-c3.c3", 0x0200000, 0x080000, 0x580f7f33 )
	ROM_LOAD_GFX_ODD( "031-c4.c4", 0x0200000, 0x080000, 0xed297de8 )
ROM_END

ROM_START( roboarmy )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "032-p1.p1", 0x000000, 0x080000, 0xcd11cbd4 )
	NEO_SFIX_128K( "032-s1.s1", 0xac0daa1b )
	NEO_BIOS_SOUND_128K( "032-m1.m1", 0x35ec952d )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "032-v1.v1", 0x000000, 0x100000, 0x63791533 )
	ROM_LOAD( "032-v2.v2", 0x100000, 0x100000, 0xeb95de70 )
	ROM_REGION( 0x300000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "032-c1.c1", 0x0000000, 0x100000, 0x97984c6c )
	ROM_LOAD_GFX_ODD( "032-c2.c2", 0x0000000, 0x100000, 0x65773122 )
	ROM_LOAD_GFX_EVEN( "032-c3.c3", 0x0200000, 0x080000, 0x40adfccd )
	ROM_LOAD_GFX_ODD( "032-c4.c4", 0x0200000, 0x080000, 0x462571de )
ROM_END

ROM_START( fatfury1 )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "033-p1.p1", 0x000000, 0x080000, 0x47ebdc2f )
	ROM_LOAD_WIDE_SWAP( "033-p2.p2", 0x080000, 0x020000, 0xc473af1c )
	NEO_SFIX_128K( "033-s1.s1", 0x3c3bdf8c )
	NEO_BIOS_SOUND_128K( "033-m1.m1", 0x5be10ffd )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "033-v1.v1", 0x000000, 0x100000, 0x212fd20d )
	ROM_LOAD( "033-v2.v2", 0x100000, 0x100000, 0xfa2ae47f )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "033-c1.c1", 0x0000000, 0x100000, 0x74317e54 )
	ROM_LOAD_GFX_ODD( "033-c2.c2", 0x0000000, 0x100000, 0x5bb952f3 )
	ROM_LOAD_GFX_EVEN( "033-c3.c3", 0x0200000, 0x100000, 0x9b714a7c )
	ROM_LOAD_GFX_ODD( "033-c4.c4", 0x0200000, 0x100000, 0x9397476a )
ROM_END

ROM_START( fbfrenzy )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "034-p1.p1", 0x000000, 0x080000, 0xcdef6b19 )
	NEO_SFIX_128K( "034-s1.s1", 0x8472ed44 )
	NEO_BIOS_SOUND_128K( "034-m1.m1", 0xf41b16b8 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "034-v1.v1", 0x000000, 0x100000, 0x50c9d0dd )
	ROM_LOAD( "034-v2.v2", 0x100000, 0x100000, 0x5aa15686 )
	ROM_REGION( 0x300000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "034-c1.c1", 0x0000000, 0x100000, 0x91c56e78 )
	ROM_LOAD_GFX_ODD( "034-c2.c2", 0x0000000, 0x100000, 0x9743ea2f )
	ROM_LOAD_GFX_EVEN( "034-c3.c3", 0x0200000, 0x080000, 0xe5aa65f5 )
	ROM_LOAD_GFX_ODD( "034-c4.c4", 0x0200000, 0x080000, 0x0eb138cc )
ROM_END

ROM_START( bakatono )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "036-p1.p1", 0x000000, 0x080000, 0x1c66b6fa )
	NEO_SFIX_128K( "036-s1.s1", 0xf3ef4485 )
	NEO_BIOS_SOUND_128K( "036-m1.m1", 0xf1385b96 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "036-v1.v1", 0x000000, 0x100000, 0x1c335dce )
	ROM_LOAD( "036-v2.v2", 0x100000, 0x100000, 0xbbf79342 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "036-c1.c1", 0x0000000, 0x100000, 0xfe7f1010 )
	ROM_LOAD_GFX_ODD( "036-c2.c2", 0x0000000, 0x100000, 0xbbf003f5 )
	ROM_LOAD_GFX_EVEN( "036-c3.c3", 0x0200000, 0x100000, 0x9ac0708e )
	ROM_LOAD_GFX_ODD( "036-c4.c4", 0x0200000, 0x100000, 0xf2577d22 )
ROM_END

ROM_START( crsword )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "037-p1.p1", 0x000000, 0x080000, 0xe7f2553c )
	NEO_SFIX_128K( "037-s1.s1", 0x74651f27 )
	NEO_BIOS_SOUND_128K( "037-m1.m1", 0x9504b2c6 )
	ROM_REGION( 0x100000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "037-v1.v1", 0x000000, 0x100000, 0x61fedf65 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "037-c1.c1", 0x0000000, 0x100000, 0x09df6892 )
	ROM_LOAD_GFX_ODD( "037-c2.c2", 0x0000000, 0x100000, 0xac122a78 )
	ROM_LOAD_GFX_EVEN( "037-c3.c3", 0x0200000, 0x100000, 0x9d7ed1ca )
	ROM_LOAD_GFX_ODD( "037-c4.c4", 0x0200000, 0x100000, 0x4a24395d )
ROM_END

ROM_START( trally )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "038-p1.p1", 0x000000, 0x080000, 0x1e52a576 )
	ROM_LOAD_WIDE_SWAP( "038-p2.p2", 0x080000, 0x080000, 0xa5193e2f )
	NEO_SFIX_128K( "038-s1.s1", 0xfff62ae3 )
	NEO_BIOS_SOUND_128K( "038-m1.m1", 0x0908707e )
	ROM_REGION( 0x180000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "038-v1.v1", 0x000000, 0x100000, 0x5ccd9fd5 )
	ROM_LOAD( "038-v2.v2", 0x100000, 0x080000, 0xddd8d1e6 )
	ROM_REGION( 0x300000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "038-c1.c1", 0x0000000, 0x100000, 0xc58323d4 )
	ROM_LOAD_GFX_ODD( "038-c2.c2", 0x0000000, 0x100000, 0xbba9c29e )
	ROM_LOAD_GFX_EVEN( "038-c3.c3", 0x0200000, 0x080000, 0x3bb7b9d6 )
	ROM_LOAD_GFX_ODD( "038-c4.c4", 0x0200000, 0x080000, 0xa4513ecf )
ROM_END

ROM_START( kotm2 )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "039-p1.p1", 0x000000, 0x080000, 0xb372d54c )
	ROM_LOAD_WIDE_SWAP( "039-p2.p2", 0x080000, 0x080000, 0x28661afe )
	NEO_SFIX_128K( "039-s1.s1", 0x63ee053a )
	NEO_BIOS_SOUND_128K( "039-m1.m1", 0x0c5b2ad5 )
	ROM_REGION( 0x300000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "039-v2.v2", 0x000000, 0x200000, 0x86d34b25 )
	ROM_LOAD( "039-v4.v4", 0x200000, 0x100000, 0x8fa62a0b )
	ROM_REGION( 0x600000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "039-c1.c1", 0x0000000, 0x100000, 0x6d1c4aa9 )
	ROM_CONTINUE( 0x0400001, 0x100000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_GFX_ODD( "039-c2.c2", 0x0000000, 0x100000, 0xf7b75337 )
	ROM_CONTINUE( 0x0400000, 0x100000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_GFX_EVEN( "039-c3.c3", 0x0200000, 0x080000, 0xbfc4f0b2 )
	ROM_LOAD_GFX_ODD( "039-c4.c4", 0x0200000, 0x080000, 0x81c9c250 )
ROM_END

ROM_START( sengoku2 )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "040-p1.p1", 0x000000, 0x100000, 0x6dde02c2 )
	NEO_SFIX_128K( "040-s1.s1", 0xcd9802a3 )
	NEO_BIOS_SOUND_128K( "040-m1.m1", 0xd4de4bca )
	ROM_REGION( 0x300000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "040-v1.v1", 0x000000, 0x200000, 0x71cb4b5d )
	ROM_LOAD( "040-v2.v2", 0x200000, 0x100000, 0xc5cece01 )
	ROM_REGION( 0x600000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "040-c1.c1", 0x0000000, 0x100000, 0xfaa8ea99 )
	ROM_CONTINUE( 0x0400001, 0x100000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_GFX_ODD( "040-c2.c2", 0x0000000, 0x100000, 0x87d0ec65 )
	ROM_CONTINUE( 0x0400000, 0x100000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_GFX_EVEN( "040-c3.c3", 0x0200000, 0x080000, 0x24b5ba80 )
	ROM_LOAD_GFX_ODD( "040-c4.c4", 0x0200000, 0x080000, 0x1c9e9930 )
ROM_END

ROM_START( bstars2 )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "041-p1.p1", 0x000000, 0x080000, 0x523567fd )
	NEO_SFIX_128K( "041-s1.s1", 0x015c5c94 )
	NEO_BIOS_SOUND_128K( "041-m1.m1", 0x15c177a6 )
	ROM_REGION( 0x280000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "041-v1.v1", 0x000000, 0x100000, 0xcb1da093 )
	ROM_LOAD( "041-v2.v2", 0x100000, 0x100000, 0x1c954a9d )
	ROM_LOAD( "041-v3.v3", 0x200000, 0x080000, 0xafaa0180 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "041-c1.c1", 0x0000000, 0x100000, 0xb39a12e1 )
	ROM_LOAD_GFX_ODD( "041-c2.c2", 0x0000000, 0x100000, 0x766cfc2f )
	ROM_LOAD_GFX_EVEN( "041-c3.c3", 0x0200000, 0x100000, 0xfb31339d )
	ROM_LOAD_GFX_ODD( "041-c4.c4", 0x0200000, 0x100000, 0x70457a0c )
ROM_END

ROM_START( quizdai2 )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "042-p1.p1", 0x000000, 0x100000, 0xed719dcf )
	NEO_SFIX_128K( "042-s1.s1", 0x164fd6e6 )
	NEO_BIOS_SOUND_128K( "042-m1.m1", 0xbb19995d )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "042-v1.v1", 0x000000, 0x100000, 0xaf7f8247 )
	ROM_LOAD( "042-v2.v2", 0x100000, 0x100000, 0xc6474b59 )
	ROM_REGION( 0x300000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "042-c1.c1", 0x0000000, 0x100000, 0xcb5809a1 )
	ROM_LOAD_GFX_ODD( "042-c2.c2", 0x0000000, 0x100000, 0x1436dfeb )
	ROM_LOAD_GFX_EVEN( "042-c3.c3", 0x0200000, 0x080000, 0xbcd4a518 )
	ROM_LOAD_GFX_ODD( "042-c4.c4", 0x0200000, 0x080000, 0xd602219b )
ROM_END

ROM_START( 3countb )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "043-p1.p1", 0x000000, 0x100000, 0xffbdd928 )
	NEO_SFIX_128K( "043-s1.s1", 0xc362d484 )
	NEO_BIOS_SOUND_128K( "043-m1.m1", 0x7eab59cb )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "043-v1.v1", 0x000000, 0x200000, 0x63688ce8 )
	ROM_LOAD( "043-v2.v2", 0x200000, 0x200000, 0xc69a827b )
	ROM_REGION( 0x0800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "043-c1.c1", 0x0000000, 0x100000, 0xbad2d67f )
	ROM_CONTINUE( 0x0400001, 0x100000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_GFX_ODD( "043-c2.c2", 0x0000000, 0x100000, 0xa7fbda95 )
	ROM_CONTINUE( 0x0400000, 0x100000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_GFX_EVEN( "043-c3.c3", 0x0200000, 0x100000, 0xf00be011 )
	ROM_CONTINUE( 0x0600001, 0x100000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_GFX_ODD( "043-c4.c4", 0x0200000, 0x100000, 0x1887e5c0 )
	ROM_CONTINUE( 0x0600000, 0x100000 | ROMFLAG_ALTERNATE )
ROM_END

ROM_START( aof )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "044-p1.p1", 0x000000, 0x080000, 0xca9f7a6d )
	NEO_SFIX_128K( "044-s1.s1", 0x89903f39 )
	NEO_BIOS_SOUND_128K( "044-m1.m1", 0x0987e4bb )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "044-v2.v2", 0x000000, 0x200000, 0x3ec632ea )
	ROM_LOAD( "044-v4.v4", 0x200000, 0x200000, 0x4b0f8e23 )
	ROM_REGION( 0x800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "044-c1.c1", 0x0000000, 0x100000, 0xddab98a7 )
	ROM_CONTINUE( 0x0400001, 0x100000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_GFX_ODD( "044-c2.c2", 0x0000000, 0x100000, 0xd8ccd575 )
	ROM_CONTINUE( 0x0400000, 0x100000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_GFX_EVEN( "044-c3.c3", 0x0200000, 0x100000, 0x403e898a )
	ROM_CONTINUE( 0x0600001, 0x100000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_GFX_ODD( "044-c4.c4", 0x0200000, 0x100000, 0x6235fbaa )
	ROM_CONTINUE( 0x0600000, 0x100000 | ROMFLAG_ALTERNATE )
ROM_END

ROM_START( samsho )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "045-p1.p1", 0x000000, 0x100000, 0xdfe51bf0 )
	ROM_LOAD_WIDE_SWAP( "045-pg2.sp2", 0x100000, 0x100000, 0x46745b94 )
	NEO_SFIX_128K( "045-s1.s1", 0x9142a4d3 )
	NEO_BIOS_SOUND_128K( "045-m1.m1", 0x95170640 )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "045-v1.v1", 0x000000, 0x200000, 0x37f78a9b )
	ROM_LOAD( "045-v2.v2", 0x200000, 0x200000, 0x568b20cf )
	ROM_REGION( 0xa00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "045-c1.c1", 0x0000000, 0x200000, 0x2e5873a4 )
	ROM_LOAD_GFX_ODD( "045-c2.c2", 0x0000000, 0x200000, 0x04febb10 )
	ROM_LOAD_GFX_EVEN( "045-c3.c3", 0x0400000, 0x200000, 0xf3dabd1e )
	ROM_LOAD_GFX_ODD( "045-c4.c4", 0x0400000, 0x200000, 0x935c62f0 )
	ROM_LOAD_GFX_EVEN( "045-c51.c5", 0x0800000, 0x100000, 0x81932894 )
	ROM_LOAD_GFX_ODD( "045-c61.c6", 0x0800000, 0x100000, 0xbe30612e )
ROM_END

ROM_START( tophuntr )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "046-p1.p1", 0x000000, 0x100000, 0x69fa9e29 )
	ROM_LOAD_WIDE_SWAP( "046-p2.sp2", 0x100000, 0x100000, 0xf182cb3e )
	NEO_SFIX_128K( "046-s1.s1", 0x14b01d7b )
	NEO_BIOS_SOUND_128K( "046-m1.m1", 0x3f84bb9f )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "046-v1.v1", 0x000000, 0x100000, 0xc1f9c2db )
	ROM_LOAD( "046-v2.v2", 0x100000, 0x100000, 0x56254a64 )
	ROM_LOAD( "046-v3.v3", 0x200000, 0x100000, 0x58113fb1 )
	ROM_LOAD( "046-v4.v4", 0x300000, 0x100000, 0x4f54c187 )
	ROM_REGION( 0x800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "046-c1.c1", 0x0000000, 0x100000, 0xfa720a4a )
	ROM_LOAD_GFX_ODD( "046-c2.c2", 0x0000000, 0x100000, 0xc900c205 )
	ROM_LOAD_GFX_EVEN( "046-c3.c3", 0x0200000, 0x100000, 0x880e3c25 )
	ROM_LOAD_GFX_ODD( "046-c4.c4", 0x0200000, 0x100000, 0x7a2248aa )
	ROM_LOAD_GFX_EVEN( "046-c5.c5", 0x0400000, 0x100000, 0x4b735e45 )
	ROM_LOAD_GFX_ODD( "046-c6.c6", 0x0400000, 0x100000, 0x273171df )
	ROM_LOAD_GFX_EVEN( "046-c7.c7", 0x0600000, 0x100000, 0x12829c4c )
	ROM_LOAD_GFX_ODD( "046-c8.c8", 0x0600000, 0x100000, 0xc944e03d )
ROM_END

ROM_START( fatfury2 )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "047-p1.p1", 0x000000, 0x100000, 0xecfdbb69 )
	NEO_SFIX_128K( "047-s1.s1", 0xd7dbbf39 )
	NEO_BIOS_SOUND_128K( "047-m1.m1", 0x820b0ba7 )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "047-v1.v1", 0x000000, 0x200000, 0xd9d00784 )
	ROM_LOAD( "047-v2.v2", 0x200000, 0x200000, 0x2c9a4b33 )
	ROM_REGION( 0x800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "047-c1.c1", 0x0000000, 0x100000, 0xf72a939e )
	ROM_CONTINUE( 0x0400001, 0x100000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_GFX_ODD( "047-c2.c2", 0x0000000, 0x100000, 0x05119a0d )
	ROM_CONTINUE( 0x0400000, 0x100000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_GFX_EVEN( "047-c3.c3", 0x0200000, 0x100000, 0x01e00738 )
	ROM_CONTINUE( 0x0600001, 0x100000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_GFX_ODD( "047-c4.c4", 0x0200000, 0x100000, 0x9fe27432 )
	ROM_CONTINUE( 0x0600000, 0x100000 | ROMFLAG_ALTERNATE )
ROM_END

ROM_START( janshin )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "048-p1.p1", 0x000000, 0x100000, 0xfa818cbb )
	NEO_SFIX_128K( "048-s1.s1", 0x8285b25a )
	NEO_BIOS_SOUND_128K( "048-m1.m1", 0x310467c7 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "048-v1.v1", 0x000000, 0x200000, 0xf1947d2b )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "048-c1.c1", 0x0000000, 0x200000, 0x3fa890e9 )
	ROM_LOAD_GFX_ODD( "048-c2.c2", 0x0000000, 0x200000, 0x59c48ad8 )
ROM_END

ROM_START( androdun )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "049-p1.p1", 0x000000, 0x080000, 0x3b857da2 )
	ROM_LOAD_WIDE_SWAP( "049-p2.p2", 0x080000, 0x080000, 0x2f062209 )
	NEO_SFIX_128K( "049-s1.s1", 0x6349de5d )
	NEO_BIOS_SOUND_128K( "049-m1.m1", 0xedd2acf4 )
	ROM_REGION( 0x100000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "049-v1.v1", 0x000000, 0x100000, 0xce43cb89 )
	ROM_REGION( 0x200000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "049-c1.c1", 0x0000000, 0x100000, 0x7ace6db3 )
	ROM_LOAD_GFX_ODD( "049-c2.c2", 0x0000000, 0x100000, 0xb17024f7 )
ROM_END

ROM_START( ncommand )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "050-p1.p1", 0x000000, 0x100000, 0x4e097c40 )
	NEO_SFIX_128K( "050-s1.s1", 0xdb8f9c8e )
	NEO_BIOS_SOUND_128K( "050-m1.m1", 0x6fcf07d3 )
	ROM_REGION( 0x180000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "050-v1.v1", 0x000000, 0x100000, 0x23c3ab42 )
	ROM_LOAD( "050-v2.v2", 0x100000, 0x080000, 0x80b8a984 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "050-c1.c1", 0x0000000, 0x100000, 0x87421a0a )
	ROM_LOAD_GFX_ODD( "050-c2.c2", 0x0000000, 0x100000, 0xc4cf5548 )
	ROM_LOAD_GFX_EVEN( "050-c3.c3", 0x0200000, 0x100000, 0x03422c1e )
	ROM_LOAD_GFX_ODD( "050-c4.c4", 0x0200000, 0x100000, 0x0845eadb )
ROM_END

ROM_START( viewpoin )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "051-p1.p1", 0x000000, 0x100000, 0x17aa899d )
	NEO_SFIX_128K( "051-s1.s1", 0x9fea5758 )
	NEO_BIOS_SOUND_128K( "051-m1.m1", 0x8e69f29a )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "051-v2.v1", 0x000000, 0x200000, 0x019978b6 )
	ROM_LOAD( "051-v4.v2", 0x200000, 0x200000, 0x5758f38c )
	ROM_REGION( 0x600000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "051-c1.c1", 0x0000000, 0x100000, 0xd624c132 )
	ROM_CONTINUE( 0x0400001, 0x100000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_GFX_ODD( "051-c2.c2", 0x0000000, 0x100000, 0x40d69f1e )
	ROM_CONTINUE( 0x0400000, 0x100000 | ROMFLAG_ALTERNATE )
ROM_END

ROM_START( ssideki )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "052-p1.p1", 0x000000, 0x080000, 0x9cd97256 )
	NEO_SFIX_128K( "052-s1.s1", 0x97689804 )
	NEO_BIOS_SOUND_128K( "052-m1.m1", 0x49f17d2d )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "052-v1.v1", 0x000000, 0x200000, 0x22c097a5 )
	ROM_REGION( 0x600000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "052-c1.c1", 0x0000000, 0x100000, 0x53e1c002 )
	ROM_CONTINUE( 0x0400001, 0x100000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_GFX_ODD( "052-c2.c2", 0x0000000, 0x100000, 0x776a2d1f )
	ROM_CONTINUE( 0x0400000, 0x100000 | ROMFLAG_ALTERNATE )
ROM_END

ROM_START( wh1 )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "053-epr.p1", 0x000000, 0x080000, 0xd42e1e9a )
	ROM_LOAD_WIDE_SWAP( "053-epr.p2", 0x080000, 0x080000, 0x0e33e8a3 )
	NEO_SFIX_128K( "053-s1.s1", 0x8c2c2d6b )
	NEO_BIOS_SOUND_128K( "053-m1.m1", 0x1bd9d04b )
	ROM_REGION( 0x300000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "053-v2.v2", 0x000000, 0x200000, 0xa68df485 )
	ROM_LOAD( "053-v4.v4", 0x200000, 0x100000, 0x7bea8f66 )
	ROM_REGION( 0x600000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "053-c1.c1", 0x0000000, 0x100000, 0x85eb5bce )
	ROM_CONTINUE( 0x0400001, 0x100000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_GFX_ODD( "053-c2.c2", 0x0000000, 0x100000, 0xec93b048 )
	ROM_CONTINUE( 0x0400000, 0x100000 | ROMFLAG_ALTERNATE )
	ROM_LOAD_GFX_EVEN( "053-c3.c3", 0x0200000, 0x100000, 0x0dd64965 )
	ROM_LOAD_GFX_ODD( "053-c4.c4", 0x0200000, 0x100000, 0x9270d954 )
ROM_END

ROM_START( kof94 )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "055-p1.p1", 0x100000, 0x100000, 0xf10a2042 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "055-s1.s1", 0x825976c1 )
	NEO_BIOS_SOUND_128K( "055-m1.m1", 0xf6e77cf5 )
	ROM_REGION( 0x600000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "055-v1.v1", 0x000000, 0x200000, 0x8889596d )
	ROM_LOAD( "055-v2.v2", 0x200000, 0x200000, 0x25022b27 )
	ROM_LOAD( "055-v3.v3", 0x400000, 0x200000, 0x83cf32c0 )
	ROM_REGION( 0x1000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "055-c1.c1", 0x0000000, 0x200000, 0xb96ef460 )
	ROM_LOAD_GFX_ODD( "055-c2.c2", 0x0000000, 0x200000, 0x15e096a7 )
	ROM_LOAD_GFX_EVEN( "055-c3.c3", 0x0400000, 0x200000, 0x54f66254 )
	ROM_LOAD_GFX_ODD( "055-c4.c4", 0x0400000, 0x200000, 0x0b01765f )
	ROM_LOAD_GFX_EVEN( "055-c5.c5", 0x0800000, 0x200000, 0xee759363 )
	ROM_LOAD_GFX_ODD( "055-c6.c6", 0x0800000, 0x200000, 0x498da52c )
	ROM_LOAD_GFX_EVEN( "055-c7.c7", 0x0c00000, 0x200000, 0x62f66888 )
	ROM_LOAD_GFX_ODD( "055-c8.c8", 0x0c00000, 0x200000, 0xfe0a235d )
ROM_END

ROM_START( aof2 )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "056-p1.p1", 0x000000, 0x100000, 0xa3b1d021 )
	NEO_SFIX_128K( "056-s1.s1", 0x8b02638e )
	NEO_BIOS_SOUND_128K( "056-m1.m1", 0xf27e9d52 )
	ROM_REGION( 0x500000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "056-v1.v1", 0x000000, 0x200000, 0x4628fde0 )
	ROM_LOAD( "056-v2.v2", 0x200000, 0x200000, 0xb710e2f2 )
	ROM_LOAD( "056-v3.v3", 0x400000, 0x100000, 0xd168c301 )
	ROM_REGION( 0x1000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "056-c1.c1", 0x0000000, 0x200000, 0x17b9cbd2 )
	ROM_LOAD_GFX_ODD( "056-c2.c2", 0x0000000, 0x200000, 0x5fd76b67 )
	ROM_LOAD_GFX_EVEN( "056-c3.c3", 0x0400000, 0x200000, 0xd2c88768 )
	ROM_LOAD_GFX_ODD( "056-c4.c4", 0x0400000, 0x200000, 0xdb39b883 )
	ROM_LOAD_GFX_EVEN( "056-c5.c5", 0x0800000, 0x200000, 0xc3074137 )
	ROM_LOAD_GFX_ODD( "056-c6.c6", 0x0800000, 0x200000, 0x31de68d3 )
	ROM_LOAD_GFX_EVEN( "056-c7.c7", 0x0c00000, 0x200000, 0x3f36df57 )
	ROM_LOAD_GFX_ODD( "056-c8.c8", 0x0c00000, 0x200000, 0xe546d7a8 )
ROM_END

ROM_START( wh2 )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "057-p1.p1", 0x100000, 0x100000, 0x65a891d9 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "057-s1.s1", 0xfcaeb3a4 )
	NEO_BIOS_SOUND_128K( "057-m1.m1", 0x8fa3bc77 )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "057-v1.v1", 0x000000, 0x200000, 0x8877e301 )
	ROM_LOAD( "057-v2.v2", 0x200000, 0x200000, 0xc1317ff4 )
	ROM_REGION( 0xc00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "057-c1.c1", 0x0000000, 0x200000, 0x21c6bb91 )
	ROM_LOAD_GFX_ODD( "057-c2.c2", 0x0000000, 0x200000, 0xa3999925 )
	ROM_LOAD_GFX_EVEN( "057-c3.c3", 0x0400000, 0x200000, 0xb725a219 )
	ROM_LOAD_GFX_ODD( "057-c4.c4", 0x0400000, 0x200000, 0x8d96425e )
	ROM_LOAD_GFX_EVEN( "057-c5.c5", 0x0800000, 0x200000, 0xb20354af )
	ROM_LOAD_GFX_ODD( "057-c6.c6", 0x0800000, 0x200000, 0xb13d1de3 )
ROM_END

ROM_START( fatfursp )
	ROM_REGION( 0x180000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "058-p1.p1", 0x000000, 0x100000, 0x2f585ba2 )
	ROM_LOAD_WIDE_SWAP( "058-p2.sp2", 0x100000, 0x080000, 0xd7c71a6b )
	NEO_SFIX_128K( "058-s1.s1", 0x2df03197 )
	NEO_BIOS_SOUND_128K( "058-m1.m1", 0xccc5186e )
	ROM_REGION( 0x500000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "058-v1.v1", 0x000000, 0x200000, 0x55d7ce84 )
	ROM_LOAD( "058-v2.v2", 0x200000, 0x200000, 0xee080b10 )
	ROM_LOAD( "058-v3.v3", 0x400000, 0x100000, 0xf9eb3d4a )
	ROM_REGION( 0xc00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "058-c1.c1", 0x0000000, 0x200000, 0x044ab13c )
	ROM_LOAD_GFX_ODD( "058-c2.c2", 0x0000000, 0x200000, 0x11e6bf96 )
	ROM_LOAD_GFX_EVEN( "058-c3.c3", 0x0400000, 0x200000, 0x6f7938d5 )
	ROM_LOAD_GFX_ODD( "058-c4.c4", 0x0400000, 0x200000, 0x4ad066ff )
	ROM_LOAD_GFX_EVEN( "058-c5.c5", 0x0800000, 0x200000, 0x49c5e0bf )
	ROM_LOAD_GFX_ODD( "058-c6.c6", 0x0800000, 0x200000, 0x8ff1f43d )
ROM_END

ROM_START( savagere )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "059-p1.p1", 0x100000, 0x100000, 0x01d4e9c0 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "059-s1.s1", 0xe08978ca )
	NEO_BIOS_SOUND_128K( "059-m1.m1", 0x29992eba )
	ROM_REGION( 0x600000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "059-v1.v1", 0x000000, 0x200000, 0x530c50fd )
	ROM_LOAD( "059-v2.v2", 0x200000, 0x200000, 0xeb6f1cdb )
	ROM_LOAD( "059-v3.v3", 0x400000, 0x200000, 0x7038c2f9 )
	ROM_REGION( 0x1000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "059-c1.c1", 0x0000000, 0x200000, 0x763ba611 )
	ROM_LOAD_GFX_ODD( "059-c2.c2", 0x0000000, 0x200000, 0xe05e8ca6 )
	ROM_LOAD_GFX_EVEN( "059-c3.c3", 0x0400000, 0x200000, 0x3e4eba4b )
	ROM_LOAD_GFX_ODD( "059-c4.c4", 0x0400000, 0x200000, 0x3c2a3808 )
	ROM_LOAD_GFX_EVEN( "059-c5.c5", 0x0800000, 0x200000, 0x59013f9e )
	ROM_LOAD_GFX_ODD( "059-c6.c6", 0x0800000, 0x200000, 0x1c8d5def )
	ROM_LOAD_GFX_EVEN( "059-c7.c7", 0x0c00000, 0x200000, 0xc88f7035 )
	ROM_LOAD_GFX_ODD( "059-c8.c8", 0x0c00000, 0x200000, 0x484ce3ba )
ROM_END

ROM_START( fightfev )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "060-p1.p1", 0x0000000, 0x100000, 0x2a104b50 )
	NEO_SFIX_128K( "060-s1.s1", 0xd62a72e9 )
	NEO_BIOS_SOUND_128K( "060-m1.m1", 0x0b7c4e65 )
	ROM_REGION( 0x300000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "060-v1.v1", 0x000000, 0x200000, 0xf417c215 )
	ROM_LOAD( "060-v2.v2", 0x200000, 0x100000, 0xefcff7cf )
	ROM_REGION( 0x0800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "060-c1.c1", 0x0000000, 0x200000, 0x8908fff9 )
	ROM_LOAD_GFX_ODD( "060-c2.c2", 0x0000000, 0x200000, 0xc6649492 )
	ROM_LOAD_GFX_EVEN( "060-c3.c3", 0x0400000, 0x200000, 0x0956b437 )
	ROM_LOAD_GFX_ODD( "060-c4.c4", 0x0400000, 0x200000, 0x026f3b62 )
ROM_END

ROM_START( ssideki2 )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "061-p1.p1", 0x000000, 0x100000, 0x5969e0dc )
	NEO_SFIX_128K( "061-s1.s1", 0x226d1b68 )
	NEO_BIOS_SOUND_128K( "061-m1.m1", 0x156f6951 )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "061-v1.v1", 0x000000, 0x200000, 0xf081c8d3 )
	ROM_LOAD( "061-v2.v2", 0x200000, 0x200000, 0x7cd63302 )
	ROM_REGION( 0x800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "061-c1-16.c1", 0x0000000, 0x200000, 0xa626474f )
	ROM_LOAD_GFX_ODD( "061-c2-16.c2", 0x0000000, 0x200000, 0xc3be42ae )
	ROM_LOAD_GFX_EVEN( "061-c3-16.c3", 0x0400000, 0x200000, 0x2a7b98b9 )
	ROM_LOAD_GFX_ODD( "061-c4-16.c4", 0x0400000, 0x200000, 0xc0be9a1f )
ROM_END

ROM_START( spinmast )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "062-p1.p1", 0x000000, 0x100000, 0x37aba1aa )
	ROM_LOAD_WIDE_SWAP( "062-p2.sp2", 0x100000, 0x100000, 0xf025ab77 )
	NEO_SFIX_128K( "062-s1.s1", 0x289e2bbe )
	NEO_BIOS_SOUND_128K( "062-m1.m1", 0x76108b2f )
	ROM_REGION( 0x100000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "062-v1.v1", 0x000000, 0x100000, 0xcc281aef )
	ROM_REGION( 0x800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "062-c1.c1", 0x0000000, 0x100000, 0xa9375aa2 )
	ROM_LOAD_GFX_ODD( "062-c2.c2", 0x0000000, 0x100000, 0x0e73b758 )
	ROM_LOAD_GFX_EVEN( "062-c3.c3", 0x0200000, 0x100000, 0xdf51e465 )
	ROM_LOAD_GFX_ODD( "062-c4.c4", 0x0200000, 0x100000, 0x38517e90 )
	ROM_LOAD_GFX_EVEN( "062-c5.c5", 0x0400000, 0x100000, 0x7babd692 )
	ROM_LOAD_GFX_ODD( "062-c6.c6", 0x0400000, 0x100000, 0xcde5ade5 )
	ROM_LOAD_GFX_EVEN( "062-c7.c7", 0x0600000, 0x100000, 0xbb2fd7c0 )
	ROM_LOAD_GFX_ODD( "062-c8.c8", 0x0600000, 0x100000, 0x8d7be933 )
ROM_END

ROM_START( samsho2 )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "063-p1.p1", 0x100000, 0x100000, 0x22368892 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "063-s1.s1", 0x64a5cd66 )
	NEO_BIOS_SOUND_128K( "063-m1.m1", 0x56675098 )
	ROM_REGION( 0x700000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "063-v1.v1", 0x000000, 0x200000, 0x37703f91 )
	ROM_LOAD( "063-v2.v2", 0x200000, 0x200000, 0x0142bde8 )
	ROM_LOAD( "063-v3.v3", 0x400000, 0x200000, 0xd07fa5ca )
	ROM_LOAD( "063-v4.v4", 0x600000, 0x100000, 0x24aab4bb )
	ROM_REGION( 0x1000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "063-c1.c1", 0x0000000, 0x200000, 0x86cd307c )
	ROM_LOAD_GFX_ODD( "063-c2.c2", 0x0000000, 0x200000, 0xcdfcc4ca )
	ROM_LOAD_GFX_EVEN( "063-c3.c3", 0x0400000, 0x200000, 0x7a63ccc7 )
	ROM_LOAD_GFX_ODD( "063-c4.c4", 0x0400000, 0x200000, 0x751025ce )
	ROM_LOAD_GFX_EVEN( "063-c5.c5", 0x0800000, 0x200000, 0x20d3a475 )
	ROM_LOAD_GFX_ODD( "063-c6.c6", 0x0800000, 0x200000, 0xae4c0a88 )
	ROM_LOAD_GFX_EVEN( "063-c7.c7", 0x0c00000, 0x200000, 0x2df3cbcf )
	ROM_LOAD_GFX_ODD( "063-c8.c8", 0x0c00000, 0x200000, 0x1ffc6dfa )
ROM_END

ROM_START( wh2j )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "064-p1.p1", 0x100000, 0x100000, 0x385a2e86 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "064-s1.s1", 0x2a03998a )
	NEO_BIOS_SOUND_128K( "064-m1.m1", 0xd2eec9d3 )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "064-v1.v1", 0x000000, 0x200000, 0xaa277109 )
	ROM_LOAD( "064-v2.v2", 0x200000, 0x200000, 0xb6527edd )
	ROM_REGION( 0x1000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "064-c1.c1", 0x0000000, 0x200000, 0x2ec87cea )
	ROM_LOAD_GFX_ODD( "064-c2.c2", 0x0000000, 0x200000, 0x526b81ab )
	ROM_LOAD_GFX_EVEN( "064-c3.c3", 0x0400000, 0x200000, 0x436d1b31 )
	ROM_LOAD_GFX_ODD( "064-c4.c4", 0x0400000, 0x200000, 0xf9c8dd26 )
	ROM_LOAD_GFX_EVEN( "064-c5.c5", 0x0800000, 0x200000, 0x8e34a9f4 )
	ROM_LOAD_GFX_ODD( "064-c6.c6", 0x0800000, 0x200000, 0xa43e4766 )
	ROM_LOAD_GFX_EVEN( "064-c7.c7", 0x0c00000, 0x200000, 0x59d97215 )
	ROM_LOAD_GFX_ODD( "064-c8.c8", 0x0c00000, 0x200000, 0xfc092367 )
ROM_END

ROM_START( wjammers )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "065-p1.p1", 0x000000, 0x100000, 0x6692c140 )
	NEO_SFIX_128K( "065-s1.s1", 0x074b5723 )
	NEO_BIOS_SOUND_128K( "065-m1.m1", 0x52c23cfc )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "065-v1.v1", 0x000000, 0x100000, 0xce8b3698 )
	ROM_LOAD( "065-v2.v2", 0x100000, 0x100000, 0x659f9b96 )
	ROM_LOAD( "065-v3.v3", 0x200000, 0x100000, 0x39f73061 )
	ROM_LOAD( "065-v4.v4", 0x300000, 0x100000, 0x5dee7963 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "065-c1.c1", 0x0000000, 0x100000, 0xc7650204 )
	ROM_LOAD_GFX_ODD( "065-c2.c2", 0x0000000, 0x100000, 0xd9f3e71d )
	ROM_LOAD_GFX_EVEN( "065-c3.c3", 0x0200000, 0x100000, 0x40986386 )
	ROM_LOAD_GFX_ODD( "065-c4.c4", 0x0200000, 0x100000, 0x715e15ff )
ROM_END

ROM_START( karnovr )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "066-p1.p1", 0x000000, 0x100000, 0x8c86fd22 )
	NEO_SFIX_128K( "066-s1.s1", 0xbae5d5e5 )
	NEO_BIOS_SOUND_128K( "066-m1.m1", 0x030beae4 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "066-v1.v1", 0x000000, 0x200000, 0x0b7ea37a )
	ROM_REGION( 0xc00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "066-c1.c1", 0x0000000, 0x200000, 0x09dfe061 )
	ROM_LOAD_GFX_ODD( "066-c2.c2", 0x0000000, 0x200000, 0xe0f6682a )
	ROM_LOAD_GFX_EVEN( "066-c3.c3", 0x0400000, 0x200000, 0xa673b4f7 )
	ROM_LOAD_GFX_ODD( "066-c4.c4", 0x0400000, 0x200000, 0xcb3dc5f4 )
	ROM_LOAD_GFX_EVEN( "066-c5.c5", 0x0800000, 0x200000, 0x9a28785d )
	ROM_LOAD_GFX_ODD( "066-c6.c6", 0x0800000, 0x200000, 0xc15c01ed )
ROM_END

ROM_START( gururin )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "067-p1.p1", 0x000000, 0x80000, 0x4cea8a49 )
	NEO_SFIX_128K( "067-s1.s1", 0xb119e1eb )
	NEO_BIOS_SOUND_128K( "067-m1.m1", 0x9e3c6328 )
	ROM_REGION( 0x80000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "067-v1.v1", 0x000000, 0x80000, 0xcf23afd0 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "067-c1.c1", 0x0000000, 0x200000, 0x35866126 )
	ROM_LOAD_GFX_ODD( "067-c2.c2", 0x0000000, 0x200000, 0x9db64084 )
ROM_END

ROM_START( pspikes2 )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "068-pg1.p1", 0x000000, 0x100000, 0x105a408f )
	NEO_SFIX_128K( "068-sg1.s1", 0x18082299 )
	NEO_BIOS_SOUND_128K( "068-mg1.m1", 0xb1c7911e )
	ROM_REGION( 0x300000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "068-v1.v1", 0x000000, 0x100000, 0x2ced86df )
	ROM_LOAD( "068-v2.v2", 0x100000, 0x100000, 0x970851ab )
	ROM_LOAD( "068-v3.v3", 0x200000, 0x100000, 0x81ff05aa )
	ROM_REGION( 0x600000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "068-c1.c1", 0x0000000, 0x100000, 0x7f250f76 )
	ROM_LOAD_GFX_ODD( "068-c2.c2", 0x0000000, 0x100000, 0x20912873 )
	ROM_LOAD_GFX_EVEN( "068-c3.c3", 0x0200000, 0x100000, 0x4b641ba1 )
	ROM_LOAD_GFX_ODD( "068-c4.c4", 0x0200000, 0x100000, 0x35072596 )
	ROM_LOAD_GFX_EVEN( "068-c5.c5", 0x0400000, 0x100000, 0x151dd624 )
	ROM_LOAD_GFX_ODD( "068-c6.c6", 0x0400000, 0x100000, 0xa6722604 )
ROM_END

ROM_START( fatfury3 )
	ROM_REGION( 0x300000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "069-p1.p1", 0x000000, 0x100000, 0xa8bcfbbc )
	ROM_LOAD_WIDE_SWAP( "069-sp2.sp2", 0x100000, 0x200000, 0xdbe963ed )
	NEO_SFIX_128K( "069-s1.s1", 0x0b33a800 )
	NEO_BIOS_SOUND_128K( "069-m1.m1", 0xfce72926 )
	ROM_REGION( 0xa00000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "069-v1.v1", 0x000000, 0x400000, 0x2bdbd4db )
	ROM_LOAD( "069-v2.v2", 0x400000, 0x400000, 0xa698a487 )
	ROM_LOAD( "069-v3.v3", 0x800000, 0x200000, 0x581c5304 )
	ROM_REGION( 0x1400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "069-c1.c1", 0x0000000, 0x400000, 0xe302f93c )
	ROM_LOAD_GFX_ODD( "069-c2.c2", 0x0000000, 0x400000, 0x1053a455 )
	ROM_LOAD_GFX_EVEN( "069-c3.c3", 0x0800000, 0x400000, 0x1c0fde2f )
	ROM_LOAD_GFX_ODD( "069-c4.c4", 0x0800000, 0x400000, 0xa25fc3d0 )
	ROM_LOAD_GFX_EVEN( "069-c5.c5", 0x1000000, 0x200000, 0xb3ec6fa6 )
	ROM_LOAD_GFX_ODD( "069-c6.c6", 0x1000000, 0x200000, 0x69210441 )
ROM_END

ROM_START( panicbom )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "073-p1.p1", 0x000000, 0x080000, 0xadc356ad )
	NEO_SFIX_128K( "073-s1.s1", 0xb876de7e )
	NEO_BIOS_SOUND_128K( "073-m1.m1", 0x3cdf5d88 )
	ROM_REGION( 0x300000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "073-v1.v1", 0x000000, 0x200000, 0x7fc86d2f )
	ROM_LOAD( "073-v2.v2", 0x200000, 0x100000, 0x082adfc7 )
	ROM_REGION( 0x200000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "073-c1.c1", 0x0000000, 0x100000, 0x8582e1b5 )
	ROM_LOAD_GFX_ODD( "073-c2.c2", 0x0000000, 0x100000, 0xe15a093b )
ROM_END

ROM_START( aodk )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "074-p1.p1", 0x100000, 0x100000, 0x62369553 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "074-s1.s1", 0x96148d2b )
	NEO_BIOS_SOUND_128K( "074-m1.m1", 0x5a52a9d1 )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "074-v1.v1", 0x000000, 0x200000, 0x7675b8fa )
	ROM_LOAD( "074-v2.v2", 0x200000, 0x200000, 0xa9da86e9 )
	ROM_REGION( 0x1000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "074-c1.c1", 0x0000000, 0x200000, 0xa0b39344 )
	ROM_LOAD_GFX_ODD( "074-c2.c2", 0x0000000, 0x200000, 0x203f6074 )
	ROM_LOAD_GFX_EVEN( "074-c3.c3", 0x0400000, 0x200000, 0x7fff4d41 )
	ROM_LOAD_GFX_ODD( "074-c4.c4", 0x0400000, 0x200000, 0x48db3e0a )
	ROM_LOAD_GFX_EVEN( "074-c5.c5", 0x0800000, 0x200000, 0xc74c5e51 )
	ROM_LOAD_GFX_ODD( "074-c6.c6", 0x0800000, 0x200000, 0x73e8e7e0 )
	ROM_LOAD_GFX_EVEN( "074-c7.c7", 0x0c00000, 0x200000, 0xac7daa01 )
	ROM_LOAD_GFX_ODD( "074-c8.c8", 0x0c00000, 0x200000, 0x14e7ad71 )
ROM_END

ROM_START( sonicwi2 )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "075-p1.p1", 0x100000, 0x100000, 0x92871738 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "075-s1.s1", 0xc9eec367 )
	NEO_BIOS_SOUND_128K( "075-m1.m1", 0xbb828df1 )
	ROM_REGION( 0x300000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "075-v1.v1", 0x000000, 0x200000, 0x7577e949 )
	ROM_LOAD( "075-v2.v2", 0x200000, 0x100000, 0x021760cd )
	ROM_REGION( 0x800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "075-c1.c1", 0x0000000, 0x200000, 0x3278e73e )
	ROM_LOAD_GFX_ODD( "075-c2.c2", 0x0000000, 0x200000, 0xfe6355d6 )
	ROM_LOAD_GFX_EVEN( "075-c3.c3", 0x0400000, 0x200000, 0xc1b438f1 )
	ROM_LOAD_GFX_ODD( "075-c4.c4", 0x0400000, 0x200000, 0x1f777206 )
ROM_END

ROM_START( zedblade )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "076-p1.p1", 0x000000, 0x080000, 0xd7c1effd )
	NEO_SFIX_128K( "076-s1.s1", 0xf4c25dd5 )
	NEO_BIOS_SOUND_128K( "076-m1.m1", 0x7b5f3d0a )
	ROM_REGION( 0x500000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "076-v1.v1", 0x000000, 0x200000, 0x1a21d90c )
	ROM_LOAD( "076-v2.v2", 0x200000, 0x200000, 0xb61686c3 )
	ROM_LOAD( "076-v3.v3", 0x400000, 0x100000, 0xb90658fa )
	ROM_REGION( 0x800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "076-c1.c1", 0x0000000, 0x200000, 0x4d9cb038 )
	ROM_LOAD_GFX_ODD( "076-c2.c2", 0x0000000, 0x200000, 0x09233884 )
	ROM_LOAD_GFX_EVEN( "076-c3.c3", 0x0400000, 0x200000, 0xd06431e3 )
	ROM_LOAD_GFX_ODD( "076-c4.c4", 0x0400000, 0x200000, 0x4b1c089b )
ROM_END

ROM_START( galaxyfg )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "078-p1.p1", 0x100000, 0x100000, 0x45906309 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "078-s1.s1", 0x72f8923e )
	NEO_BIOS_SOUND_128K( "078-m1.m1", 0x8e9e3b10 )
	ROM_REGION( 0x500000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "078-v1.v1", 0x000000, 0x200000, 0xe3b735ac )
	ROM_LOAD( "078-v2.v2", 0x200000, 0x200000, 0x6a8e78c2 )
	ROM_LOAD( "078-v3.v3", 0x400000, 0x100000, 0x70bca656 )
	ROM_REGION( 0xe00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "078-c1.c1", 0x0000000, 0x200000, 0xc890c7c0 )
	ROM_LOAD_GFX_ODD( "078-c2.c2", 0x0000000, 0x200000, 0xb6d25419 )
	ROM_LOAD_GFX_EVEN( "078-c3.c3", 0x0400000, 0x200000, 0x9d87e761 )
	ROM_LOAD_GFX_ODD( "078-c4.c4", 0x0400000, 0x200000, 0x765d7cb8 )
	ROM_LOAD_GFX_EVEN( "078-c5.c5", 0x0800000, 0x200000, 0xe6b77e6a )
	ROM_LOAD_GFX_ODD( "078-c6.c6", 0x0800000, 0x200000, 0xd779a181 )
	ROM_LOAD_GFX_EVEN( "078-c7.c7", 0x0c00000, 0x100000, 0x4f27d580 )
	ROM_LOAD_GFX_ODD( "078-c8.c8", 0x0c00000, 0x100000, 0x0a7cc0d8 )
ROM_END

ROM_START( strhoop )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "079-p1.p1", 0x000000, 0x100000, 0x5e78328e )
	NEO_SFIX_128K( "079-s1.s1", 0x3ac06665 )
	NEO_BIOS_SOUND_128K( "079-m1.m1", 0xbee3455a )
	ROM_REGION( 0x300000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "079-v1.v1", 0x000000, 0x200000, 0x718a2400 )
	ROM_LOAD( "079-v2.v2", 0x200000, 0x100000, 0x720774eb )
	ROM_REGION( 0x800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "079-c1.c1", 0x0000000, 0x200000, 0x0581c72a )
	ROM_LOAD_GFX_ODD( "079-c2.c2", 0x0000000, 0x200000, 0x5b9b8fb6 )
	ROM_LOAD_GFX_EVEN( "079-c3.c3", 0x0400000, 0x200000, 0xcd65bb62 )
	ROM_LOAD_GFX_ODD( "079-c4.c4", 0x0400000, 0x200000, 0xa4c90213 )
ROM_END

ROM_START( quizkof )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "080-p1.p1", 0x000000, 0x100000, 0x4440315e )
	NEO_SFIX_128K( "080-s1.s1", 0xd7b86102 )
	NEO_BIOS_SOUND_128K( "080-m1.m1", 0xf5f44172 )
	ROM_REGION( 0x600000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "080-v1.v1", 0x000000, 0x200000, 0x0be18f60 )
	ROM_LOAD( "080-v2.v2", 0x200000, 0x200000, 0x4abde3ff )
	ROM_LOAD( "080-v3.v3", 0x400000, 0x200000, 0xf02844e2 )
	ROM_REGION( 0x800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "080-c1.c1", 0x0000000, 0x200000, 0xea1d764a )
	ROM_LOAD_GFX_ODD( "080-c2.c2", 0x0000000, 0x200000, 0xd331d4a4 )
	ROM_LOAD_GFX_EVEN( "080-c3.c3", 0x0400000, 0x200000, 0xb4851bfe )
	ROM_LOAD_GFX_ODD( "080-c4.c4", 0x0400000, 0x200000, 0xca6f5460 )
ROM_END

ROM_START( ssideki3 )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "081-p1.p1", 0x100000, 0x100000, 0x6bc27a3d )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "081-s1.s1", 0x7626da34 )
	NEO_BIOS_SOUND_128K( "081-m1.m1", 0x82fcd863 )
	ROM_REGION( 0x600000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "081-v1.v1", 0x000000, 0x200000, 0x201fa1e1 )
	ROM_LOAD( "081-v2.v2", 0x200000, 0x200000, 0xacf29d96 )
	ROM_LOAD( "081-v3.v3", 0x400000, 0x200000, 0xe524e415 )
	ROM_REGION( 0xc00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "081-c1.c1", 0x0000000, 0x200000, 0x1fb68ebe )
	ROM_LOAD_GFX_ODD( "081-c2.c2", 0x0000000, 0x200000, 0xb28d928f )
	ROM_LOAD_GFX_EVEN( "081-c3.c3", 0x0400000, 0x200000, 0x3b2572e8 )
	ROM_LOAD_GFX_ODD( "081-c4.c4", 0x0400000, 0x200000, 0x47d26a7c )
	ROM_LOAD_GFX_EVEN( "081-c5.c5", 0x0800000, 0x200000, 0x17d42f0d )
	ROM_LOAD_GFX_ODD( "081-c6.c6", 0x0800000, 0x200000, 0x6b53fb75 )
ROM_END

ROM_START( doubledr )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "082-p1.p1", 0x100000, 0x100000, 0x34ab832a )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "082-s1.s1", 0xbef995c5 )
	NEO_BIOS_SOUND_128K( "082-m1.m1", 0x10b144de )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "082-v1.v1", 0x000000, 0x200000, 0xcc1128e4 )
	ROM_LOAD( "082-v2.v2", 0x200000, 0x200000, 0xc3ff5554 )
	ROM_REGION( 0xe00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "082-c1.c1", 0x0000000, 0x200000, 0xb478c725 )
	ROM_LOAD_GFX_ODD( "082-c2.c2", 0x0000000, 0x200000, 0x2857da32 )
	ROM_LOAD_GFX_EVEN( "082-c3.c3", 0x0400000, 0x200000, 0x8b0d378e )
	ROM_LOAD_GFX_ODD( "082-c4.c4", 0x0400000, 0x200000, 0xc7d2f596 )
	ROM_LOAD_GFX_EVEN( "082-c5.c5", 0x0800000, 0x200000, 0xec87bff6 )
	ROM_LOAD_GFX_ODD( "082-c6.c6", 0x0800000, 0x200000, 0x844a8a11 )
	ROM_LOAD_GFX_EVEN( "082-c7.c7", 0x0c00000, 0x100000, 0x727c4d02 )
	ROM_LOAD_GFX_ODD( "082-c8.c8", 0x0c00000, 0x100000, 0x69a5fa37 )
ROM_END

ROM_START( pbobblen )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "d96-07.ep1", 0x000000, 0x080000, 0x6102ca14 )
	NEO_SFIX_128K( "d96-04.s1", 0x9caae538 )
	NEO_BIOS_SOUND_128K( "d96-06.m1", 0xf424368a )
	ROM_REGION( 0x380000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "068-v1.v1", 0x000000, 0x100000, 0x2ced86df )
	ROM_LOAD( "068-v2.v2", 0x100000, 0x100000, 0x970851ab )
	ROM_LOAD( "d96-01.v3", 0x200000, 0x100000, 0x0840cbc4 )
	ROM_LOAD( "d96-05.v4", 0x300000, 0x080000, 0x0a548948 )
	ROM_REGION( 0x500000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "068-c1.c1", 0x0000000, 0x100000, 0x7f250f76 )
	ROM_LOAD_GFX_ODD( "068-c2.c2", 0x0000000, 0x100000, 0x20912873 )
	ROM_LOAD_GFX_EVEN( "068-c3.c3", 0x0200000, 0x100000, 0x4b641ba1 )
	ROM_LOAD_GFX_ODD( "068-c4.c4", 0x0200000, 0x100000, 0x35072596 )
	ROM_LOAD_GFX_EVEN( "d96-02.c5", 0x0400000, 0x080000, 0xe89ad494 )
	ROM_LOAD_GFX_ODD( "d96-03.c6", 0x0400000, 0x080000, 0x4b42d7eb )
ROM_END

ROM_START( kof95 )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "084-p1.p1", 0x100000, 0x100000, 0x2cba2716 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "084-s1.s1", 0xde716f8a )
	NEO_BIOS_SOUND_128K( "084-m1.m1", 0x6f2d7429 )
	ROM_REGION( 0x900000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "084-v1.v1", 0x000000, 0x400000, 0x84861b56 )
	ROM_LOAD( "084-v2.v2", 0x400000, 0x200000, 0xb38a2803 )
	ROM_LOAD( "084-v3.v3", 0x800000, 0x100000, 0xd683a338 )
	ROM_REGION( 0x1a00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "084-c1.c1", 0x0000000, 0x400000, 0xfe087e32 )
	ROM_LOAD_GFX_ODD( "084-c2.c2", 0x0000000, 0x400000, 0x07864e09 )
	ROM_LOAD_GFX_EVEN( "084-c3.c3", 0x0800000, 0x400000, 0xa4e65d1b )
	ROM_LOAD_GFX_ODD( "084-c4.c4", 0x0800000, 0x400000, 0xc1ace468 )
	ROM_LOAD_GFX_EVEN( "084-c5.c5", 0x1000000, 0x200000, 0x8a2c1edc )
	ROM_LOAD_GFX_ODD( "084-c6.c6", 0x1000000, 0x200000, 0xf593ac35 )
	ROM_LOAD_GFX_EVEN( "084-c7.c7", 0x1800000, 0x100000, 0x9904025f )
	ROM_LOAD_GFX_ODD( "084-c8.c8", 0x1800000, 0x100000, 0x78eb0f9b )
ROM_END

ROM_START( twsoc96 )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "086-p1.p1", 0x000000, 0x100000, 0x03e20ab6 )
	NEO_SFIX_128K( "086-s1.s1", 0x6f5e2b3a )
	NEO_BIOS_SOUND_128K( "086-m1.m1", 0xcb82bc5d )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "086-v1.v1", 0x000000, 0x200000, 0x97bf1986 )
	ROM_LOAD( "086-v2.v2", 0x200000, 0x200000, 0xb7eb05df )
	ROM_REGION( 0xa00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "086-c1.c1", 0x0000000, 0x400000, 0x2611bc2a )
	ROM_LOAD_GFX_ODD( "086-c2.c2", 0x0000000, 0x400000, 0x6b0d6827 )
	ROM_LOAD_GFX_EVEN( "086-c3.c3", 0x0800000, 0x100000, 0x750ddc0c )
	ROM_LOAD_GFX_ODD( "086-c4.c4", 0x0800000, 0x100000, 0x7a6e7d82 )
ROM_END

ROM_START( samsho3 )
	ROM_REGION( 0x300000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "087-epr.ep1", 0x000000, 0x080000, 0x23e09bb8 )
	ROM_LOAD_WIDE_SWAP( "087-epr.ep2", 0x080000, 0x080000, 0x256f5302 )
	ROM_LOAD_WIDE_SWAP( "087-epr.ep3", 0x100000, 0x080000, 0xbf2db5dd )
	ROM_LOAD_WIDE_SWAP( "087-epr.ep4", 0x180000, 0x080000, 0x53e60c58 )
	ROM_LOAD_WIDE_SWAP( "087-p5.p5", 0x200000, 0x100000, 0xe86ca4af )
	NEO_SFIX_128K( "087-s1.s1", 0x74ec7d9f )
	NEO_BIOS_SOUND_128K( "087-m1.m1", 0x8e6440eb )
	ROM_REGION( 0x600000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "087-v1.v1", 0x000000, 0x400000, 0x84bdd9a0 )
	ROM_LOAD( "087-v2.v2", 0x400000, 0x200000, 0xac0f261a )
	ROM_REGION( 0x1a00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "087-c1.c1", 0x0000000, 0x400000, 0x07a233bc )
	ROM_LOAD_GFX_ODD( "087-c2.c2", 0x0000000, 0x400000, 0x7a413592 )
	ROM_LOAD_GFX_EVEN( "087-c3.c3", 0x0800000, 0x400000, 0x8b793796 )
	ROM_LOAD_GFX_ODD( "087-c4.c4", 0x0800000, 0x400000, 0x728fbf11 )
	ROM_LOAD_GFX_EVEN( "087-c5.c5", 0x1000000, 0x400000, 0x172ab180 )
	ROM_LOAD_GFX_ODD( "087-c6.c6", 0x1000000, 0x400000, 0x002ff8f3 )
	ROM_LOAD_GFX_EVEN( "087-c7.c7", 0x1800000, 0x100000, 0xae450e3d )
	ROM_LOAD_GFX_ODD( "087-c8.c8", 0x1800000, 0x100000, 0xa9e82717 )
ROM_END

ROM_START( stakwin )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "088-p1.p1", 0x100000, 0x100000, 0xbd5814f6 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "088-s1.s1", 0x073cb208 )
	NEO_BIOS_SOUND_128K( "088-m1.m1", 0x2fe1f499 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "088-v1.v1", 0x000000, 0x200000, 0xb7785023 )
	ROM_REGION( 0x800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "088-c1.c1", 0x0000000, 0x200000, 0x6e733421 )
	ROM_LOAD_GFX_ODD( "088-c2.c2", 0x0000000, 0x200000, 0x4d865347 )
	ROM_LOAD_GFX_EVEN( "088-c3.c3", 0x0400000, 0x200000, 0x8fa5a9eb )
	ROM_LOAD_GFX_ODD( "088-c4.c4", 0x0400000, 0x200000, 0x4604f0dc )
ROM_END

ROM_START( pulstar )
	ROM_REGION( 0x300000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "089-p1.p1", 0x000000, 0x100000, 0x5e5847a2 )
	ROM_LOAD_WIDE_SWAP( "089-p2.sp2", 0x100000, 0x200000, 0x028b774c )
	NEO_SFIX_128K( "089-s1.s1", 0xc79fc2c8 )
	NEO_BIOS_SOUND_128K( "089-m1.m1", 0xff3df7c7 )
	ROM_REGION( 0x800000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "089-v1.v1", 0x000000, 0x400000, 0x6f726ecb )
	ROM_LOAD( "089-v2.v2", 0x400000, 0x400000, 0x9d2db551 )
	ROM_REGION( 0x1c00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "089-c1.c1", 0x0000000, 0x400000, 0xf4e97332 )
	ROM_LOAD_GFX_ODD( "089-c2.c2", 0x0000000, 0x400000, 0x836d14da )
	ROM_LOAD_GFX_EVEN( "089-c3.c3", 0x0800000, 0x400000, 0x913611c4 )
	ROM_LOAD_GFX_ODD( "089-c4.c4", 0x0800000, 0x400000, 0x44cef0e3 )
	ROM_LOAD_GFX_EVEN( "089-c5.c5", 0x1000000, 0x400000, 0x89baa1d7 )
	ROM_LOAD_GFX_ODD( "089-c6.c6", 0x1000000, 0x400000, 0xb2594d56 )
	ROM_LOAD_GFX_EVEN( "089-c7.c7", 0x1800000, 0x200000, 0x6a5618ca )
	ROM_LOAD_GFX_ODD( "089-c8.c8", 0x1800000, 0x200000, 0xa223572d )
ROM_END

ROM_START( whp )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "090-p1.p1", 0x100000, 0x100000, 0xafaa4702 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "090-s1.s1", 0x174a880f )
	NEO_BIOS_SOUND_128K( "090-m1.m1", 0x28065668 )
	ROM_REGION( 0x600000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "090-v1.v1", 0x000000, 0x200000, 0x30cf2709 )
	ROM_LOAD( "064-v2.v2", 0x200000, 0x200000, 0xb6527edd )
	ROM_LOAD( "090-v3.v3", 0x400000, 0x200000, 0x1908a7ce )
	ROM_REGION( 0x1c00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "090-c1.c1", 0x0000000, 0x400000, 0xcd30ed9b )
	ROM_LOAD_GFX_ODD( "090-c2.c2", 0x0000000, 0x400000, 0x10eed5ee )
	ROM_LOAD_GFX_EVEN( "064-c3.c3", 0x0800000, 0x200000, 0x436d1b31 )
	ROM_LOAD_GFX_ODD( "064-c4.c4", 0x0800000, 0x200000, 0xf9c8dd26 )
	ROM_LOAD_GFX_EVEN( "064-c5.c5", 0x1000000, 0x200000, 0x8e34a9f4 )
	ROM_LOAD_GFX_ODD( "064-c6.c6", 0x1000000, 0x200000, 0xa43e4766 )
	ROM_LOAD_GFX_EVEN( "064-c7.c7", 0x1800000, 0x200000, 0x59d97215 )
	ROM_LOAD_GFX_ODD( "064-c8.c8", 0x1800000, 0x200000, 0xfc092367 )
ROM_END

ROM_START( kabukikl )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "092-p1.p1", 0x100000, 0x100000, 0x28ec9b77 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "092-s1.s1", 0xa3d68ee2 )
	NEO_BIOS_SOUND_128K( "092-m1.m1", 0x91957ef6 )
	ROM_REGION( 0x700000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "092-v1.v1", 0x000000, 0x200000, 0x69e90596 )
	ROM_LOAD( "092-v2.v2", 0x200000, 0x200000, 0x7abdb75d )
	ROM_LOAD( "092-v3.v3", 0x400000, 0x200000, 0xeccc98d3 )
	ROM_LOAD( "092-v4.v4", 0x600000, 0x100000, 0xa7c9c949 )
	ROM_REGION( 0x1000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "092-c1.c1", 0x0000000, 0x400000, 0x2a9fab01 )
	ROM_LOAD_GFX_ODD( "092-c2.c2", 0x0000000, 0x400000, 0x6d2bac02 )
	ROM_LOAD_GFX_EVEN( "092-c3.c3", 0x0800000, 0x400000, 0x5da735d6 )
	ROM_LOAD_GFX_ODD( "092-c4.c4", 0x0800000, 0x400000, 0xde07f997 )
ROM_END

ROM_START( neobombe )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "093-p1.p1", 0x000000, 0x100000, 0xa1a71d0d )
	NEO_SFIX_128K( "093-s1.s1", 0x4b3fa119 )
	NEO_BIOS_SOUND_128K( "093-m1.m1", 0xe81e780b )
	ROM_REGION( 0x600000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "093-v1.v1", 0x000000, 0x400000, 0x02abd4b0 )
	ROM_LOAD( "093-v2.v2", 0x400000, 0x200000, 0xa92b8b3d )
	ROM_REGION( 0x900000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "093-c1.c1", 0x0000000, 0x400000, 0xd1f328f8 )
	ROM_LOAD_GFX_ODD( "093-c2.c2", 0x0000000, 0x400000, 0x82c49540 )
	ROM_LOAD_GFX_EVEN( "093-c3.c3", 0x0800000, 0x080000, 0xe37578c5 )
	ROM_LOAD_GFX_ODD( "093-c4.c4", 0x0800000, 0x080000, 0x59826783 )
ROM_END

ROM_START( gowcaizr )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "094-p1.p1", 0x100000, 0x100000, 0x33019545 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "094-s1.s1", 0x2f8748a2 )
	NEO_BIOS_SOUND_128K( "094-m1.m1", 0x78c851cb )
	ROM_REGION( 0x500000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "094-v1.v1", 0x000000, 0x200000, 0x6c31223c )
	ROM_LOAD( "094-v2.v2", 0x200000, 0x200000, 0x8edb776c )
	ROM_LOAD( "094-v3.v3", 0x400000, 0x100000, 0xc63b9285 )
	ROM_REGION( 0x1000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "094-c1.c1", 0x0000000, 0x200000, 0x042f6af5 )
	ROM_LOAD_GFX_ODD( "094-c2.c2", 0x0000000, 0x200000, 0x0fbcd046 )
	ROM_LOAD_GFX_EVEN( "094-c3.c3", 0x0400000, 0x200000, 0x58bfbaa1 )
	ROM_LOAD_GFX_ODD( "094-c4.c4", 0x0400000, 0x200000, 0x9451ee73 )
	ROM_LOAD_GFX_EVEN( "094-c5.c5", 0x0800000, 0x200000, 0xff9cf48c )
	ROM_LOAD_GFX_ODD( "094-c6.c6", 0x0800000, 0x200000, 0x31bbd918 )
	ROM_LOAD_GFX_EVEN( "094-c7.c7", 0x0c00000, 0x200000, 0x2091ec04 )
	ROM_LOAD_GFX_ODD( "094-c8.c8", 0x0c00000, 0x200000, 0xd80dd241 )
ROM_END

ROM_START( rbff1 )
	ROM_REGION( 0x300000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "095-p1.p1", 0x000000, 0x100000, 0x63b4d8ae )
	ROM_LOAD_WIDE_SWAP( "095-p2.sp2", 0x100000, 0x200000, 0xcc15826e )
	NEO_SFIX_128K( "095-s1.s1", 0xb6bf5e08 )
	NEO_BIOS_SOUND_128K( "095-m1.m1", 0x653492a7 )
	ROM_REGION( 0xc00000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "069-v1.v1", 0x000000, 0x400000, 0x2bdbd4db )
	ROM_LOAD( "069-v2.v2", 0x400000, 0x400000, 0xa698a487 )
	ROM_LOAD( "095-v3.v3", 0x800000, 0x400000, 0x189d1c6c )
	ROM_REGION( 0x1c00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "069-c1.c1", 0x0000000, 0x400000, 0xe302f93c )
	ROM_LOAD_GFX_ODD( "069-c2.c2", 0x0000000, 0x400000, 0x1053a455 )
	ROM_LOAD_GFX_EVEN( "069-c3.c3", 0x0800000, 0x400000, 0x1c0fde2f )
	ROM_LOAD_GFX_ODD( "069-c4.c4", 0x0800000, 0x400000, 0xa25fc3d0 )
	ROM_LOAD_GFX_EVEN( "095-c5.c5", 0x1000000, 0x400000, 0x8b9b65df )
	ROM_LOAD_GFX_ODD( "095-c6.c6", 0x1000000, 0x400000, 0x3e164718 )
	ROM_LOAD_GFX_EVEN( "095-c7.c7", 0x1800000, 0x200000, 0xca605e12 )
	ROM_LOAD_GFX_ODD( "095-c8.c8", 0x1800000, 0x200000, 0x4e6beb6c )
ROM_END

ROM_START( aof3 )
	ROM_REGION( 0x300000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "096-p1.p1", 0x000000, 0x100000, 0x9edb420d )
	ROM_LOAD_WIDE_SWAP( "096-p2.sp2", 0x100000, 0x200000, 0x4d5a2602 )
	NEO_SFIX_128K( "096-s1.s1", 0xcc7fd344 )
	NEO_BIOS_SOUND_128K( "096-m1.m1", 0xcb07b659 )
	ROM_REGION( 0x600000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "096-v1.v1", 0x000000, 0x200000, 0xe2c32074 )
	ROM_LOAD( "096-v2.v2", 0x200000, 0x200000, 0xa290eee7 )
	ROM_LOAD( "096-v3.v3", 0x400000, 0x200000, 0x199d12ea )
	ROM_REGION( 0x1c00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "096-c1.c1", 0x0000000, 0x400000, 0xf17b8d89 )
	ROM_LOAD_GFX_ODD( "096-c2.c2", 0x0000000, 0x400000, 0x3840c508 )
	ROM_LOAD_GFX_EVEN( "096-c3.c3", 0x0800000, 0x400000, 0x55f9ee1e )
	ROM_LOAD_GFX_ODD( "096-c4.c4", 0x0800000, 0x400000, 0x585b7e47 )
	ROM_LOAD_GFX_EVEN( "096-c5.c5", 0x1000000, 0x400000, 0xc75a753c )
	ROM_LOAD_GFX_ODD( "096-c6.c6", 0x1000000, 0x400000, 0x9a9d2f7a )
	ROM_LOAD_GFX_EVEN( "096-c7.c7", 0x1800000, 0x200000, 0x51bd8ab2 )
	ROM_LOAD_GFX_ODD( "096-c8.c8", 0x1800000, 0x200000, 0x9a34f99c )
ROM_END

ROM_START( sonicwi3 )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "097-p1.p1", 0x100000, 0x100000, 0x0547121d )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "097-s1.s1", 0x8dd66743 )
	NEO_BIOS_SOUND_128K( "097-m1.m1", 0xb20e4291 )
	ROM_REGION( 0x600000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "097-v1.v1", 0x000000, 0x400000, 0x6f885152 )
	ROM_LOAD( "097-v2.v2", 0x400000, 0x200000, 0x3359e868 )
	ROM_REGION( 0xc00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "097-c1.c1", 0x0000000, 0x400000, 0x33d0d589 )
	ROM_LOAD_GFX_ODD( "097-c2.c2", 0x0000000, 0x400000, 0x186f8b43 )
	ROM_LOAD_GFX_EVEN( "097-c3.c3", 0x0800000, 0x200000, 0xc339fff5 )
	ROM_LOAD_GFX_ODD( "097-c4.c4", 0x0800000, 0x200000, 0x84a40c6e )
ROM_END

ROM_START( turfmast )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "200-p1.p1", 0x100000, 0x100000, 0x28c83048 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "200-s1.s1", 0x9a5402b2 )
	NEO_BIOS_SOUND_128K( "200-m1.m1", 0x9994ac00 )
	ROM_REGION( 0x800000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "200-v1.v1", 0x000000, 0x200000, 0x00fd48d2 )
	ROM_LOAD( "200-v2.v2", 0x200000, 0x200000, 0x082acb31 )
	ROM_LOAD( "200-v3.v3", 0x400000, 0x200000, 0x7abca053 )
	ROM_LOAD( "200-v4.v4", 0x600000, 0x200000, 0x6c7b4902 )
	ROM_REGION( 0x800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "200-c1.c1", 0x0000000, 0x400000, 0x8e7bf41a )
	ROM_LOAD_GFX_ODD( "200-c2.c2", 0x0000000, 0x400000, 0x5a65a8ce )
ROM_END

ROM_START( mslug )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "201-p1.p1", 0x100000, 0x100000, 0x08d8daa5 )
	ROM_CONTINUE(                    0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )

	NEO_SFIX_128K( "201-s1.s1", 0x2f55958d )

	NEO_BIOS_SOUND_128K( "201-m1.m1", 0xc28b3253 )

	ROM_REGION( 0x800000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "201-v1.v1", 0x000000, 0x400000, 0x23d22ed1 )
	ROM_LOAD( "201-v2.v2", 0x400000, 0x400000, 0x472cf9db )

	NO_DELTAT_REGION

	ROM_REGION( 0x1000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "201-c1.c1", 0x000000, 0x400000, 0x72813676 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "201-c2.c2", 0x000000, 0x400000, 0x96f62574 ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "201-c3.c3", 0x800000, 0x400000, 0x5121456a ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "201-c4.c4", 0x800000, 0x400000, 0xf4ad59a3 ) /* Plane 2,3 */
ROM_END

ROM_START( puzzledp )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "202-p1.p1", 0x000000, 0x080000, 0x2b61415b )
	NEO_SFIX_128K( "202-s1.s1", 0xcd19264f )
	NEO_BIOS_SOUND_128K( "202-m1.m1", 0x9c0291ea )
	ROM_REGION( 0x080000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "202-v1.v1", 0x000000, 0x080000, 0xdebeb8fb )
	ROM_REGION( 0x200000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "202-c1.c1", 0x0000000, 0x100000, 0xcc0095ef )
	ROM_LOAD_GFX_ODD( "202-c2.c2", 0x0000000, 0x100000, 0x42371307 )
ROM_END

ROM_START( moshougi )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "203-p1.p1", 0x000000, 0x100000, 0x7ba70e2d )
	NEO_SFIX_128K( "203-s1.s1", 0xbfdc8309 )
	NEO_BIOS_SOUND_128K( "203-m1.m1", 0xa602c2c2 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "203-v1.v1", 0x000000, 0x200000, 0xbaa2b9a5 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "203-c1.c1", 0x0000000, 0x200000, 0xbba9e8c0 )
	ROM_LOAD_GFX_ODD( "203-c2.c2", 0x0000000, 0x200000, 0x2574be03 )
ROM_END

ROM_START( marukodq )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "206-p1.p1", 0x000000, 0x100000, 0xc33ed21e )
	NEO_SFIX_128K( "206-s1.s1", 0xf0b68780 )
	NEO_BIOS_SOUND_128K( "206-m1.m1", 0x0e22902e )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "206-v1.v1", 0x000000, 0x200000, 0x5385eca8 )
	ROM_LOAD( "206-v2.v2", 0x200000, 0x200000, 0xf8c55404 )
	ROM_REGION( 0xa00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "206-c1.c1", 0x0000000, 0x400000, 0x846e4e8e )
	ROM_LOAD_GFX_ODD( "206-c2.c2", 0x0000000, 0x400000, 0x1cba876d )
	ROM_LOAD_GFX_EVEN( "206-c3.c3", 0x0800000, 0x100000, 0x79aa2b48 )
	ROM_LOAD_GFX_ODD( "206-c4.c4", 0x0800000, 0x100000, 0x55e1314d )
ROM_END

ROM_START( neomrdo )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "207-p1.p1", 0x000000, 0x100000, 0x334ea51e )
	NEO_SFIX_128K( "207-s1.s1", 0x6aebafce )
	NEO_BIOS_SOUND_128K( "207-m1.m1", 0xb5b74a95 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "207-v1.v1", 0x000000, 0x200000, 0x4143c052 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "207-c1.c1", 0x0000000, 0x200000, 0xc7541b9d )
	ROM_LOAD_GFX_ODD( "207-c2.c2", 0x0000000, 0x200000, 0xf57166d2 )
ROM_END

ROM_START( sdodgeb )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "208-p1.p1", 0x100000, 0x100000, 0x127f3d32 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "208-s1.s1", 0x64abd6b3 )
	NEO_BIOS_SOUND_128K( "208-m1.m1", 0x0a5f3325 )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "208-v1.v1", 0x000000, 0x400000, 0xe7899a24 )
	ROM_REGION( 0x0c00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "208-c1.c1", 0x0000000, 0x400000, 0x93d8619b )
	ROM_LOAD_GFX_ODD( "208-c2.c2", 0x0000000, 0x400000, 0x1c737bb6 )
	ROM_LOAD_GFX_EVEN( "208-c3.c3", 0x0800000, 0x200000, 0x14cb1703 )
	ROM_LOAD_GFX_ODD( "208-c4.c4", 0x0800000, 0x200000, 0xc7165f19 )
ROM_END

ROM_START( goalx3 )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "209-p1.p1", 0x100000, 0x100000, 0x2a019a79 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "209-s1.s1", 0xc0eaad86 )
	NEO_BIOS_SOUND_128K( "209-m1.m1", 0xcd758325 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "209-v1.v1", 0x000000, 0x200000, 0xef214212 )
	ROM_REGION( 0xa00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "209-c1.c1", 0x0000000, 0x400000, 0xb49d980e )
	ROM_LOAD_GFX_ODD( "209-c2.c2", 0x0000000, 0x400000, 0x5649b015 )
	ROM_LOAD_GFX_EVEN( "209-c3.c3", 0x0800000, 0x100000, 0x5f91bace )
	ROM_LOAD_GFX_ODD( "209-c4.c4", 0x0800000, 0x100000, 0x1e9f76f2 )
ROM_END

ROM_START( overtop )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "212-p1.p1", 0x100000, 0x100000, 0x16c063a9 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "212-s1.s1", 0x481d3ddc )
	NEO_BIOS_SOUND_128K( "212-m1.m1", 0xfcab6191 )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "212-v1.v1", 0x000000, 0x400000, 0x013d4ef9 )
	ROM_REGION( 0x1400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "212-c1.c1", 0x0000000, 0x400000, 0x50f43087 )
	ROM_LOAD_GFX_ODD( "212-c2.c2", 0x0000000, 0x400000, 0xa5b39807 )
	ROM_LOAD_GFX_EVEN( "212-c3.c3", 0x0800000, 0x400000, 0x9252ea02 )
	ROM_LOAD_GFX_ODD( "212-c4.c4", 0x0800000, 0x400000, 0x5f41a699 )
	ROM_LOAD_GFX_EVEN( "212-c5.c5", 0x1000000, 0x200000, 0xfc858bef )
	ROM_LOAD_GFX_ODD( "212-c6.c6", 0x1000000, 0x200000, 0x0589c15e )
ROM_END

ROM_START( neodrift )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "213-p1.p1", 0x100000, 0x100000, 0xe397d798 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "213-s1.s1", 0xb76b61bc )
	NEO_BIOS_SOUND_128K( "213-m1.m1", 0x200045f1 )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "213-v1.v1", 0x000000, 0x200000, 0xa421c076 )
	ROM_LOAD( "213-v2.v2", 0x200000, 0x200000, 0x233c7dd9 )
	ROM_REGION( 0x800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "213-c1.c1", 0x0000000, 0x400000, 0x3edc8bd3 )
	ROM_LOAD_GFX_ODD( "213-c2.c2", 0x0000000, 0x400000, 0x46ae5f16 )
ROM_END

ROM_START( kof96 )
	ROM_REGION( 0x300000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "214-p1.p1", 0x000000, 0x100000, 0x52755d74 )
	ROM_LOAD_WIDE_SWAP( "214-p2.sp2", 0x100000, 0x200000, 0x002ccb73 )
	NEO_SFIX_128K( "214-s1.s1", 0x1254cbdb )
	NEO_BIOS_SOUND_128K( "214-m1.m1", 0xdabc427c )
	ROM_REGION( 0xa00000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "214-v1.v1", 0x000000, 0x400000, 0x63f7b045 )
	ROM_LOAD( "214-v2.v2", 0x400000, 0x400000, 0x25929059 )
	ROM_LOAD( "214-v3.v3", 0x800000, 0x200000, 0x92a2257d )
	ROM_REGION( 0x2000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "214-c1.c1", 0x0000000, 0x400000, 0x7ecf4aa2 )
	ROM_LOAD_GFX_ODD( "214-c2.c2", 0x0000000, 0x400000, 0x05b54f37 )
	ROM_LOAD_GFX_EVEN( "214-c3.c3", 0x0800000, 0x400000, 0x64989a65 )
	ROM_LOAD_GFX_ODD( "214-c4.c4", 0x0800000, 0x400000, 0xafbea515 )
	ROM_LOAD_GFX_EVEN( "214-c5.c5", 0x1000000, 0x400000, 0x2a3bbd26 )
	ROM_LOAD_GFX_ODD( "214-c6.c6", 0x1000000, 0x400000, 0x44d30dc7 )
	ROM_LOAD_GFX_EVEN( "214-c7.c7", 0x1800000, 0x400000, 0x3687331b )
	ROM_LOAD_GFX_ODD( "214-c8.c8", 0x1800000, 0x400000, 0xfa1461ad )
ROM_END

ROM_START( ssideki4 )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "215-p1.p1", 0x100000, 0x100000, 0x519b4ba3 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "215-s1.s1", 0xf0fe5c36 )
	NEO_BIOS_SOUND_128K( "215-m1.m1", 0xa932081d )
	ROM_REGION( 0x600000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "215-v1.v1", 0x000000, 0x400000, 0x877d1409 )
	ROM_LOAD( "215-v2.v2", 0x400000, 0x200000, 0x1bfa218b )
	ROM_REGION( 0x1400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "215-c1.c1", 0x0000000, 0x400000, 0x8ff444f5 )
	ROM_LOAD_GFX_ODD( "215-c2.c2", 0x0000000, 0x400000, 0x5b155037 )
	ROM_LOAD_GFX_EVEN( "215-c3.c3", 0x0800000, 0x400000, 0x456a073a )
	ROM_LOAD_GFX_ODD( "215-c4.c4", 0x0800000, 0x400000, 0x43c182e1 )
	ROM_LOAD_GFX_EVEN( "215-c5.c5", 0x1000000, 0x200000, 0x0c6f97ec )
	ROM_LOAD_GFX_ODD( "215-c6.c6", 0x1000000, 0x200000, 0x329c5e1b )
ROM_END

ROM_START( kizuna )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "216-p1.p1", 0x100000, 0x100000, 0x75d2b3de )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "216-s1.s1", 0xefdc72d7 )
	NEO_BIOS_SOUND_128K( "216-m1.m1", 0x1b096820 )
	ROM_REGION( 0x800000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "059-v1.v1", 0x000000, 0x200000, 0x530c50fd )
	ROM_LOAD( "216-v2.v2", 0x200000, 0x200000, 0x03667a8d )
	ROM_LOAD( "059-v3.v3", 0x400000, 0x200000, 0x7038c2f9 )
	ROM_LOAD( "216-v4.v4", 0x600000, 0x200000, 0x31b99bd6 )
	ROM_REGION( 0x1c00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "059-c1.c1", 0x0000000, 0x200000, 0x763ba611 )
	ROM_LOAD_GFX_ODD( "059-c2.c2", 0x0000000, 0x200000, 0xe05e8ca6 )
	ROM_LOAD_GFX_EVEN( "216-c3.c3", 0x0800000, 0x400000, 0x665c9f16 )
	ROM_LOAD_GFX_ODD( "216-c4.c4", 0x0800000, 0x400000, 0x7f5d03db )
	ROM_LOAD_GFX_EVEN( "059-c5.c5", 0x1000000, 0x200000, 0x59013f9e )
	ROM_LOAD_GFX_ODD( "059-c6.c6", 0x1000000, 0x200000, 0x1c8d5def )
	ROM_LOAD_GFX_EVEN( "059-c7.c7", 0x1800000, 0x200000, 0xc88f7035 )
	ROM_LOAD_GFX_ODD( "059-c8.c8", 0x1800000, 0x200000, 0x484ce3ba )
ROM_END

ROM_START( ninjamas )
	ROM_REGION( 0x300000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "217-p1.p1", 0x000000, 0x100000, 0x3e97ed69 )
	ROM_LOAD_WIDE_SWAP( "217-p2.sp2", 0x100000, 0x200000, 0x191fca88 )
	NEO_SFIX_128K( "217-s1.s1", 0x8ff782f0 )
	NEO_BIOS_SOUND_128K( "217-m1.m1", 0xd00fb2af )
	ROM_REGION( 0x600000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "217-v1.v1", 0x000000, 0x400000, 0x1c34e013 )
	ROM_LOAD( "217-v2.v2", 0x400000, 0x200000, 0x22f1c681 )
	ROM_REGION( 0x2000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "217-c1.c1", 0x0000000, 0x400000, 0x5fe97bc4 )
	ROM_LOAD_GFX_ODD( "217-c2.c2", 0x0000000, 0x400000, 0x886e0d66 )
	ROM_LOAD_GFX_EVEN( "217-c3.c3", 0x0800000, 0x400000, 0x59e8525f )
	ROM_LOAD_GFX_ODD( "217-c4.c4", 0x0800000, 0x400000, 0x8521add2 )
	ROM_LOAD_GFX_EVEN( "217-c5.c5", 0x1000000, 0x400000, 0xfb1896e5 )
	ROM_LOAD_GFX_ODD( "217-c6.c6", 0x1000000, 0x400000, 0x1c98c54b )
	ROM_LOAD_GFX_EVEN( "217-c7.c7", 0x1800000, 0x400000, 0x8b0ede2e )
	ROM_LOAD_GFX_ODD( "217-c8.c8", 0x1800000, 0x400000, 0xa085bb61 )
ROM_END

ROM_START( ragnagrd )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "218-p1.p1", 0x100000, 0x100000, 0xca372303 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "218-s1.s1", 0x7d402f9a )
	NEO_BIOS_SOUND_128K( "218-m1.m1", 0x17028bcf )
	ROM_REGION( 0x800000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "218-v1.v1", 0x000000, 0x400000, 0x61eee7f4 )
	ROM_LOAD( "218-v2.v2", 0x400000, 0x400000, 0x6104e20b )
	ROM_REGION( 0x2000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "218-c1.c1", 0x0000000, 0x400000, 0xc31500a4 )
	ROM_LOAD_GFX_ODD( "218-c2.c2", 0x0000000, 0x400000, 0x98aba1f9 )
	ROM_LOAD_GFX_EVEN( "218-c3.c3", 0x0800000, 0x400000, 0x833c163a )
	ROM_LOAD_GFX_ODD( "218-c4.c4", 0x0800000, 0x400000, 0xc1a30f69 )
	ROM_LOAD_GFX_EVEN( "218-c5.c5", 0x1000000, 0x400000, 0x6b6de0ff )
	ROM_LOAD_GFX_ODD( "218-c6.c6", 0x1000000, 0x400000, 0x94beefcf )
	ROM_LOAD_GFX_EVEN( "218-c7.c7", 0x1800000, 0x400000, 0xde6f9b28 )
	ROM_LOAD_GFX_ODD( "218-c8.c8", 0x1800000, 0x400000, 0xd9b311f6 )
ROM_END

ROM_START( pgoal )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "219-p1.p1", 0x100000, 0x100000, 0x6af0e574 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "219-s1.s1", 0x002f3c88 )
	NEO_BIOS_SOUND_128K( "219-m1.m1", 0x958efdc8 )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "219-v1.v1", 0x000000, 0x400000, 0xd0ae33d9 )
	ROM_REGION( 0xc00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "219-c1.c1", 0x0000000, 0x400000, 0x67fec4dc )
	ROM_LOAD_GFX_ODD( "219-c2.c2", 0x0000000, 0x400000, 0x86ed01f2 )
	ROM_LOAD_GFX_EVEN( "219-c3.c3", 0x0800000, 0x200000, 0x5fdad0a5 )
	ROM_LOAD_GFX_ODD( "219-c4.c4", 0x0800000, 0x200000, 0xf57b4a1c )
ROM_END

ROM_START( magdrop2 )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "221-p1.p1", 0x000000, 0x80000, 0x7be82353 )
	NEO_SFIX_128K( "221-s1.s1", 0x2a4063a3 )
	NEO_BIOS_SOUND_128K( "221-m1.m1", 0xbddae628 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "221-v1.v1", 0x000000, 0x200000, 0x7e5e53e4 )
	ROM_REGION( 0x800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "221-c1.c1", 0x0000000, 0x400000, 0x1f862a14 )
	ROM_LOAD_GFX_ODD( "221-c2.c2", 0x0000000, 0x400000, 0x14b90536 )
ROM_END

ROM_START( samsho4 )
	ROM_REGION( 0x500000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "222-p1.p1", 0x000000, 0x100000, 0x1a5cb56d )
	ROM_LOAD_WIDE_SWAP( "222-p2.sp2", 0x100000, 0x400000, 0xb023cd8b )
	NEO_SFIX_128K( "222-s1.s1", 0x8d3d3bf9 )
	NEO_BIOS_SOUND_128K( "222-m1.m1", 0x7615bc1b )
	ROM_REGION( 0xa00000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "222-v1.v1", 0x000000, 0x400000, 0x7d6ba95f )
	ROM_LOAD( "222-v2.v2", 0x400000, 0x400000, 0x6c33bb5d )
	ROM_LOAD( "222-v3.v3", 0x800000, 0x200000, 0x831ea8c0 )
	ROM_REGION( 0x2000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "222-c1.c1", 0x0000000, 0x400000, 0x68f2ed95 )
	ROM_LOAD_GFX_ODD( "222-c2.c2", 0x0000000, 0x400000, 0xa6e9aff0 )
	ROM_LOAD_GFX_EVEN( "222-c3.c3", 0x0800000, 0x400000, 0xc91b40f4 )
	ROM_LOAD_GFX_ODD( "222-c4.c4", 0x0800000, 0x400000, 0x359510a4 )
	ROM_LOAD_GFX_EVEN( "222-c5.c5", 0x1000000, 0x400000, 0x9cfbb22d )
	ROM_LOAD_GFX_ODD( "222-c6.c6", 0x1000000, 0x400000, 0x685efc32 )
	ROM_LOAD_GFX_EVEN( "222-c7.c7", 0x1800000, 0x400000, 0xd0f86f0d )
	ROM_LOAD_GFX_ODD( "222-c8.c8", 0x1800000, 0x400000, 0xadfc50e3 )
ROM_END

ROM_START( rbffspec )
	ROM_REGION( 0x500000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "223-p1.p1", 0x000000, 0x100000, 0xf84a2d1d )
	ROM_LOAD_WIDE_SWAP( "223-p2.sp2", 0x100000, 0x400000, 0xaddd8f08 )
	NEO_SFIX_128K( "223-s1.s1", 0x7ecd6e8c )
	NEO_BIOS_SOUND_128K( "223-m1.m1", 0x3fee46bf )
	ROM_REGION( 0xc00000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "223-v1.v1", 0x000000, 0x400000, 0x76673869 )
	ROM_LOAD( "223-v2.v2", 0x400000, 0x400000, 0x7a275acd )
	ROM_LOAD( "223-v3.v3", 0x800000, 0x400000, 0x5a797fd2 )
	ROM_REGION( 0x2000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "223-c1.c1", 0x0000000, 0x400000, 0xebab05e2 )
	ROM_LOAD_GFX_ODD( "223-c2.c2", 0x0000000, 0x400000, 0x641868c3 )
	ROM_LOAD_GFX_EVEN( "223-c3.c3", 0x0800000, 0x400000, 0xca00191f )
	ROM_LOAD_GFX_ODD( "223-c4.c4", 0x0800000, 0x400000, 0x1f23d860 )
	ROM_LOAD_GFX_EVEN( "223-c5.c5", 0x1000000, 0x400000, 0x321e362c )
	ROM_LOAD_GFX_ODD( "223-c6.c6", 0x1000000, 0x400000, 0xd8fcef90 )
	ROM_LOAD_GFX_EVEN( "223-c7.c7", 0x1800000, 0x400000, 0xbc80dd2d )
	ROM_LOAD_GFX_ODD( "223-c8.c8", 0x1800000, 0x400000, 0x5ad62102 )
ROM_END

ROM_START( twinspri )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "224-p1.p1", 0x100000, 0x100000, 0x7697e445 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "224-s1.s1", 0xeeed5758 )
	NEO_BIOS_SOUND_128K( "224-m1.m1", 0x364d6f96 )
	ROM_REGION( 0x600000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "224-v1.v1", 0x000000, 0x400000, 0xff57f088 )
	ROM_LOAD( "224-v2.v2", 0x400000, 0x200000, 0x7ad26599 )
	ROM_REGION( 0xa00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "224-c1.c1", 0x0000000, 0x400000, 0xf7da64ab )
	ROM_LOAD_GFX_ODD( "224-c2.c2", 0x0000000, 0x400000, 0x4c09bbfb )
	ROM_LOAD_GFX_EVEN( "224-c3.c3", 0x0800000, 0x100000, 0xc59e4129 )
	ROM_LOAD_GFX_ODD( "224-c4.c4", 0x0800000, 0x100000, 0xb5532e53 )
ROM_END

ROM_START( wakuwak7 )
	ROM_REGION( 0x300000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "225-p1.p1", 0x000000, 0x100000, 0xb14da766 )
	ROM_LOAD_WIDE_SWAP( "225-p2.sp2", 0x100000, 0x200000, 0xfe190665 )
	NEO_SFIX_128K( "225-s1.s1", 0x71c4b4b5 )
	NEO_BIOS_SOUND_128K( "225-m1.m1", 0x0634bba6 )
	ROM_REGION( 0x800000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "225-v1.v1", 0x000000, 0x400000, 0x6195c6b4 )
	ROM_LOAD( "225-v2.v2", 0x400000, 0x400000, 0x6159c5fe )
	ROM_REGION( 0x1800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "225-c1.c1", 0x0000000, 0x400000, 0xee4fea54 )
	ROM_LOAD_GFX_ODD( "225-c2.c2", 0x0000000, 0x400000, 0x0c549e2d )
	ROM_LOAD_GFX_EVEN( "225-c3.c3", 0x0800000, 0x400000, 0xaf0897c0 )
	ROM_LOAD_GFX_ODD( "225-c4.c4", 0x0800000, 0x400000, 0x4c66527a )
	ROM_LOAD_GFX_EVEN( "225-c5.c5", 0x1000000, 0x400000, 0x8ecea2b5 )
	ROM_LOAD_GFX_ODD( "225-c6.c6", 0x1000000, 0x400000, 0x0eb11a6d )
ROM_END

ROM_START( stakwin2 )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "227-p1.p1", 0x100000, 0x100000, 0xdaf101d2 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "227-s1.s1", 0x2a8c4462 )
	NEO_BIOS_SOUND_128K( "227-m1.m1", 0xc8e5e0f9 )
	ROM_REGION( 0x800000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "227-v1.v1", 0x000000, 0x400000, 0xb8f24181 )
	ROM_LOAD( "227-v2.v2", 0x400000, 0x400000, 0xee39e260 )
	ROM_REGION( 0xc00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "227-c1.c1", 0x0000000, 0x400000, 0x7d6c2af4 )
	ROM_LOAD_GFX_ODD( "227-c2.c2", 0x0000000, 0x400000, 0x7e402d39 )
	ROM_LOAD_GFX_EVEN( "227-c3.c3", 0x0800000, 0x200000, 0x93dfd660 )
	ROM_LOAD_GFX_ODD( "227-c4.c4", 0x0800000, 0x200000, 0x7efea43a )
ROM_END

ROM_START( breakers )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "230-p1.p1", 0x100000, 0x100000, 0xed24a6e6 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "230-s1.s1", 0x076fb64c )
	NEO_BIOS_SOUND_128K( "230-m1.m1", 0x3951a1c1 )
	ROM_REGION( 0x800000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "230-v1.v1", 0x000000, 0x400000, 0x7f9ed279 )
	ROM_LOAD( "230-v2.v2", 0x400000, 0x400000, 0x1d43e420 )
	ROM_REGION( 0x1000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "230-c1.c1", 0x0000000, 0x400000, 0x68d4ae76 )
	ROM_LOAD_GFX_ODD( "230-c2.c2", 0x0000000, 0x400000, 0xfdee05cd )
	ROM_LOAD_GFX_EVEN( "230-c3.c3", 0x0800000, 0x400000, 0x645077f3 )
	ROM_LOAD_GFX_ODD( "230-c4.c4", 0x0800000, 0x400000, 0x63aeb74c )
ROM_END

ROM_START( miexchng )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "231-p1.p1", 0x000000, 0x80000, 0x61be1810 )
	NEO_SFIX_128K( "231-s1.s1", 0xfe0c0c53 )
	NEO_BIOS_SOUND_128K( "231-m1.m1", 0xde41301b )
	ROM_REGION( 0x400000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "231-v1.v1", 0x000000, 0x400000, 0x113fb898 )
	ROM_REGION( 0x600000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "231-c1.c1", 0x0000000, 0x200000, 0x6c403ba3 )
	ROM_LOAD_GFX_ODD( "231-c2.c2", 0x0000000, 0x200000, 0x554bcd9b )
	ROM_LOAD_GFX_EVEN( "231-c3.c3", 0x0400000, 0x100000, 0x4f6f7a63 )
	ROM_LOAD_GFX_ODD( "231-c4.c4", 0x0400000, 0x100000, 0x2e35e71b )
ROM_END

ROM_START( kof97 )
	ROM_REGION( 0x500000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "232-p1.p1", 0x000000, 0x100000, 0x7db81ad9 )
	ROM_LOAD_WIDE_SWAP( "232-p2.sp2", 0x100000, 0x400000, 0x158b23f6 )
	NEO_SFIX_128K( "232-s1.s1", 0x8514ecf5 )
	NEO_BIOS_SOUND_128K( "232-m1.m1", 0x45348747 )
	ROM_REGION( 0xc00000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "232-v1.v1", 0x000000, 0x400000, 0x22a2b5b5 )
	ROM_LOAD( "232-v2.v2", 0x400000, 0x400000, 0x2304e744 )
	ROM_LOAD( "232-v3.v3", 0x800000, 0x400000, 0x759eb954 )
	ROM_REGION( 0x2800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "232-c1.c1", 0x0000000, 0x800000, 0x5f8bf0a1 )
	ROM_LOAD_GFX_ODD( "232-c2.c2", 0x0000000, 0x800000, 0xe4d45c81 )
	ROM_LOAD_GFX_EVEN( "232-c3.c3", 0x1000000, 0x800000, 0x581d6618 )
	ROM_LOAD_GFX_ODD( "232-c4.c4", 0x1000000, 0x800000, 0x49bb1e68 )
	ROM_LOAD_GFX_EVEN( "232-c5.c5", 0x2000000, 0x400000, 0x34fc4e51 )
	ROM_LOAD_GFX_ODD( "232-c6.c6", 0x2000000, 0x400000, 0x4ff4d47b )
ROM_END

ROM_START( magdrop3 )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "233-p1.p1", 0x000000, 0x100000, 0x931e17fa )
	NEO_SFIX_128K( "233-s1.s1", 0x7399e68a )
	NEO_BIOS_SOUND_128K( "233-m1.m1", 0x5beaf34e )
	ROM_REGION( 0x480000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "233-v1.v1", 0x000000, 0x400000, 0x58839298 )
	ROM_LOAD( "233-v2.v2", 0x400000, 0x080000, 0xd5e30df4 )
	ROM_REGION( 0x1000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "233-c1.c1", 0x0000000, 0x400000, 0x65e3f4c4 )
	ROM_LOAD_GFX_ODD( "233-c2.c2", 0x0000000, 0x400000, 0x35dea6c9 )
	ROM_LOAD_GFX_EVEN( "233-c3.c3", 0x0800000, 0x400000, 0x0ba2c502 )
	ROM_LOAD_GFX_ODD( "233-c4.c4", 0x0800000, 0x400000, 0x70dbbd6d )
ROM_END

ROM_START( lastblad )
	ROM_REGION( 0x500000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "234-p1.p1", 0x000000, 0x100000, 0xe123a5a3 )
	ROM_LOAD_WIDE_SWAP( "234-p2.sp2", 0x100000, 0x400000, 0x0fdc289e )
	NEO_SFIX_128K( "234-s1.s1", 0x95561412 )
	NEO_BIOS_SOUND_128K( "234-m1.m1", 0x087628ea )
	ROM_REGION( 0x1000000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "234-v1.v1", 0x000000, 0x400000, 0xed66b76f )
	ROM_LOAD( "234-v2.v2", 0x400000, 0x400000, 0xa0e7f6e2 )
	ROM_LOAD( "234-v3.v3", 0x800000, 0x400000, 0xa506e1e2 )
	ROM_LOAD( "234-v4.v4", 0xc00000, 0x400000, 0x0e34157f )
	ROM_REGION( 0x2800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "234-c1.c1", 0x0000000, 0x800000, 0x9f7e2bd3 )
	ROM_LOAD_GFX_ODD( "234-c2.c2", 0x0000000, 0x800000, 0x80623d3c )
	ROM_LOAD_GFX_EVEN( "234-c3.c3", 0x1000000, 0x800000, 0x91ab1a30 )
	ROM_LOAD_GFX_ODD( "234-c4.c4", 0x1000000, 0x800000, 0x3d60b037 )
	ROM_LOAD_GFX_EVEN( "234-c5.c5", 0x2000000, 0x400000, 0x1ba80cee )
	ROM_LOAD_GFX_ODD( "234-c6.c6", 0x2000000, 0x400000, 0xbeafd091 )
ROM_END

ROM_START( puzzldpr )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "235-p1.p1", 0x000000, 0x080000, 0xafed5de2 )
	NEO_SFIX_128K( "235-s1.s1", 0x3b13a22f )
	NEO_BIOS_SOUND_128K( "202-m1.m1", 0x9c0291ea )
	ROM_REGION( 0x080000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "202-v1.v1", 0x000000, 0x080000, 0xdebeb8fb )
	ROM_REGION( 0x200000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "202-c1.c1", 0x0000000, 0x100000, 0xcc0095ef )
	ROM_LOAD_GFX_ODD( "202-c2.c2", 0x0000000, 0x100000, 0x42371307 )
ROM_END

ROM_START( irrmaze )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "236-p1.p1", 0x100000, 0x100000, 0x4c2ff660 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )

	NEO_SFIX_128K( "236-s1.s1", 0x5d1ca640 )

	ROM_REGION( 0x20000, REGION_USER1 )
	/* special BIOS with trackball support */
	ROM_LOAD_WIDE_SWAP( "236-bios.sp1", 0x00000, 0x020000, 0x853e6b96 )
	ROM_REGION( 0x40000, REGION_CPU2 )
	ROM_LOAD( "236-m1.m1",  0x00000, 0x20000, 0x880a1abd )

	ROM_REGION( 0x300000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "236-v1.v1", 0x000000, 0x200000, 0x5f89c3b4 )
	ROM_LOAD( "236-v2.v2", 0x200000, 0x100000, 0x72e3add7 )

	ROM_REGION( 0x0800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "236-c1.c1", 0x000000, 0x400000, 0xc1d47902 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "236-c2.c2", 0x000000, 0x400000, 0xe15f972e ) /* Plane 2,3 */
ROM_END

ROM_START( popbounc )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "237-p1.p1", 0x000000, 0x100000, 0xbe96e44f )
	NEO_SFIX_128K( "237-s1.s1", 0xb61cf595 )
	NEO_BIOS_SOUND_128K( "237-m1.m1", 0xd4c946dd )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "237-v1.v1", 0x000000, 0x200000, 0xedcb1beb )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "237-c1.c1", 0x0000000, 0x200000, 0xeda42d66 )
	ROM_LOAD_GFX_ODD( "237-c2.c2", 0x0000000, 0x200000, 0x5e633c65 )
ROM_END

ROM_START( shocktro )
	ROM_REGION( 0x500000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "238-pg1.p1", 0x000000, 0x100000, 0xefedf8dc )
	ROM_LOAD_WIDE_SWAP( "238-p2.sp2", 0x100000, 0x400000, 0x5b4a09c5 )
	NEO_SFIX_128K( "238-s1.s1", 0x1f95cedb )
	NEO_BIOS_SOUND_128K( "238-m1.m1", 0x075b9518 )
	ROM_REGION( 0x600000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "238-v1.v1", 0x000000, 0x400000, 0x260c0bef )
	ROM_LOAD( "238-v2.v2", 0x400000, 0x200000, 0x4ad7d59e )
	ROM_REGION( 0x2000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "238-c1.c1", 0x0000000, 0x400000, 0x90c6a181 )
	ROM_LOAD_GFX_ODD( "238-c2.c2", 0x0000000, 0x400000, 0x888720f0 )
	ROM_LOAD_GFX_EVEN( "238-c3.c3", 0x0800000, 0x400000, 0x2c393aa3 )
	ROM_LOAD_GFX_ODD( "238-c4.c4", 0x0800000, 0x400000, 0xb9e909eb )
	ROM_LOAD_GFX_EVEN( "238-c5.c5", 0x1000000, 0x400000, 0xc22c68eb )
	ROM_LOAD_GFX_ODD( "238-c6.c6", 0x1000000, 0x400000, 0x119323cd )
	ROM_LOAD_GFX_EVEN( "238-c7.c7", 0x1800000, 0x400000, 0xa72ce7ed )
	ROM_LOAD_GFX_ODD( "238-c8.c8", 0x1800000, 0x400000, 0x1c7c2efb )
ROM_END

ROM_START( blazstar )
	ROM_REGION( 0x300000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "239-p1.p1", 0x000000, 0x100000, 0x183682f8 )
	ROM_LOAD_WIDE_SWAP( "239-p2.sp2", 0x100000, 0x200000, 0x9a9f4154 )
	NEO_SFIX_128K( "239-s1.s1", 0xd56cb498 )
	NEO_BIOS_SOUND_128K( "239-m1.m1", 0xd31a3aea )
	ROM_REGION( 0x800000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "239-v1.v1", 0x000000, 0x400000, 0x1b8d5bf7 )
	ROM_LOAD( "239-v2.v2", 0x400000, 0x400000, 0x74cf0a70 )
	ROM_REGION( 0x2000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "239-c1.c1", 0x0000000, 0x400000, 0x84f6d584 )
	ROM_LOAD_GFX_ODD( "239-c2.c2", 0x0000000, 0x400000, 0x05a0cb22 )
	ROM_LOAD_GFX_EVEN( "239-c3.c3", 0x0800000, 0x400000, 0x5fb69c9e )
	ROM_LOAD_GFX_ODD( "239-c4.c4", 0x0800000, 0x400000, 0x0be028c4 )
	ROM_LOAD_GFX_EVEN( "239-c5.c5", 0x1000000, 0x400000, 0x74bae5f8 )
	ROM_LOAD_GFX_ODD( "239-c6.c6", 0x1000000, 0x400000, 0x4e0700d2 )
	ROM_LOAD_GFX_EVEN( "239-c7.c7", 0x1800000, 0x400000, 0x010ff4fd )
	ROM_LOAD_GFX_ODD( "239-c8.c8", 0x1800000, 0x400000, 0xdb60460e )
ROM_END

ROM_START( rbff2 )
	ROM_REGION( 0x500000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "240-p1.p1", 0x000000, 0x100000, 0x80e41205 )
	ROM_LOAD_WIDE_SWAP( "240-p2.sp2", 0x100000, 0x400000, 0x960aa88d )
	NEO_SFIX_128K( "240-s1.s1", 0xda3b40de )
	NEO_BIOS_SOUND_256K( "240-m1.m1", 0xed482791 )
	ROM_REGION( 0x0e00000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "240-v1.v1", 0x000000, 0x400000, 0xf796265a )
	ROM_LOAD( "240-v2.v2", 0x400000, 0x400000, 0x2cb3f3bb )
	ROM_LOAD( "240-v3.v3", 0x800000, 0x400000, 0x8fe1367a )
	ROM_LOAD( "240-v4.v4", 0xc00000, 0x200000, 0x996704d8 )
	ROM_REGION( 0x3000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "240-c1.c1", 0x0000000, 0x800000, 0xeffac504 )
	ROM_LOAD_GFX_ODD( "240-c2.c2", 0x0000000, 0x800000, 0xed182d44 )
	ROM_LOAD_GFX_EVEN( "240-c3.c3", 0x1000000, 0x800000, 0x22e0330a )
	ROM_LOAD_GFX_ODD( "240-c4.c4", 0x1000000, 0x800000, 0xc19a07eb )
	ROM_LOAD_GFX_EVEN( "240-c5.c5", 0x2000000, 0x800000, 0x244dff5a )
	ROM_LOAD_GFX_ODD( "240-c6.c6", 0x2000000, 0x800000, 0x4609e507 )
ROM_END

ROM_START( mslug2 )
	ROM_REGION( 0x300000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "241-p1.p1", 0x000000, 0x100000, 0x2a53c5da )
	ROM_LOAD_WIDE_SWAP( "241-p2.sp2", 0x100000, 0x200000, 0x38883f44 )
	NEO_SFIX_128K( "241-s1.s1", 0xf3d32f0f )
	NEO_BIOS_SOUND_128K( "241-m1.m1", 0x94520ebd )
	ROM_REGION( 0x800000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "241-v1.v1", 0x000000, 0x400000, 0x99ec20e8 )
	ROM_LOAD( "241-v2.v2", 0x400000, 0x400000, 0xecb16799 )
	ROM_REGION( 0x2000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "241-c1.c1", 0x0000000, 0x800000, 0x394b5e0d )
	ROM_LOAD_GFX_ODD( "241-c2.c2", 0x0000000, 0x800000, 0xe5806221 )
	ROM_LOAD_GFX_EVEN( "241-c3.c3", 0x1000000, 0x800000, 0x9f6bfa6f )
	ROM_LOAD_GFX_ODD( "241-c4.c4", 0x1000000, 0x800000, 0x7d3e306f )
ROM_END

ROM_START( kof98 )
	ROM_REGION( 0x600000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "242-p1.p1", 0x000000, 0x200000, 0x8893df89 )
	ROM_LOAD_WIDE_SWAP( "242-p2.sp2", 0x200000, 0x400000, 0x980aba4c )
	NEO_SFIX_128K( "242-s1.s1", 0x7f7b4805 )
	NEO_BIOS_SOUND_256K( "242-m1.m1", 0x4ef7016b )
	ROM_REGION( 0x1000000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "242-v1.v1", 0x000000, 0x400000, 0xb9ea8051 )
	ROM_LOAD( "242-v2.v2", 0x400000, 0x400000, 0xcc11106e )
	ROM_LOAD( "242-v3.v3", 0x800000, 0x400000, 0x044ea4e1 )
	ROM_LOAD( "242-v4.v4", 0xc00000, 0x400000, 0x7985ea30 )
	ROM_REGION( 0x4000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "242-c1.c1", 0x0000000, 0x800000, 0xe564ecd6 )
	ROM_LOAD_GFX_ODD( "242-c2.c2", 0x0000000, 0x800000, 0xbd959b60 )
	ROM_LOAD_GFX_EVEN( "242-c3.c3", 0x1000000, 0x800000, 0x22127b4f )
	ROM_LOAD_GFX_ODD( "242-c4.c4", 0x1000000, 0x800000, 0x0b4fa044 )
	ROM_LOAD_GFX_EVEN( "242-c5.c5", 0x2000000, 0x800000, 0x9d10bed3 )
	ROM_LOAD_GFX_ODD( "242-c6.c6", 0x2000000, 0x800000, 0xda07b6a2 )
	ROM_LOAD_GFX_EVEN( "242-c7.c7", 0x3000000, 0x800000, 0xf6d7a38a )
	ROM_LOAD_GFX_ODD( "242-c8.c8", 0x3000000, 0x800000, 0xc823e045 )
ROM_END

ROM_START( lastbld2 )
	ROM_REGION( 0x500000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "243-pg1.p1", 0x000000, 0x100000, 0xaf1e6554 )
	ROM_LOAD_WIDE_SWAP( "243-pg2.sp2", 0x100000, 0x400000, 0xadd4a30b )
	NEO_SFIX_128K( "243-s1.s1", 0xc9cd2298 )
	NEO_BIOS_SOUND_128K( "243-m1.m1", 0xacf12d10 )
	ROM_REGION( 0x1000000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "243-v1.v1", 0x000000, 0x400000, 0xf7ee6fbb )
	ROM_LOAD( "243-v2.v2", 0x400000, 0x400000, 0xaa9e4df6 )
	ROM_LOAD( "243-v3.v3", 0x800000, 0x400000, 0x4ac750b2 )
	ROM_LOAD( "243-v4.v4", 0xc00000, 0x400000, 0xf5c64ba6 )
	ROM_REGION( 0x3000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "243-c1.c1", 0x0000000, 0x800000, 0x5839444d )
	ROM_LOAD_GFX_ODD( "243-c2.c2", 0x0000000, 0x800000, 0xdd087428 )
	ROM_LOAD_GFX_EVEN( "243-c3.c3", 0x1000000, 0x800000, 0x6054cbe0 )
	ROM_LOAD_GFX_ODD( "243-c4.c4", 0x1000000, 0x800000, 0x8bd2a9d2 )
	ROM_LOAD_GFX_EVEN( "243-c5.c5", 0x2000000, 0x800000, 0x6a503dcf )
	ROM_LOAD_GFX_ODD( "243-c6.c6", 0x2000000, 0x800000, 0xec9c36d0 )
ROM_END

ROM_START( neocup98 )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "244-p1.p1", 0x100000, 0x100000, 0xf8fdb7a5 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "244-s1.s1", 0x9bddb697 )
	NEO_BIOS_SOUND_128K( "244-m1.m1", 0xa701b276 )
	ROM_REGION( 0x600000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "244-v1.v1", 0x000000, 0x400000, 0x79def46d )
	ROM_LOAD( "244-v2.v2", 0x400000, 0x200000, 0xb231902f )
	ROM_REGION( 0x1000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "244-c1.c1", 0x0000000, 0x800000, 0xc7a62b23 )
	ROM_LOAD_GFX_ODD( "244-c2.c2", 0x0000000, 0x800000, 0x33aa0f35 )
ROM_END

ROM_START( breakrev )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "245-p1.p1", 0x100000, 0x100000, 0xc828876d )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "245-s1.s1", 0xe7660a5d )
	NEO_BIOS_SOUND_128K( "245-m1.m1", 0x00f31c66 )
	ROM_REGION( 0x800000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "245-v1.v1", 0x000000, 0x400000, 0xe255446c )
	ROM_LOAD( "245-v2.v2", 0x400000, 0x400000, 0x9068198a )
	ROM_REGION( 0x1800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "245-c1.c1", 0x0000000, 0x400000, 0x68d4ae76 )
	ROM_LOAD_GFX_ODD( "245-c2.c2", 0x0000000, 0x400000, 0xfdee05cd )
	ROM_LOAD_GFX_EVEN( "245-c3.c3", 0x0800000, 0x400000, 0x645077f3 )
	ROM_LOAD_GFX_ODD( "245-c4.c4", 0x0800000, 0x400000, 0x63aeb74c )
	ROM_LOAD_GFX_EVEN( "245-c5.c5", 0x1000000, 0x400000, 0xb5f40e7f )
	ROM_LOAD_GFX_ODD( "245-c6.c6", 0x1000000, 0x400000, 0xd0337328 )
ROM_END

ROM_START( shocktr2 )
	ROM_REGION( 0x500000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "246-p1.p1", 0x000000, 0x100000, 0x6d4b7781 )
	ROM_LOAD_WIDE_SWAP( "246-p2.sp2", 0x100000, 0x400000, 0x72ea04c3 )
	NEO_SFIX_128K( "246-s1.s1", 0x2a360637 )
	NEO_BIOS_SOUND_128K( "246-m1.m1", 0xd0604ad1 )
	ROM_REGION( 0x1000000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "246-v1.v1", 0x000000, 0x400000, 0x16986fc6 )
	ROM_LOAD( "246-v2.v2", 0x400000, 0x400000, 0xada41e83 )
	ROM_LOAD( "246-v3.v3", 0x800000, 0x200000, 0xa05ba5db )
	ROM_REGION( 0x3000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "246-c1.c1", 0x0000000, 0x800000, 0x47ac9ec5 )
	ROM_LOAD_GFX_ODD( "246-c2.c2", 0x0000000, 0x800000, 0x7bcab64f )
	ROM_LOAD_GFX_EVEN( "246-c3.c3", 0x1000000, 0x800000, 0xdb2f73e8 )
	ROM_LOAD_GFX_ODD( "246-c4.c4", 0x1000000, 0x800000, 0x5503854e )
	ROM_LOAD_GFX_EVEN( "246-c5.c5", 0x2000000, 0x800000, 0x055b3701 )
	ROM_LOAD_GFX_ODD( "246-c6.c6", 0x2000000, 0x800000, 0x7e2caae1 )
ROM_END

ROM_START( flipshot )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "247-p1.p1", 0x000000, 0x100000, 0x95779094 )
	NEO_SFIX_128K( "247-s1.s1", 0x6300185c )
	NEO_BIOS_SOUND_128K( "247-m1.m1", 0xa9fe0144 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "247-v1.v1", 0x000000, 0x200000, 0x42ec743d )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "247-c1.c1", 0x0000000, 0x200000, 0xc9eedcb2 )
	ROM_LOAD_GFX_ODD( "247-c2.c2", 0x0000000, 0x200000, 0x7d6d6e87 )
ROM_END

ROM_START( pbobbl2n )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "248-p1.p1", 0x000000, 0x100000, 0x9d6c0754 )
	NEO_SFIX_128K( "248-s1.s1", 0x0a3fee41 )
	NEO_BIOS_SOUND_128K( "248-m1.m1", 0x883097a9 )
	ROM_REGION( 0x800000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "248-v1.v1", 0x000000, 0x400000, 0x57fde1fa )
	ROM_LOAD( "248-v2.v2", 0x400000, 0x400000, 0x4b966ef3 )
	ROM_REGION( 0xa00000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "248-c1.c1", 0x0000000, 0x400000, 0xd9115327 )
	ROM_LOAD_GFX_ODD( "248-c2.c2", 0x0000000, 0x400000, 0x77f9fdac )
	ROM_LOAD_GFX_EVEN( "248-c3.c3", 0x0800000, 0x100000, 0x8890bf7c )
	ROM_LOAD_GFX_ODD( "248-c4.c4", 0x0800000, 0x100000, 0x8efead3f )
ROM_END

ROM_START( ctomaday )
	ROM_REGION( 0x200000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "249-p1.p1", 0x100000, 0x100000, 0xc9386118 )
	ROM_CONTINUE( 0x000000, 0x100000 | ROMFLAG_WIDE | ROMFLAG_SWAP )
	NEO_SFIX_128K( "249-s1.s1", 0xdc9eb372 )
	NEO_BIOS_SOUND_128K( "249-m1.m1", 0x80328a47 )
	ROM_REGION( 0x500000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "249-v1.v1", 0x000000, 0x400000, 0xde7c8f27 )
	ROM_LOAD( "249-v2.v2", 0x400000, 0x100000, 0xc8e40119 )
	ROM_REGION( 0x800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "249-c1.c1", 0x0000000, 0x400000, 0x041fb8ee )
	ROM_LOAD_GFX_ODD( "249-c2.c2", 0x0000000, 0x400000, 0x74f3cdf4 )
ROM_END

ROM_START( mslugx )
	ROM_REGION( 0x500000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "250-p1.p1",  0x000000, 0x100000, 0x81f1f60b )
	ROM_LOAD_WIDE_SWAP( "250-p2.ep1", 0x100000, 0x400000, 0x1fda2e12 )

	NEO_SFIX_128K( "250-s1.s1", 0xfb6f441d )

	NEO_BIOS_SOUND_128K( "250-m1.m1", 0xfd42a842 )

	ROM_REGION( 0xa00000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "250-v1.v1", 0x000000, 0x400000, 0xc79ede73 )
	ROM_LOAD( "250-v2.v2", 0x400000, 0x400000, 0xea9aabe1 )
	ROM_LOAD( "250-v3.v3", 0x800000, 0x200000, 0x2ca65102 )

	NO_DELTAT_REGION

	ROM_REGION( 0x3000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "250-c1.c1", 0x0000000, 0x800000, 0x09a52c6f ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "250-c2.c2", 0x0000000, 0x800000, 0x31679821 ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "250-c3.c3", 0x1000000, 0x800000, 0xfd602019 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "250-c4.c4", 0x1000000, 0x800000, 0x31354513 ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "250-c5.c5", 0x2000000, 0x800000, 0xa4b56124 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "250-c6.c6", 0x2000000, 0x800000, 0x83e3e69d ) /* Plane 2,3 */
ROM_END

ROM_START( kof99 )
	ROM_REGION( 0x500000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "kof99_p1.rom", 0x000000, 0x100000, 0x00000000 )
	ROM_LOAD_WIDE_SWAP( "kof99_p2.rom", 0x100000, 0x400000, 0x00000000 )

	NEO_SFIX_128K( "kof99_s1.rom", 0x00000000 )

	NEO_BIOS_SOUND_128K( "kof99_m1.rom", 0x5e74539c )

	ROM_REGION( 0x0e00000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "kof99_v1.rom", 0x000000, 0x400000, 0xef2eecc8 )
	ROM_LOAD( "kof99_v2.rom", 0x400000, 0x400000, 0x73e211ca )
	ROM_LOAD( "kof99_v3.rom", 0x800000, 0x400000, 0x821901da )
	ROM_LOAD( "kof99_v4.rom", 0xc00000, 0x200000, 0xb49e6178 )

	NO_DELTAT_REGION

	ROM_REGION( 0x4000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "kof99_c1.rom", 0x0000000, 0x800000, 0x00000000 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "kof99_c2.rom", 0x0000000, 0x800000, 0x00000000 ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "kof99_c3.rom", 0x1000000, 0x800000, 0x00000000 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "kof99_c4.rom", 0x1000000, 0x800000, 0x00000000 ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "kof99_c5.rom", 0x2000000, 0x800000, 0x00000000 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "kof99_c6.rom", 0x2000000, 0x800000, 0x00000000 ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "kof99_c7.rom", 0x3000000, 0x800000, 0x00000000 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "kof99_c8.rom", 0x3000000, 0x800000, 0x00000000 ) /* Plane 2,3 */
ROM_END

ROM_START( garou )
	ROM_REGION( 0x500000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "motw_p1.rom", 0x000000, 0x100000, 0x00000000 )
	ROM_LOAD_WIDE_SWAP( "motw_p2.rom", 0x100000, 0x400000, 0x00000000 )

	NEO_SFIX_128K( "motw_s1.rom", 0x00000000 )

	NEO_BIOS_SOUND_256K( "motw_m1.rom", 0x00000000 )

	ROM_REGION( 0x1000000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "motw_v1.rom", 0x000000, 0x400000, 0x00000000 )
	ROM_LOAD( "motw_v2.rom", 0x400000, 0x400000, 0x00000000 )
	ROM_LOAD( "motw_v3.rom", 0x800000, 0x400000, 0x00000000 )
	ROM_LOAD( "motw_v4.rom", 0xc00000, 0x400000, 0x00000000 )

	NO_DELTAT_REGION

	ROM_REGION( 0x4000000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "motw_c1.rom", 0x0000000, 0x800000, 0x00000000 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "motw_c2.rom", 0x0000000, 0x800000, 0x00000000 ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "motw_c3.rom", 0x1000000, 0x800000, 0x00000000 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "motw_c4.rom", 0x1000000, 0x800000, 0x00000000 ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "motw_c5.rom", 0x2000000, 0x800000, 0x00000000 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "motw_c6.rom", 0x2000000, 0x800000, 0x00000000 ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "motw_c7.rom", 0x3000000, 0x800000, 0x00000000 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "motw_c8.rom", 0x3000000, 0x800000, 0x00000000 ) /* Plane 2,3 */
ROM_END

/******************************************************************************/

/* dummy entry for the dummy bios driver */
ROM_START( neogeo )
	ROM_REGION( 0x020000, REGION_USER1 )
	ROM_LOAD_WIDE_SWAP( "neo-geo.rom", 0x00000, 0x020000, 0x9036d879 )

	ROM_REGION( 0x40000, REGION_CPU2 )
	ROM_LOAD( "ng-sm1.rom", 0x00000, 0x20000, 0x97cf998b )

	ROM_REGION( 0x40000, REGION_GFX1 )
	ROM_LOAD( "ng-sfix.rom",  0x020000, 0x20000, 0x354029fc )
ROM_END

/******************************************************************************/
/* Later carts with protection / encryption (ROM sets as in current MAME).    */
/* Decryption is done once at load time, see machine/neocrypt.cpp.            */
/******************************************************************************/

#define NEO_BIOS_ONLY \
	ROM_REGION( 0x20000, REGION_USER1 ) \
	ROM_LOAD_WIDE_SWAP( "neo-geo.rom", 0x00000, 0x020000, 0x9036d879 )

/* the fix layer data is extracted from the C ROMs; BIOS fix data after it */
#define NEO_SFIX_FROM_C(size) \
	ROM_REGION( (size) + 0x20000, REGION_GFX1 ) \
	ROM_LOAD( "ng-sfix.rom", (size), 0x20000, 0x354029fc )

/* CMC50 encrypted M1: loaded into REGION_USER3, decrypted into REGION_CPU2 */
#define NEO_ENCRYPTED_M1(name,length,sum) \
	ROM_REGION( 0x80000, REGION_CPU2 ) \
	ROM_REGION( 0x80000, REGION_USER3 ) \
	ROM_LOAD( name, 0x00000, length, sum )

ROM_START( mslug3 )
	ROM_REGION( 0x900000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "green.neo-sma", 0x0c0000, 0x040000, 0x9cd55736 )	/* stored in the SMA chip */
	ROM_LOAD_WIDE_SWAP( "256-pg1.p1",    0x100000, 0x400000, 0xb07edfd5 )
	ROM_LOAD_WIDE_SWAP( "256-pg2.p2",    0x500000, 0x400000, 0x6097c26b )

	NEO_SFIX_FROM_C( 0x80000 )

	NEO_BIOS_ONLY
	ROM_REGION( 0x80000, REGION_CPU2 )
	ROM_LOAD( "256-m1.m1", 0x00000, 0x80000, 0xeaeec116 )

	ROM_REGION( 0x1000000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "256-v1.v1", 0x000000, 0x400000, 0xf2690241 )
	ROM_LOAD( "256-v2.v2", 0x400000, 0x400000, 0x7e2a10bd )
	ROM_LOAD( "256-v3.v3", 0x800000, 0x400000, 0x0eaec17c )
	ROM_LOAD( "256-v4.v4", 0xc00000, 0x400000, 0x9b4b22d4 )

	NO_DELTAT_REGION

	ROM_REGION( 0x4000000, REGION_GFX2 )	/* encrypted (CMC42) */
	ROM_LOAD_GFX_EVEN( "256-c1.c1", 0x0000000, 0x800000, 0x5a79c34e ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "256-c2.c2", 0x0000000, 0x800000, 0x944c362c ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "256-c3.c3", 0x1000000, 0x800000, 0x6e69d36f ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "256-c4.c4", 0x1000000, 0x800000, 0xb755b4eb ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "256-c5.c5", 0x2000000, 0x800000, 0x7aacab47 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "256-c6.c6", 0x2000000, 0x800000, 0xc698fd5d ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "256-c7.c7", 0x3000000, 0x800000, 0xcfceddd2 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "256-c8.c8", 0x3000000, 0x800000, 0x4d9be34c ) /* Plane 2,3 */
ROM_END

ROM_START( mslug4 )
	ROM_REGION( 0x500000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "263-p1.p1",  0x000000, 0x100000, 0x27e4def3 )
	ROM_LOAD_WIDE_SWAP( "263-p2.sp2", 0x100000, 0x400000, 0xfdb7aed8 )

	NEO_SFIX_FROM_C( 0x20000 )

	NEO_BIOS_ONLY
	NEO_ENCRYPTED_M1( "263-m1.m1", 0x20000, 0x46ac8228 )

	ROM_REGION( 0x1000000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )	/* encrypted (NEO-PCM2) */
	ROM_LOAD( "263-v1.v1", 0x000000, 0x800000, 0x01e9b9cd )
	ROM_LOAD( "263-v2.v2", 0x800000, 0x800000, 0x4ab2bf81 )

	NO_DELTAT_REGION

	ROM_REGION( 0x3000000, REGION_GFX2 )	/* encrypted (CMC50) */
	ROM_LOAD_GFX_EVEN( "263-c1.c1", 0x0000000, 0x800000, 0x84865f8a ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "263-c2.c2", 0x0000000, 0x800000, 0x81df97f2 ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "263-c3.c3", 0x1000000, 0x800000, 0x1a343323 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "263-c4.c4", 0x1000000, 0x800000, 0x942cfb44 ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "263-c5.c5", 0x2000000, 0x800000, 0xa748854f ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "263-c6.c6", 0x2000000, 0x800000, 0x5c8ba116 ) /* Plane 2,3 */
ROM_END

ROM_START( mslug5 )
	ROM_REGION( 0x800000, REGION_CPU1 )	/* filled by init_mslug5 */
	ROM_REGION( 0x800000, REGION_USER2 )	/* 32-bit interleaved, encrypted (PVC) */
	ROM_LOAD( "268-p1cr.p1", 0x000000, 0x400000, 0xd0466792 )
	ROM_LOAD( "268-p2cr.p2", 0x400000, 0x400000, 0xfbf6b61e )

	NEO_SFIX_FROM_C( 0x20000 )

	NEO_BIOS_ONLY
	NEO_ENCRYPTED_M1( "268-m1.m1", 0x80000, 0x4a5a6e0e )

	ROM_REGION( 0x1000000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )	/* encrypted (NEO-PCM2) */
	ROM_LOAD( "268-v1c.v1", 0x000000, 0x800000, 0xae31d60c )
	ROM_LOAD( "268-v2c.v2", 0x800000, 0x800000, 0xc40613ed )

	NO_DELTAT_REGION

	ROM_REGION( 0x4000000, REGION_GFX2 )	/* encrypted (CMC50) */
	ROM_LOAD_GFX_EVEN( "268-c1c.c1", 0x0000000, 0x800000, 0xab7c389a ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "268-c2c.c2", 0x0000000, 0x800000, 0x3560881b ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "268-c3c.c3", 0x1000000, 0x800000, 0x3af955ea ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "268-c4c.c4", 0x1000000, 0x800000, 0xc329c373 ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "268-c5c.c5", 0x2000000, 0x800000, 0x959c8177 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "268-c6c.c6", 0x2000000, 0x800000, 0x010a831b ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "268-c7c.c7", 0x3000000, 0x800000, 0x6d72a969 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "268-c8c.c8", 0x3000000, 0x800000, 0x551d720e ) /* Plane 2,3 */
ROM_END

ROM_START( samsho5 )
	ROM_REGION( 0x800000, REGION_CPU1 )	/* encrypted (block scramble) */
	ROM_LOAD_WIDE_SWAP( "270-p1.p1",  0x000000, 0x400000, 0x4a2a09e6 )
	ROM_LOAD_WIDE_SWAP( "270-p2.sp2", 0x400000, 0x400000, 0xe0c74c85 )

	NEO_SFIX_FROM_C( 0x20000 )

	NEO_BIOS_ONLY
	NEO_ENCRYPTED_M1( "270-m1.m1", 0x80000, 0x49c9901a )

	ROM_REGION( 0x1000000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )	/* encrypted (NEO-PCM2) */
	ROM_LOAD( "270-v1.v1", 0x000000, 0x800000, 0x62e434eb )
	ROM_LOAD( "270-v2.v2", 0x800000, 0x800000, 0x180f3c9a )

	NO_DELTAT_REGION

	ROM_REGION( 0x4000000, REGION_GFX2 )	/* encrypted (CMC50) */
	ROM_LOAD_GFX_EVEN( "270-c1.c1", 0x0000000, 0x800000, 0x14ffffac ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "270-c2.c2", 0x0000000, 0x800000, 0x401f7299 ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "270-c3.c3", 0x1000000, 0x800000, 0x838f0260 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "270-c4.c4", 0x1000000, 0x800000, 0x041560a5 ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "270-c5.c5", 0x2000000, 0x800000, 0xbd30b52d ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "270-c6.c6", 0x2000000, 0x800000, 0x86a69c70 ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "270-c7.c7", 0x3000000, 0x800000, 0xd28fbc3c ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "270-c8.c8", 0x3000000, 0x800000, 0x02c530a6 ) /* Plane 2,3 */
ROM_END

ROM_START( samsh5sp )
	ROM_REGION( 0x800000, REGION_CPU1 )	/* encrypted (block scramble) */
	ROM_LOAD_WIDE_SWAP( "272-p1.p1",  0x000000, 0x400000, 0xfb7a6bba )
	ROM_LOAD_WIDE_SWAP( "272-p2.sp2", 0x400000, 0x400000, 0x63492ea6 )

	NEO_SFIX_FROM_C( 0x20000 )

	NEO_BIOS_ONLY
	NEO_ENCRYPTED_M1( "272-m1.m1", 0x80000, 0xadeebf40 )

	ROM_REGION( 0x1000000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )	/* encrypted (NEO-PCM2) */
	ROM_LOAD( "272-v1.v1", 0x000000, 0x800000, 0x76a94127 )
	ROM_LOAD( "272-v2.v2", 0x800000, 0x800000, 0x4ba507f1 )

	NO_DELTAT_REGION

	ROM_REGION( 0x4000000, REGION_GFX2 )	/* encrypted (CMC50) */
	ROM_LOAD_GFX_EVEN( "272-c1.c1", 0x0000000, 0x800000, 0x4f97661a ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "272-c2.c2", 0x0000000, 0x800000, 0xa3afda4f ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "272-c3.c3", 0x1000000, 0x800000, 0x8c3c7502 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "272-c4.c4", 0x1000000, 0x800000, 0x32d5e2e2 ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "272-c5.c5", 0x2000000, 0x800000, 0x6ce085bc ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "272-c6.c6", 0x2000000, 0x800000, 0x05c8dc8e ) /* Plane 2,3 */
	ROM_LOAD_GFX_EVEN( "272-c7.c7", 0x3000000, 0x800000, 0x1417b742 ) /* Plane 0,1 */
	ROM_LOAD_GFX_ODD ( "272-c8.c8", 0x3000000, 0x800000, 0xd49773cd ) /* Plane 2,3 */
ROM_END

/******************************************************************************/

static void neo_decrypt_sprites(int cmc50, int key)
{
	UINT8 *spr = memory_region(REGION_GFX2);
	UINT32 len = memory_region_length(REGION_GFX2);
	UINT32 fixlen = memory_region_length(REGION_GFX1) - 0x20000;

	/* the sprite region has the same byte layout as in current MAME
	   (ROM_LOAD_GFX_EVEN -> ROM_LOAD_ODD, and the loader flips it back on LSB_FIRST) */
	if (cmc50) neo_cmc50_gfx_decrypt(spr, len, key);
	else       neo_cmc42_gfx_decrypt(spr, len, key);
	neo_sfix_decrypt(spr, len, memory_region(REGION_GFX1), fixlen);
}

static void neo_decrypt_m1(void)
{
	neo_cmc50_m1_decrypt(memory_region(REGION_USER3), memory_region(REGION_CPU2), memory_region_length(REGION_CPU2));
}

/* Metal Slug X protection (Altera chip next to the P1 ROM) */
static int mslugx_counter, mslugx_command;

static WRITE_HANDLER( mslugx_protection_w )
{
	data &= 0xffff;
	switch (offset >> 1)
	{
		case 0x0/2: mslugx_command = 0; break;				/* start new read? */
		case 0x2/2: case 0x4/2: mslugx_command |= data; break;	/* command */
		case 0xa/2: mslugx_counter = 0; mslugx_command = 0; break;	/* init? */
		default: break;
	}
}

static READ_HANDLER( mslugx_protection_r )
{
	UINT8 *RAM = memory_region(REGION_CPU1);
	int res = 0;

	switch (mslugx_command)
	{
		case 0x0001:
			res = (RAM[(0xdedd2 + ((mslugx_counter >> 3) & 0xfff)) ^ 1] >> (~mslugx_counter & 7)) & 1;
			mslugx_counter++;
			break;
		case 0x0fff:
		{
			int select = READ_WORD(&neogeo_ram[0xf00a]) - 1;
			res = (RAM[(0xdedd2 + ((select >> 3) & 0x0fff)) ^ 1] >> (~select & 7)) & 1;
			break;
		}
	}
	return res;
}

void init_mslugx(void)
{
	init_neogeo();
	mslugx_counter = mslugx_command = 0;
	install_mem_read_handler (0, 0x2fffe0, 0x2fffef, mslugx_protection_r);
	install_mem_write_handler(0, 0x2fffe0, 0x2fffef, mslugx_protection_w);
}

/* Metal Slug 3: SMA bankswitch + protection, CMC42 graphics */
static WRITE_HANDLER( mslug3_bankswitch_w )
{
	cpu_setbank(4, &memory_region(REGION_CPU1)[neo_mslug3_bank_base(data & 0xffff)]);
}

static READ_HANDLER( mslug3_prot_r )
{
	return 0x9a37;
}

void init_mslug3(void)
{
	neo_mslug3_decrypt_68k(memory_region(REGION_CPU1));
	neo_decrypt_sprites(0, MSLUG3_GFX_KEY);
	init_neogeo();
	neogeo_fix_bank_type = 1;
	install_mem_write_handler(0, 0x2fffe4, 0x2fffe5, mslug3_bankswitch_w);
	install_mem_read_handler (0, 0x2fe446, 0x2fe447, mslug3_prot_r);
}

/* Metal Slug 4: CMC50 graphics and M1, NEO-PCM2 sound */
void init_mslug4(void)
{
	neo_decrypt_m1();
	neo_decrypt_sprites(1, MSLUG4_GFX_KEY);
	if (memory_region(REGION_SOUND1))
		neo_pcm2_decrypt(memory_region(REGION_SOUND1), memory_region_length(REGION_SOUND1), 8);
	init_neogeo();
}

/* Samurai Shodown V / V Special: scrambled P ROM, CMC50, NEO-PCM2 */
void init_samsho5(void)
{
	neo_samsho5_decrypt_68k(memory_region(REGION_CPU1));
	neo_decrypt_m1();
	neo_decrypt_sprites(1, SAMSHO5_GFX_KEY);
	if (memory_region(REGION_SOUND1))
		neo_pcm2_swap(memory_region(REGION_SOUND1), memory_region_length(REGION_SOUND1), 4);
	init_neogeo();
}

void init_samsh5sp(void)
{
	neo_samsh5sp_decrypt_68k(memory_region(REGION_CPU1));
	neo_decrypt_m1();
	neo_decrypt_sprites(1, SAMSHO5SP_GFX_KEY);
	if (memory_region(REGION_SOUND1))
		neo_pcm2_swap(memory_region(REGION_SOUND1), memory_region_length(REGION_SOUND1), 6);
	init_neogeo();
}

/* Metal Slug 5: PVC (P ROM scramble, bankswitch and colour helpers in cart RAM) */
static UINT16 pvc_ram[0x1000];

static READ_HANDLER( pvc_prot_r )
{
	return pvc_ram[(offset >> 1) & 0xfff];
}

static WRITE_HANDLER( pvc_prot_w )
{
	int o = (offset >> 1) & 0xfff;
	pvc_ram[o] = COMBINE_WORD(pvc_ram[o], data);

	if (o == 0xff0)
	{
		/* unpack colour */
		int pen = pvc_ram[0xff0];
		int b = ((pen & 0x000f) << 1) | ((pen & 0x1000) >> 12);
		int g = ((pen & 0x00f0) >> 3) | ((pen & 0x2000) >> 13);
		int r = ((pen & 0x0f00) >> 7) | ((pen & 0x4000) >> 14);
		int sh = (pen & 0x8000) >> 15;
		pvc_ram[0xff1] = (g << 8) | b;
		pvc_ram[0xff2] = (sh << 8) | r;
	}
	else if (o >= 0xff4 && o <= 0xff5)
	{
		/* pack colour */
		int gb = pvc_ram[0xff4], sr = pvc_ram[0xff5];
		pvc_ram[0xff6] = ((gb & 0x001e) >> 1) | ((gb & 0x1e00) >> 5) | ((sr & 0x001e) << 7) |
			((gb & 0x0001) << 12) | ((gb & 0x0100) << 5) | ((sr & 0x0001) << 14) | ((sr & 0x0100) << 7);
	}
	else if (o >= 0xff8)
	{
		/* bankswitch */
		UINT32 bank = ((pvc_ram[0xff8] >> 8) | (pvc_ram[0xff9] << 8)) + 0x100000;
		pvc_ram[0xff8] = (pvc_ram[0xff8] & 0xfe00) | 0x00a0;
		pvc_ram[0xff9] &= 0x7fff;
		if (bank + 0x100000 <= (UINT32)memory_region_length(REGION_CPU1))
			cpu_setbank(4, &memory_region(REGION_CPU1)[bank]);
	}
}

void init_mslug5(void)
{
	UINT16 *dst = (UINT16 *)memory_region(REGION_CPU1);
	UINT8 *src = memory_region(REGION_USER2);
	int i;

	/* ROM_LOAD32_WORD_SWAP of p1 (even words) and p2 (odd words) */
	for (i = 0; i < 0x400000 / 2; i++)
	{
		/* Neo-Geo P ROM files hold the 68000 words little-endian, like WIDE_SWAP loads them */
		dst[2*i + 0] = src[2*i] | (src[2*i + 1] << 8);
		dst[2*i + 1] = src[0x400000 + 2*i] | (src[0x400000 + 2*i + 1] << 8);
	}

	neo_mslug5_decrypt_68k(memory_region(REGION_CPU1));
	neo_decrypt_m1();
	neo_decrypt_sprites(1, MSLUG5_GFX_KEY);
	if (memory_region(REGION_SOUND1))
		neo_pcm2_swap(memory_region(REGION_SOUND1), memory_region_length(REGION_SOUND1), 2);
	init_neogeo();

	memset(pvc_ram, 0, sizeof pvc_ram);
	install_mem_read_handler (0, 0x2fe000, 0x2fffff, pvc_prot_r);
	install_mem_write_handler(0, 0x2fe000, 0x2fffff, pvc_prot_w);
}

ROM_START( ghostlop )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "proto_228-p1.p1", 0x000000, 0x100000, 0x6033172e )
	NEO_SFIX_128K( "proto_228-s1.s1", 0x83c24e81 )
	NEO_BIOS_SOUND_128K( "proto_228-m1.m1", 0xfd833b33 )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "proto_228-v1.v1", 0x000000, 0x200000, 0xc603fce6 )
	ROM_REGION( 0x800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "proto_228-c1.c1", 0x0000000, 0x400000, 0xbfc99efe )
	ROM_LOAD_GFX_ODD( "proto_228-c2.c2", 0x0000000, 0x400000, 0x69788082 )
ROM_END

ROM_START( kof97pls )
	ROM_REGION( 0x500000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "kf97-p1p.bin", 0x000000, 0x100000, 0xc01fda46 )
	ROM_LOAD_WIDE_SWAP( "kf97-p2p.bin", 0x100000, 0x400000, 0x5502b020 )
	NEO_SFIX_128K( "kf97-s1p.bin", 0x73254270 )
	NEO_BIOS_SOUND_128K( "232-m1.m1", 0x45348747 )
	ROM_REGION( 0xc00000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "232-v1.v1", 0x000000, 0x400000, 0x22a2b5b5 )
	ROM_LOAD( "232-v2.v2", 0x400000, 0x400000, 0x2304e744 )
	ROM_LOAD( "232-v3.v3", 0x800000, 0x400000, 0x759eb954 )
	ROM_REGION( 0x2800000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "232-c1.c1", 0x0000000, 0x800000, 0x5f8bf0a1 )
	ROM_LOAD_GFX_ODD( "232-c2.c2", 0x0000000, 0x800000, 0xe4d45c81 )
	ROM_LOAD_GFX_EVEN( "232-c3.c3", 0x1000000, 0x800000, 0x581d6618 )
	ROM_LOAD_GFX_ODD( "232-c4.c4", 0x1000000, 0x800000, 0x49bb1e68 )
	ROM_LOAD_GFX_EVEN( "232-c5.c5", 0x2000000, 0x400000, 0x34fc4e51 )
	ROM_LOAD_GFX_ODD( "232-c6.c6", 0x2000000, 0x400000, 0x4ff4d47b )
ROM_END

ROM_START( zintrckb )
	ROM_REGION( 0x100000, REGION_CPU1 )
	ROM_LOAD_WIDE_SWAP( "zin-p1.bin", 0x000000, 0x100000, 0x06c8fca7 )
	NEO_SFIX_128K( "zin-s1.bin", 0xa7ab0e81 )
	NEO_BIOS_SOUND_128K( "zin-m1.bin", 0xfd9627ca )
	ROM_REGION( 0x200000, REGION_SOUND1 | REGIONFLAG_SOUNDONLY )
	ROM_LOAD( "zin-v1.bin", 0x000000, 0x200000, 0xc09f74f1 )
	ROM_REGION( 0x400000, REGION_GFX2 )
	ROM_LOAD_GFX_EVEN( "zin-c1.bin", 0x0000000, 0x200000, 0x76aee189 )
	ROM_LOAD_GFX_ODD( "zin-c2.bin", 0x0000000, 0x200000, 0x844ed4b3 )
ROM_END

/******************************************************************************/

/* For MGD-2 dumps */
static void shuffle(unsigned char *buf,int len)
{
	int i;
	unsigned char t;

	if (len == 2) return;

	if (len == 6)
	{
		unsigned char swp[6];

		memcpy(swp,buf,6);
		buf[0] = swp[0];
		buf[1] = swp[3];
		buf[2] = swp[1];
		buf[3] = swp[4];
		buf[4] = swp[2];
		buf[5] = swp[5];
		return;
	}

	if (len % 4) exit(1);	/* must not happen */

	len /= 2;

	for (i = 0;i < len/2;i++)
	{
		t = buf[len/2 + i];
		buf[len/2 + i] = buf[len + i];
		buf[len + i] = t;
	}

	shuffle(buf,len);
	shuffle(buf + len,len);
}

void init_mgd2(void)
{
	unsigned char *gfxdata = memory_region(REGION_GFX2);
	int len = memory_region_length(REGION_GFX2);


	init_neogeo();

	/*
		data is now in the order 0 4 8 12... 1 5 9 13... 2 6 10 14... 3 7 11 15...
		we must convert it to the MVS order 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15...
		to do so we use a recursive function which doesn't use additional memory
		(it could be easily converted into an iterative one).
		It's called shuffle because it mimics the shuffling of a deck of cards.
	*/
	shuffle(gfxdata,len);
	/* data is now in the order 0 2 4 8 10 12 14... 1 3 5 7 9 11 13 15... */
	shuffle(gfxdata,len);
	/* data is now in the order 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15... */

    gfxdata=memory_region(REGION_GFX3);
    len = memory_region_length(REGION_GFX3);
    if (gfxdata)
    {
    	shuffle(gfxdata,len);
    	shuffle(gfxdata,len);
    }
}

/******************************************************************************/


/* A dummy driver, so that the bios can be debugged, and to serve as */
/* parent for the neo-geo.rom file, so that we do not have to include */
/* it in every zip file */
GAMEX( 1990, neogeo, 0, neogeo, neogeo, neogeo, ROT0, "SNK", "Neo-Geo", NOT_A_DRIVER )

/******************************************************************************/

/*    YEAR  NAME      PARENT    MACHINE INPUT    INIT    MONITOR  */

/* SNK */
GAME( 1990, nam1975,  neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "NAM-1975" )
GAME( 1990, bstars,   neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Baseball Stars Professional" )
GAME( 1990, tpgolf,   neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Top Player's Golf" )
GAME( 1990, mahretsu, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Mahjong Kyoretsuden" )
GAME( 1990, ridhero,  neogeo,   raster, neogeo,  neogeo,   ROT0_16BIT, "SNK", "Riding Hero" )
GAME( 1991, alpham2,  neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Alpha Mission II / ASO II - Last Guardian" )
GAME( 1990, cyberlip, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Cyber-Lip" )
GAME( 1990, superspy, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "The Super Spy" )
GAME( 1992, mutnat,   neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Mutation Nation" )
GAME( 1991, kotm,     neogeo,   neogeo, neogeo,  neogeo,   ROT0_16BIT, "SNK", "King of the Monsters" )
GAME( 1991, sengoku,  neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Sengoku / Sengoku Denshou (set 1)" )
GAME( 1991, sengokuh,  sengoku,  neogeo, neogeo,  neogeo, ROT0,       "SNK", "Sengoku / Sengoku Denshou (set 2)" )
GAME( 1991, burningf, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Burning Fight (set 1)" )
GAME( 1991, burningfh, burningf, neogeo, neogeo,  neogeo, ROT0,       "SNK", "Burning Fight (set 2)" )
GAME( 1990, lbowling, neogeo,   neogeo, neogeo,  neogeo,   ROT0,       "SNK", "League Bowling" )
GAME( 1991, gpilots,  neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Ghost Pilots" )
GAME( 1990, joyjoy,   neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Puzzled / Joy Joy Kid" )
GAME( 1991, quizdais, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Quiz Daisousa Sen - The Last Count Down" )
GAME( 1992, lresort,  neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Last Resort" )
GAME( 1991, eightman, neogeo,   neogeo, neogeo,  neogeo,   ROT0,       "SNK / Pallas", "Eight Man" )
GAME( 1991, legendos, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Legend of Success Joe / Ashitano Joe Densetsu" )
GAME( 1991, 2020bb,   neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK / Pallas", "2020 Super Baseball (set 1)" )
GAME( 1991, 2020bbh,  2020bb,   neogeo, neogeo,  neogeo, ROT0,       "SNK / Pallas", "2020 Super Baseball (set 2)" )
GAME( 1991, socbrawl, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Soccer Brawl" )
GAME( 1991, fatfury1, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Fatal Fury - King of Fighters / Garou Densetsu - shukumei no tatakai" )
GAME( 1991, roboarmy, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Robo Army" )
GAME( 1992, fbfrenzy, neogeo,   neogeo, neogeo,  neogeo,   ROT0,       "SNK", "Football Frenzy" )
GAME( 1992, kotm2,    neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "King of the Monsters 2 - The Next Thing" )
GAME( 1993, sengoku2, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Sengoku 2 / Sengoku Denshou 2")
GAME( 1992, bstars2,  neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Baseball Stars 2" )
GAME( 1992, quizdai2, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Quiz Meintantei Neo Geo - Quiz Daisousa Sen Part 2" )
GAME( 1993, 3countb,  neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "3 Count Bout / Fire Suplex" )
GAME( 1992, aof,      neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Art of Fighting / Ryuuko no Ken" )
GAME( 1993, samsho,   neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Samurai Shodown / Samurai Spirits" )
GAME( 1994, tophuntr, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Top Hunter - Roddy & Cathy" )
GAME( 1992, fatfury2, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Fatal Fury 2 / Garou Densetsu 2 - arata-naru tatakai" )
GAME( 1992, ssideki,  neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Super Sidekicks / Tokuten Ou" )
GAME( 1994, kof94,    neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "The King of Fighters '94" )
GAME( 1994, aof2,     neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Art of Fighting 2 / Ryuuko no Ken 2" )
GAME( 1993, fatfursp, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "SNK", "Fatal Fury Special / Garou Densetsu Special" )
GAME( 1995, savagere, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Savage Reign / Fu'un Mokushiroku - kakutou sousei" )
GAME( 1994, ssideki2, neogeo,   raster, neogeo,  neogeo, ROT0_16BIT, "SNK", "Super Sidekicks 2 - The World Championship / Tokuten Ou 2 - real fight football" )
GAME( 1994, samsho2,  neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Samurai Shodown II / Shin Samurai Spirits - Haohmaru jigokuhen" )
GAME( 1995, fatfury3, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Fatal Fury 3 - Road to the Final Victory / Garou Densetsu 3 - haruka-naru tatakai" )
GAME( 1995, ssideki3, neogeo,   raster, neogeo,  neogeo, ROT0_16BIT, "SNK", "Super Sidekicks 3 - The Next Glory / Tokuten Ou 3 - eikoue no michi" )
GAME( 1995, kof95,    neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "The King of Fighters '95" )
GAME( 1995, samsho3,  neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Samurai Shodown III / Samurai Spirits - Zankurou Musouken" )
GAME( 1995, rbff1,    neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Real Bout Fatal Fury / Real Bout Garou Densetsu" )
GAME( 1996, aof3,     neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Art of Fighting 3 - The Path of the Warrior / Art of Fighting - Ryuuko no Ken Gaiden" )
GAME( 1996, kof96,    neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "The King of Fighters '96" )
GAME( 1996, ssideki4, neogeo,   raster, neogeo,  neogeo, ROT0_16BIT, "SNK", "The Ultimate 11 / Tokuten Ou - Honoo no Libero" )
GAME( 1996, kizuna,   neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Kizuna Encounter - Super Tag Battle / Fu'un Super Tag Battle" )
GAME( 1996, samsho4,  neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Samurai Shodown IV - Amakusa's Revenge / Samurai Spirits - Amakusa Kourin" )
GAME( 1996, rbffspec, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Real Bout Fatal Fury Special / Real Bout Garou Densetsu Special" )
GAME( 1997, kof97,    neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "The King of Fighters '97" )
GAME( 1997, lastblad, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "The Last Blade / Bakumatsu Roman - Gekkano Kenshi" )
GAME( 1997, irrmaze,  neogeo,   neogeo, irrmaze, neogeo, ROT0,       "SNK / Saurus", "The Irritating Maze / Ultra Denryu Iraira Bou" )
GAME( 1998, rbff2,    neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Real Bout Fatal Fury 2 - The Newcomers / Real Bout Garou Densetsu 2 - the newcomers" )
GAME( 1998, mslug2,   neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Metal Slug 2 - Super Vehicle-001/II" )
GAME( 1998, kof98,    neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "The King of Fighters '98 - The Slugfest / King of Fighters '98 - dream match never ends" )
GAME( 1998, lastbld2, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "The Last Blade 2 / Bakumatsu Roman - Dai Ni Maku Gekkano Kenshi" )
GAME( 1998, neocup98, neogeo,   raster, neogeo,  neogeo, ROT0_16BIT, "SNK", "Neo-Geo Cup '98 - The Road to the Victory" )
GAME( 1999, mslugx,   neogeo,   neogeo, neogeo,  mslugx, ROT0_16BIT, "SNK", "Metal Slug X - Super Vehicle-001" )
GAME( 1999, kof99,    neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "The King of Fighters '99 - Millennium Battle" )
GAME( 1999, garou,    neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "SNK", "Garou - Mark of the Wolves" )
GAME( 2000, mslug3,   neogeo,   neobigfix, neogeo, mslug3, ROT0_16BIT, "SNK", "Metal Slug 3" )
GAME( 2002, mslug4,   neogeo,   neogeo, neogeo,  mslug4, ROT0_16BIT, "Mega / Noise Factory / Playmore", "Metal Slug 4" )
GAME( 2003, mslug5,   neogeo,   neogeo, neogeo,  mslug5, ROT0_16BIT, "SNK Playmore", "Metal Slug 5" )
GAME( 2003, samsho5,  neogeo,   neogeo, neogeo,  samsho5, ROT0_16BIT, "Yuki Enterprise / SNK Playmore", "Samurai Shodown V / Samurai Spirits Zero" )
GAME( 2004, samsh5sp, neogeo,   neogeo, neogeo,  samsh5sp, ROT0_16BIT, "Yuki Enterprise / SNK Playmore", "Samurai Shodown V Special / Samurai Spirits Zero Special" )
GAME( 1996, zintrckb, neogeo  , neogeo, neogeo,  neogeo, ROT0_16BIT, "bootleg", "Zintrick / Oshidashi Zentrix (bootleg of CD version)" )
GAME( 1997, kof97pls, kof97   , neogeo, neogeo,  neogeo, ROT0_16BIT, "bootleg", "The King of Fighters '97 Plus (bootleg)" )
GAME( 1996, ghostlop, neogeo  , neogeo, neogeo,  neogeo, ROT0_16BIT, "Data East Corporation", "Ghostlop (prototype)" )

/* Alpha Denshi Co. / ADK (changed name in 1993) */
GAME( 1990, maglord,  neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Alpha Denshi Co.", "Magician Lord (set 1)" )
GAME( 1990, maglordh, maglord,  neogeo, neogeo,  neogeo, ROT0,       "Alpha Denshi Co.", "Magician Lord (set 2)" )
GAME( 1990, ncombat,  neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Alpha Denshi Co.", "Ninja Combat" )
GAME( 1990, bjourney, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Alpha Denshi Co.", "Blue's Journey / Raguy" )
GAME( 1991, crsword,  neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Alpha Denshi Co.", "Crossed Swords" )
GAME( 1991, trally,   neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Alpha Denshi Co.", "Thrash Rally" )
GAME( 1992, ncommand, neogeo,   neogeo, neogeo,  neogeo,   ROT0,       "Alpha Denshi Co.", "Ninja Commando" )
GAME( 1992, wh1,      neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Alpha Denshi Co.", "World Heroes" )
GAME( 1993, wh2,      neogeo,   neogeo, neogeo,  neogeo, ROT0,       "ADK",              "World Heroes 2" )
GAME( 1994, wh2j,     neogeo,   neogeo, neogeo,  neogeo, ROT0,       "ADK / SNK",        "World Heroes 2 Jet" )
GAME( 1994, aodk,     neogeo,   neogeo, neogeo,  neogeo, ROT0,       "ADK / SNK",        "Aggressors of Dark Kombat / Tsuukai GANGAN Koushinkyoku" )
GAME( 1995, whp,      neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "ADK / SNK",        "World Heroes Perfect" )
GAME( 1995, moshougi, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "ADK / SNK",        "Syougi No Tatsujin - Master of Syougi" )
GAME( 1996, overtop,  neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "ADK",              "Over Top" )
GAME( 1996, ninjamas, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "ADK / SNK",        "Ninja Master's - haoh-ninpo-cho" )
GAME( 1996, twinspri, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "ADK",              "Twinkle Star Sprites" )

/* Aicom */
GAME( 1994, janshin,  neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Aicom", "Jyanshin Densetsu - Quest of Jongmaster" )
GAME( 1995, pulstar,  neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "Aicom", "Pulstar" )

/* Data East Corporation */
GAME( 1993, spinmast, neogeo,   raster, neogeo,  neogeo, ROT0_16BIT, "Data East Corporation", "Spinmaster / Miracle Adventure" )
GAME( 1994, wjammers, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Data East Corporation", "Windjammers / Flying Power Disc" )
GAME( 1994, karnovr,  neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "Data East Corporation", "Karnov's Revenge / Fighter's History Dynamite" )
GAME( 1994, strhoop,  neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Data East Corporation", "Street Hoop / Street Slam / Dunk Dream" )
GAME( 1996, magdrop2, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Data East Corporation", "Magical Drop II" )
GAME( 1997, magdrop3, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Data East Corporation", "Magical Drop III" )

/* Face */
GAME( 1994, gururin,  neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Face", "Gururin" )
GAME( 1997, miexchng, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "Face", "Money Puzzle Exchanger / Money Idol Exchanger" )

/* Hudson Soft */
GAME( 1994, panicbom, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Eighting / Hudson", "Panic Bomber" )
GAME( 1995, kabukikl, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "Hudson", "Kabuki Klash - Far East of Eden / Tengai Makyou Shinden - Far East of Eden" )
GAME( 1997, neobombe, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Hudson", "Neo Bomberman" )

/* Monolith Corp. */
GAME( 1990, minasan,  neogeo,   neogeo, neogeo,  neogeo,   ROT0,       "Monolith Corp.", "Minnasanno Okagesamadesu" )
GAME( 1991, bakatono, neogeo,   neogeo, neogeo,  neogeo,   ROT0,       "Monolith Corp.", "Bakatonosama Mahjong Manyuki" )

/* Nazca */
GAME( 1996, turfmast, neogeo,   raster, neogeo,  neogeo, ROT0_16BIT, "Nazca", "Neo Turf Masters / Big Tournament Golf" )
GAME( 1996, mslug,    neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Nazca", "Metal Slug - Super Vehicle-001" )

/* NMK */
GAME( 1994, zedblade, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "NMK", "Zed Blade / Operation Ragnarok" )

/* Sammy */
GAME( 1992, viewpoin, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Sammy", "Viewpoint" )

/* Saurus */
GAME( 1995, quizkof,  neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Saurus", "Quiz King of Fighters" )
GAME( 1995, stakwin,  neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Saurus", "Stakes Winner / Stakes Winner - GI kinzen seihae no michi" )
GAME( 1996, ragnagrd, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "Saurus", "Operation Ragnagard / Shin-Oh-Ken" )
GAME( 1996, pgoal,    neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Saurus", "Pleasure Goal / Futsal - 5 on 5 Mini Soccer" )
GAME( 1996, stakwin2, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Saurus", "Stakes Winner 2" )
GAME( 1997, shocktro, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "Saurus", "Shock Troopers" )
GAME( 1998, shocktr2, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "Saurus", "Shock Troopers - 2nd Squad" )

/* Sunsoft */
GAME( 1995, galaxyfg, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "Sunsoft", "Galaxy Fight - Universal Warriors" )
GAME( 1996, wakuwak7, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "Sunsoft", "Waku Waku 7" )

/* Taito */
GAME( 1994, pbobblen,  neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Taito", "Puzzle Bobble / Bust-A-Move (Neo-Geo)" )
GAME( 1999, pbobbl2n, neogeo  , neogeo, neogeo,  neogeo, ROT0_16BIT, "Taito (SNK license)", "Puzzle Bobble 2 / Bust-A-Move Again (Neo-Geo)" )

/* Takara */
GAME( 1995, marukodq, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Takara", "Chibi Marukochan Deluxe Quiz" )

/* Technos */
GAME( 1995, doubledr, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Technos", "Double Dragon (Neo-Geo)" )
GAME( 1995, gowcaizr, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "Technos", "Voltage Fighter - Gowcaizer / Choujin Gakuen Gowcaizer")
GAME( 1996, sdodgeb,  neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "Technos", "Super Dodge Ball / Kunio no Nekketsu Toukyuu Densetsu" )

/* Tecmo */
GAME( 1996, twsoc96,    neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Tecmo", "Tecmo World Soccer '96" )

/* Yumekobo */
GAME( 1998, blazstar, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "Yumekobo", "Blazing Star" )

/* Viccom */
GAME( 1994, fightfev, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Viccom", "Fight Fever / Crystal Legacy" )

/* Video System Co. */
GAME( 1994, pspikes2, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Video System Co.", "Power Spikes II" )
GAME( 1994, sonicwi2, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Video System Co.", "Aero Fighters 2 / Sonic Wings 2" )
GAME( 1995, sonicwi3, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Video System Co.", "Aero Fighters 3 / Sonic Wings 3" )
GAME( 1997, popbounc, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "Video System Co.", "Pop 'n Bounce / Gapporin" )

/* Visco */
GAME( 1992, androdun, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Visco", "Andro Dunos" )
GAME( 1995, puzzledp, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Taito (Visco license)", "Puzzle De Pon" )
GAME( 1996, neomrdo,  neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Visco", "Neo Mr. Do!" )
GAME( 1995, goalx3,   neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Visco", "Goal! Goal! Goal!" )
GAME( 1996, neodrift, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Visco", "Neo Drift Out - New Technology" )
GAME( 1996, breakers, neogeo,   neogeo, neogeo,  neogeo, ROT0_16BIT, "Visco", "Breakers" )
GAME( 1997, puzzldpr, puzzledp, neogeo, neogeo,  neogeo, ROT0,       "Taito (Visco license)", "Puzzle De Pon R" )
GAME( 1998, breakrev, breakers, neogeo, neogeo,  neogeo, ROT0_16BIT, "Visco", "Breakers Revenge")
GAME( 1998, flipshot, neogeo,   neogeo, neogeo,  neogeo, ROT0,       "Visco", "Battle Flip Shot" )
GAME( 1999, ctomaday, neogeo  , neogeo, neogeo,  neogeo, ROT0_16BIT, "Visco", "Captain Tomaday" )
