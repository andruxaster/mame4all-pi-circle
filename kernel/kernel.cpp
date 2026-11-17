//
// kernel.cpp - MAME4ALL for Raspberry Pi Zero / Pi 1 without Linux (Circle + circle-stdlib)
//
// Data folder on the SD card: /mame4all (mame.cfg, roms/, nvram/, hi/, cfg/, skins/ ...)
// cmdline.txt options:  game=<name>   start this game directly (no menu)
//                       aspect=169|43 shape of the screen (default 169)
//                       sounddev=sndpwm|sndhdmi|sndnone
//
#include "kernel.h"
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/screen.h>
#include <circle/string.h>
#include <circle/util.h>
#include <SDL.h>		// SDL key codes of the MAME4ALL OSD layer (src/circle/SDL.h)
#include <unistd.h>
#include <stdio.h>

// MAME4ALL entry and event sinks (src/circle/rpi.cpp, input.cpp)
int mame_main (int argc, char **argv);
void keyprocess (SDLKey inkey, SDL_bool pressed);
void joyprocess (Uint8 button, SDL_bool pressed, Uint8 njoy);

static void InitKeyTable (void);

CKernel *CKernel::s_pThis = 0;

#define SOUND_CHUNK	(384 * 4)
#define SOUND_QUEUE_MS	100

CKernel::CKernel (void)
:	CStdlibApp ("mame4all"),
	mTimer (&mInterrupt),
	mLogger (mOptions.GetLogLevel (), &mTimer),
	mCPUThrottle (CPUSpeedMaximum),
	mUSBHCI (&mInterrupt, &mTimer, TRUE),
	mEMMC (&mInterrupt, &mTimer, &mActLED),
	mConsole (&mSerial, &mSerial),
	m_pFrameBuffer (0),
	m_nFBWidth (0), m_nFBHeight (0), m_nOffsetX (0),
	m_nAspect (169),
	m_pSound (0),
	m_bSoundOn (FALSE),
	m_nSoundChannels (1),
	m_nLastUSB (0),
	m_pKeyboard (0),
	m_PrevMods (0)
{
	s_pThis = this;
	memset (m_PrevKeys, 0, sizeof m_PrevKeys);
	for (unsigned i = 0; i < MAX_PADS; i++)
	{
		m_pPad[i] = 0;
		m_PadButtons[i] = 0;
		m_PadAxis[i][0] = m_PadAxis[i][1] = 0;
	}
}

bool CKernel::Initialize (void)
{
	if (!CStdlibApp::Initialize ()) return false;
	if (!mSerial.Initialize (115200)) return false;
	if (!mLogger.Initialize (&mSerial)) return false;
	if (!mTimer.Initialize ()) return false;
	if (!mEMMC.Initialize ()) return false;
	if (f_mount (&mFileSystem, "SD:", 1) != FR_OK)
	{
		Fatal ("Cannot mount the SD card (FAT32 needed)");
	}
	mUSBHCI.Initialize ();
	if (!mConsole.Initialize ()) return false;
	CGlueStdioInit (mConsole);
	return true;
}

void CKernel::Fatal (const char *pMessage)
{
	mLogger.Write (GetKernelName (), LogError, "%s", pMessage);
	if (m_pFrameBuffer != 0)
	{
		delete m_pFrameBuffer;
		m_pFrameBuffer = 0;
	}
	CScreenDevice *pScreen = new CScreenDevice (mOptions.GetWidth (), mOptions.GetHeight ());
	if (pScreen->Initialize ())
	{
		CString Msg;
		Msg.Format ("\n  MAME4ALL for Raspberry Pi\n\n  %s\n", pMessage);
		pScreen->Write ((const char *) Msg, Msg.GetLength ());
	}
	for (;;)
	{
		mActLED.Blink (3, 150, 150);
		mTimer.MsDelay (1000);
	}
}

CStdlibApp::TShutdownMode CKernel::Run (void)
{
	mLogger.Write (GetKernelName (), LogNotice, "MAME4ALL (Circle), " __DATE__);

	InitKeyTable ();
	m_nAspect = mOptions.GetAppOptionDecimal ("aspect", 169) == 43 ? 43 : 169;

	if (chdir ("/mame4all") != 0)
	{
		Fatal ("Folder /mame4all not found on the SD card");
	}

	static char Arg0[] = "mame";
	static char GameName[64];
	char *argv[3] = { Arg0, 0, 0 };
	int argc = 1;
	const char *pGame = mOptions.GetAppOptionString ("game");
	if (pGame != 0 && *pGame != '\0')
	{
		strncpy (GameName, pGame, sizeof GameName - 1);
	}
	else
	{
		// alternatively the first word of /mame4all/autostart.txt
		FILE *f = fopen ("autostart.txt", "r");
		if (f != 0)
		{
			if (fscanf (f, "%63s", GameName) != 1) GameName[0] = '\0';
			fclose (f);
		}
	}
	if (GameName[0] != '\0')
	{
		argv[argc++] = GameName;
		mLogger.Write (GetKernelName (), LogNotice, "Autostart: %s", GameName);
	}

	mame_main (argc, argv);		// normally does not return (exit -> halt)

	return ShutdownHalt;
}

// ---------------------------------------------------------------- video

void CKernel::SetMode (int nWidth, int nHeight)
{
	// the GPU scales the frame buffer to the whole screen; pad it at the sides so the
	// picture keeps its shape: 4:3 for horizontal games, 3:4 for vertical ones
	unsigned nDisp = m_nAspect == 43 ? 4 * 1000 / 3 : 16 * 1000 / 9;	// x1000
	unsigned nGame = nWidth >= nHeight ? 4 * 1000 / 3 : 3 * 1000 / 4;
	unsigned nFBWidth = (unsigned) nWidth * nDisp / nGame;
	if (nFBWidth < (unsigned) nWidth) nFBWidth = nWidth;
	nFBWidth = (nFBWidth + 31) & ~31u;	// keep the pitch simple (some firmwares align it)

	if (m_pFrameBuffer != 0 && nFBWidth == m_nFBWidth && (unsigned) nHeight == m_nFBHeight)
	{
		m_nOffsetX = (nFBWidth - nWidth) / 2;
		return;
	}
	delete m_pFrameBuffer;
	m_pFrameBuffer = new CBcmFrameBuffer (nFBWidth, nHeight, 16);
	if (!m_pFrameBuffer->Initialize ())
	{
		Fatal ("Cannot set the video mode");
	}
	m_nFBWidth = nFBWidth;
	m_nFBHeight = nHeight;
	m_nOffsetX = (nFBWidth - nWidth) / 2;
	u8 *pFB = (u8 *) (uintptr) m_pFrameBuffer->GetBuffer ();
	memset (pFB, 0, m_pFrameBuffer->GetPitch () * nHeight);
	mLogger.Write (GetKernelName (), LogNotice, "Video %dx%d (frame %ux%d)", nWidth, nHeight, nFBWidth, nHeight);
}

void CKernel::Present565 (const unsigned short *pSrc, int nWidth, int nHeight)
{
	if (m_pFrameBuffer == 0) return;
	u8 *pFB = (u8 *) (uintptr) m_pFrameBuffer->GetBuffer ();
	unsigned nPitch = m_pFrameBuffer->GetPitch ();
	if ((unsigned) nHeight > m_nFBHeight) nHeight = m_nFBHeight;
	for (int y = 0; y < nHeight; y++)
	{
		memcpy (pFB + y * nPitch + m_nOffsetX * 2, pSrc + y * nWidth, nWidth * 2);
	}
}

void CKernel::Present8 (const unsigned char *pSrc, const unsigned short *pPal, int nWidth, int nHeight)
{
	if (m_pFrameBuffer == 0) return;
	u8 *pFB = (u8 *) (uintptr) m_pFrameBuffer->GetBuffer ();
	unsigned nPitch = m_pFrameBuffer->GetPitch ();
	if ((unsigned) nHeight > m_nFBHeight) nHeight = m_nFBHeight;
	for (int y = 0; y < nHeight; y++)
	{
		u16 *pDst = (u16 *) (pFB + y * nPitch) + m_nOffsetX;
		const unsigned char *s = pSrc + y * nWidth;
		for (int x = 0; x < nWidth; x++)
		{
			pDst[x] = pPal[s[x]];
		}
	}
}

void CKernel::WaitVSync (void)
{
	if (m_pFrameBuffer != 0)
	{
		m_pFrameBuffer->WaitForVerticalSync ();
	}
}

// ---------------------------------------------------------------- sound

void CKernel::SoundStart (int nRate, int bStereo)
{
	// the sound device is created once and kept: deleting it touches HDMI hardware,
	// and the sample rate stays the same for all games (mame.cfg samplerate)
	m_nSoundChannels = bStereo ? 2 : 1;
	if (m_pSound == 0)
	{
		const char *pDev = mOptions.GetSoundDevice ();
		if (strcmp (pDev, "sndnone") == 0)
		{
			return;
		}
		if (strcmp (pDev, "sndpwm") == 0)
		{
			m_pSound = new CPWMSoundBaseDevice (&mInterrupt, nRate, SOUND_CHUNK);
		}
		else
		{
			m_pSound = new CHDMISoundBaseDevice (&mInterrupt, nRate, SOUND_CHUNK);
		}
		if (m_pSound->AllocateQueue (SOUND_QUEUE_MS))
		{
			m_pSound->SetWriteFormat (SoundFormatSigned16, 2);
			m_bSoundOn = m_pSound->Start ();
		}
		mLogger.Write (GetKernelName (), LogNotice, "Sound %d Hz: %s", nRate, m_bSoundOn ? "on" : "off");
	}
}

void CKernel::SoundStop (void)
{
	// keep the device running; the queue simply runs empty (silence) between games
}

void CKernel::SoundWrite (const short *pSamples, int nFrames)
{
	if (!m_bSoundOn) return;
	static short Stereo[4096 * 2];
	if (nFrames > 4096) nFrames = 4096;
	const short *pOut = pSamples;
	if (m_nSoundChannels == 1)
	{
		for (int i = 0; i < nFrames; i++)
		{
			Stereo[2 * i] = Stereo[2 * i + 1] = pSamples[i];
		}
		pOut = Stereo;
	}
	m_pSound->Write (pOut, nFrames * 2 * sizeof (short));
}

// ---------------------------------------------------------------- input

// USB HID usage -> SDL 1.2 key code
static const unsigned short HIDPairs[][2] = {
	{0x04, SDLK_a}, {0x05, SDLK_b}, {0x06, SDLK_c}, {0x07, SDLK_d},
	{0x08, SDLK_e}, {0x09, SDLK_f}, {0x0A, SDLK_g}, {0x0B, SDLK_h},
	{0x0C, SDLK_i}, {0x0D, SDLK_j}, {0x0E, SDLK_k}, {0x0F, SDLK_l},
	{0x10, SDLK_m}, {0x11, SDLK_n}, {0x12, SDLK_o}, {0x13, SDLK_p},
	{0x14, SDLK_q}, {0x15, SDLK_r}, {0x16, SDLK_s}, {0x17, SDLK_t},
	{0x18, SDLK_u}, {0x19, SDLK_v}, {0x1A, SDLK_w}, {0x1B, SDLK_x},
	{0x1C, SDLK_y}, {0x1D, SDLK_z}, {0x1E, SDLK_1}, {0x1F, SDLK_2},
	{0x20, SDLK_3}, {0x21, SDLK_4}, {0x22, SDLK_5}, {0x23, SDLK_6},
	{0x24, SDLK_7}, {0x25, SDLK_8}, {0x26, SDLK_9}, {0x27, SDLK_0},
	{0x28, SDLK_RETURN}, {0x29, SDLK_ESCAPE}, {0x2A, SDLK_BACKSPACE}, {0x2B, SDLK_TAB},
	{0x2C, SDLK_SPACE}, {0x2D, SDLK_MINUS}, {0x2E, SDLK_EQUALS}, {0x2F, SDLK_LEFTBRACKET},
	{0x30, SDLK_RIGHTBRACKET}, {0x31, SDLK_BACKSLASH}, {0x33, SDLK_SEMICOLON}, {0x34, SDLK_QUOTE},
	{0x35, SDLK_BACKQUOTE}, {0x36, SDLK_COMMA}, {0x37, SDLK_PERIOD}, {0x38, SDLK_SLASH},
	{0x39, SDLK_CAPSLOCK}, {0x3A, SDLK_F1}, {0x3B, SDLK_F2}, {0x3C, SDLK_F3},
	{0x3D, SDLK_F4}, {0x3E, SDLK_F5}, {0x3F, SDLK_F6}, {0x40, SDLK_F7},
	{0x41, SDLK_F8}, {0x42, SDLK_F9}, {0x43, SDLK_F10}, {0x44, SDLK_F11},
	{0x45, SDLK_F12}, {0x47, SDLK_SCROLLOCK}, {0x48, SDLK_PAUSE}, {0x49, SDLK_INSERT},
	{0x4A, SDLK_HOME}, {0x4B, SDLK_PAGEUP}, {0x4C, SDLK_DELETE}, {0x4D, SDLK_END},
	{0x4E, SDLK_PAGEDOWN}, {0x4F, SDLK_RIGHT}, {0x50, SDLK_LEFT}, {0x51, SDLK_DOWN},
	{0x52, SDLK_UP}, {0x53, SDLK_NUMLOCK}, {0x54, SDLK_KP_DIVIDE}, {0x55, SDLK_KP_MULTIPLY},
	{0x56, SDLK_KP_MINUS}, {0x57, SDLK_KP_PLUS}, {0x58, SDLK_KP_ENTER}, {0x59, SDLK_KP1},
	{0x5A, SDLK_KP2}, {0x5B, SDLK_KP3}, {0x5C, SDLK_KP4}, {0x5D, SDLK_KP5},
	{0x5E, SDLK_KP6}, {0x5F, SDLK_KP7}, {0x60, SDLK_KP8}, {0x61, SDLK_KP9},
	{0x62, SDLK_KP0}, {0x63, SDLK_KP_PERIOD},
};
static unsigned short HIDToSDL[256];

static void InitKeyTable (void)
{
	for (unsigned i = 0; i < sizeof HIDPairs / sizeof HIDPairs[0]; i++)
	{
		HIDToSDL[HIDPairs[i][0]] = HIDPairs[i][1];
	}
}

static const SDLKey ModToSDL[8] = {
	SDLK_LCTRL, SDLK_LSHIFT, SDLK_LALT, SDLK_UNKNOWN,
	SDLK_RCTRL, SDLK_RSHIFT, SDLK_RALT, SDLK_UNKNOWN
};

void CKernel::KeyStatusHandlerRaw (unsigned char ucModifiers, const unsigned char RawKeys[6])
{
	CKernel *p = s_pThis;
	// releases
	for (unsigned i = 0; i < 6; i++)
	{
		u8 k = p->m_PrevKeys[i];
		if (k == 0) continue;
		boolean bStill = FALSE;
		for (unsigned j = 0; j < 6; j++) if (RawKeys[j] == k) bStill = TRUE;
		if (!bStill && HIDToSDL[k]) keyprocess ((SDLKey) HIDToSDL[k], SDL_FALSE);
	}
	// presses
	for (unsigned i = 0; i < 6; i++)
	{
		u8 k = RawKeys[i];
		if (k == 0) continue;
		boolean bWas = FALSE;
		for (unsigned j = 0; j < 6; j++) if (p->m_PrevKeys[j] == k) bWas = TRUE;
		if (!bWas && HIDToSDL[k]) keyprocess ((SDLKey) HIDToSDL[k], SDL_TRUE);
	}
	u8 ucChanged = ucModifiers ^ p->m_PrevMods;
	for (unsigned b = 0; b < 8; b++)
	{
		if ((ucChanged & (1 << b)) && ModToSDL[b] != SDLK_UNKNOWN)
		{
			keyprocess (ModToSDL[b], (ucModifiers & (1 << b)) ? SDL_TRUE : SDL_FALSE);
		}
	}
	memcpy (p->m_PrevKeys, RawKeys, 6);
	p->m_PrevMods = ucModifiers;
}

static int NormAxis (const TGamePadState *pState, int i)
{
	int nMin = pState->axes[i].minimum, nMax = pState->axes[i].maximum;
	if (nMax <= nMin) return 0;
	return (pState->axes[i].value - nMin) * 65535 / (nMax - nMin) - 32768;
}

void CKernel::GamePadStatusHandler (unsigned nDeviceIndex, const TGamePadState *pState)
{
	CKernel *p = s_pThis;
	if (nDeviceIndex >= MAX_PADS || p->m_pPad[nDeviceIndex] == 0) return;
	unsigned nProps = p->m_pPad[nDeviceIndex]->GetProperties ();
	unsigned b = pState->buttons;
	unsigned nButtons = 0;
	int nX = 0, nY = 0;

	if (nProps & GamePadPropertyIsKnown)
	{
		// standard layout: A B X Y LB RB Select Start = buttons 0..7 (mame.cfg J_A=0, J_SELECT=6, J_START=7)
		static const unsigned Map[8] = {
			GamePadButtonA, GamePadButtonB, GamePadButtonX, GamePadButtonY,
			GamePadButtonLB, GamePadButtonRB, GamePadButtonSelect, GamePadButtonStart };
		for (unsigned i = 0; i < 8; i++) if (b & Map[i]) nButtons |= 1u << i;
		if (b & GamePadButtonLT) nButtons |= 1u << 8;
		if (b & GamePadButtonRT) nButtons |= 1u << 9;
		if (b & GamePadButtonLeft)  nX = -32768;
		if (b & GamePadButtonRight) nX = 32767;
		if (b & GamePadButtonUp)    nY = -32768;
		if (b & GamePadButtonDown)  nY = 32767;
	}
	else
	{
		nButtons = b & 0xFFFF;
	}
	if (nX == 0 && pState->naxes > 0) nX = NormAxis (pState, 0);
	if (nY == 0 && pState->naxes > 1) nY = NormAxis (pState, 1);
	if (pState->nhats > 0 && pState->hats[0] >= 0 && pState->hats[0] < 8)
	{
		static const int hx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 }, hy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
		if (hx[pState->hats[0]]) nX = hx[pState->hats[0]] * 32767;
		if (hy[pState->hats[0]]) nY = hy[pState->hats[0]] * 32767;
	}
	p->m_PadAxis[nDeviceIndex][0] = nX;
	p->m_PadAxis[nDeviceIndex][1] = nY;

	unsigned nChanged = nButtons ^ p->m_PadButtons[nDeviceIndex];
	for (unsigned i = 0; i < 16; i++)
	{
		if (nChanged & (1u << i))
		{
			joyprocess (i, (nButtons & (1u << i)) ? SDL_TRUE : SDL_FALSE, nDeviceIndex);
		}
	}
	p->m_PadButtons[nDeviceIndex] = nButtons;
}

void CKernel::KeyboardRemovedHandler (CDevice *pDevice, void *pContext)
{
	CKernel *p = (CKernel *) pContext;
	p->m_pKeyboard = 0;
}

void CKernel::GamePadRemovedHandler (CDevice *pDevice, void *pContext)
{
	CKernel *p = (CKernel *) pContext;
	for (unsigned i = 0; i < MAX_PADS; i++)
	{
		if (p->m_pPad[i] == (CUSBGamePadDevice *) pDevice)
		{
			p->m_pPad[i] = 0;
			p->m_PadButtons[i] = 0;
			p->m_PadAxis[i][0] = p->m_PadAxis[i][1] = 0;
		}
	}
}

void CKernel::PollInput (void)
{
	unsigned nNow = mTimer.GetTicks ();
	if (nNow - m_nLastUSB < HZ / 4)
	{
		return;
	}
	m_nLastUSB = nNow;
	if (!mUSBHCI.UpdatePlugAndPlay ())
	{
		return;
	}
	if (m_pKeyboard == 0)
	{
		m_pKeyboard = (CUSBKeyboardDevice *) mDeviceNameService.GetDevice ("ukbd1", FALSE);
		if (m_pKeyboard != 0)
		{
			m_pKeyboard->RegisterRemovedHandler (KeyboardRemovedHandler, this);
			m_pKeyboard->RegisterKeyStatusHandlerRaw (KeyStatusHandlerRaw);
		}
	}
	for (unsigned i = 0; i < MAX_PADS; i++)
	{
		if (m_pPad[i] != 0) continue;
		m_pPad[i] = (CUSBGamePadDevice *) mDeviceNameService.GetDevice ("upad", i + 1, FALSE);
		if (m_pPad[i] != 0)
		{
			m_pPad[i]->RegisterRemovedHandler (GamePadRemovedHandler, this);
			m_pPad[i]->RegisterStatusHandler (GamePadStatusHandler);
		}
	}
}

int CKernel::JoyAxis (int nJoy, int nAxis)
{
	if (nJoy < 0 || nJoy >= MAX_PADS || nAxis < 0 || nAxis > 1) return 0;
	return m_PadAxis[nJoy][nAxis];
}

// ---------------------------------------------------------------- C glue for the OSD layer

extern "C" {

void cg_fb_set_mode (int width, int height)	{ CKernel::Get ()->SetMode (width, height); }
void cg_fb_present (const unsigned short *p, int w, int h)	{ CKernel::Get ()->Present565 (p, w, h); }
void cg_fb_present8 (const unsigned char *p, const unsigned short *pal, int w, int h)
						{ CKernel::Get ()->Present8 (p, pal, w, h); }
void cg_wait_vsync (void)			{ CKernel::Get ()->WaitVSync (); }
unsigned long cg_ticks_us (void)		{ return CTimer::GetClockTicks (); }
void cg_poll_input (void)			{ CKernel::Get ()->PollInput (); }
void cg_sound_start (int rate, int stereo)	{ CKernel::Get ()->SoundStart (rate, stereo); }
void cg_sound_stop (void)			{ CKernel::Get ()->SoundStop (); }
int cg_sound_free_frames (void)			{ return 0; }
void cg_sound_write (const short *s, int frames)	{ CKernel::Get ()->SoundWrite (s, frames); }
int cg_joy_axis (int joy, int axis)		{ return CKernel::Get ()->JoyAxis (joy, axis); }

int usleep (useconds_t us)
{
	CTimer::SimpleusDelay (us);
	return 0;
}

unsigned sleep (unsigned s)
{
	CTimer::SimpleMsDelay (s * 1000);
	return 0;
}

void sync (void)
{
}

}
