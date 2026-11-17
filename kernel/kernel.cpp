//
// kernel.cpp - MAME4ALL for Raspberry Pi Zero / Pi 1 without Linux (Circle + circle-stdlib)
//
// Data folder on the SD card: /mame4all (mame.cfg, roms/, nvram/, hi/, cfg/, skins/ ...)
// cmdline.txt options:  game=<name>   start this game directly (no menu)
//                       aspect=169|43 shape of the screen (default 169)
//                       sounddev=sndpwm|sndhdmi|sndnone
//
#include "kernel.h"
#include "alignfix.h"
#include "bigheap.h"
#include <circle/memory.h>
#include <circle/chargenerator.h>
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
	mTee (&mSerial),
	mTimer (&mInterrupt),
	mLogger (mOptions.GetLogLevel (), &mTimer),
	mCPUThrottle (CPUSpeedMaximum),
	mUSBHCI (&mInterrupt, &mTimer, TRUE),
	mEMMC (&mInterrupt, &mTimer, &mActLED),
	mConsole (&mSerial, &mTee),
	m_pFrameBuffer (0),
	m_nFBWidth (0), m_nFBHeight (0), m_nOffsetX (0),
	m_nAspect (169),
	m_pSound (0),
	m_bSoundOn (FALSE),
	m_nSoundChannels (1),
	m_nLastUSB (0),
	m_pKeyboard (0),
	m_PrevMods (0),
	m_nHeartbeat (0), m_nWatchBeat (0), m_nWatchTicks (0),
	m_bHangShown (FALSE), m_nPresented (0), m_nLoggedFixups (0)
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
	if (!mLogger.Initialize (&mTee)) return false;
	mLogger.RegisterPanicHandler (PanicHandler);
	AlignFixInstall ();
	if (!mTimer.Initialize ()) return false;
	if (!mEMMC.Initialize ()) return false;
	if (f_mount (&mFileSystem, "SD:", 1) != FR_OK)
	{
		Fatal ("Cannot mount the SD card (FAT32 needed)");
	}
	mUSBHCI.Initialize ();
	mTimer.RegisterPeriodicHandler (WatchdogHandler);
	BigHeapInit ();
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
	// self tests for the diagnostics (cmdline.txt aligntest=1 / crashtest=1, or that word in autostart.txt)
	if (mOptions.GetAppOptionDecimal ("aligntest", 0) == 1 || strcmp (GameName, "aligntest") == 0)
	{
		static u8 Buf[32] __attribute__ ((aligned (8)));
		for (unsigned i = 0; i < sizeof Buf; i++) Buf[i] = i;
		register u32 r2 asm ("r2"), r3 asm ("r3");
		u8 *pAddr = Buf + 1;
		asm volatile ("ldrd r2, r3, [%2]" : "=r" (r2), "=r" (r3) : "r" (pAddr) : "memory");
		u32 a = r2, b = r3;
		u32 c = 0, d = 0, *pAddr2 = (u32 *) (Buf + 6);
		asm volatile ("ldmia %2, {%0, %1}" : "=r" (c), "=r" (d) : "r" (pAddr2) : "memory");
		mLogger.Write (GetKernelName (), LogNotice, "aligntest: ldrd %08X %08X, ldm %08X %08X, fixups %u",
			a, b, c, d, g_nAlignFixups);
	}
	if (mOptions.GetAppOptionDecimal ("crashtest", 0) == 1 || strcmp (GameName, "crashtest") == 0)
	{
		mLogger.Write (GetKernelName (), LogNotice, "crashtest: writing to an invalid address");
		*(volatile u32 *) 0xF0000000 = 0;
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
	m_nPresented = 0;
	size_t nBigFree, nBigLargest;
	BigHeapStats (&nBigFree, &nBigLargest);
	mLogger.Write (GetKernelName (), LogNotice, "Memory: %u MB free for large blocks (largest %u MB), %u MB in Circle's heap",
		(unsigned) (nBigFree >> 20), (unsigned) (nBigLargest >> 20),
		(unsigned) (CMemorySystem::Get ()->GetHeapFreeSpace (HEAP_ANY) >> 20));
	SaveLog ();
}

void CKernel::Present565 (const unsigned short *pSrc, int nWidth, int nHeight)
{
	Heartbeat ();
	if (++m_nPresented == 120 || (m_nPresented % 600 == 0 && g_nAlignFixups != m_nLoggedFixups))
	{
		m_nLoggedFixups = g_nAlignFixups;
		mLogger.Write (GetKernelName (), LogNotice, "Running, frame %u, alignment fixups %u (last PC %08X)",
			m_nPresented, g_nAlignFixups, g_nAlignLastPC);
		if (m_nPresented == 120) SaveLog ();
	}
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
	Heartbeat ();
	if (++m_nPresented == 120 || (m_nPresented % 600 == 0 && g_nAlignFixups != m_nLoggedFixups))
	{
		m_nLoggedFixups = g_nAlignFixups;
		mLogger.Write (GetKernelName (), LogNotice, "Running, frame %u, alignment fixups %u (last PC %08X)",
			m_nPresented, g_nAlignFixups, g_nAlignLastPC);
		if (m_nPresented == 120) SaveLog ();
	}
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
		mLogger.Write (GetKernelName (), LogNotice, "Starting sound (%s, %d Hz)", mOptions.GetSoundDevice (), nRate);
		SaveLog ();
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
		SaveLog ();
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

// ---------------------------------------------------------------- diagnostics

char CTeeDevice::s_Ring[CTeeDevice::RING];

int CTeeDevice::Write (const void *pBuffer, size_t nCount)
{
	const char *p = (const char *) pBuffer;
	for (size_t i = 0; i < nCount; i++)
	{
		if (p[i] == '\x1b')		// drop colour escape sequences
		{
			while (i < nCount && p[i] != 'm') i++;
			continue;
		}
		s_Ring[m_nPos++] = p[i];
		if (m_nPos == RING) { m_nPos = 0; m_bWrapped = TRUE; }
	}
	return m_pOut->Write (pBuffer, nCount);
}

unsigned CTeeDevice::GetText (char *pDest, unsigned nMax)
{
	unsigned nLen = m_bWrapped ? RING : m_nPos;
	unsigned nStart = m_bWrapped ? m_nPos : 0;
	if (nLen > nMax) { nStart = (nStart + nLen - nMax) % RING; nLen = nMax; }
	for (unsigned i = 0; i < nLen; i++) pDest[i] = s_Ring[(nStart + i) % RING];
	return nLen;
}

static char s_LogText[CTeeDevice::RING + 1];

void CKernel::SaveLog (void)
{
	unsigned n = mTee.GetText (s_LogText, CTeeDevice::RING);
	FILE *f = fopen ("/mame4all/log.txt", "w");
	if (f != 0)
	{
		fwrite (s_LogText, 1, n, f);
		fclose (f);
	}
}

// draw the end of the log as white text on a black frame buffer
void CKernel::ShowLog (const char *pTitle, boolean bNewScreen)
{
	if (bNewScreen || m_pFrameBuffer == 0 || m_pFrameBuffer->GetWidth () < 320)
	{
		if (m_pFrameBuffer != 0) { delete m_pFrameBuffer; m_pFrameBuffer = 0; }
		m_pFrameBuffer = new CBcmFrameBuffer (640, 480, 16);
		if (!m_pFrameBuffer->Initialize ()) return;
		m_nFBWidth = 640; m_nFBHeight = 480; m_nOffsetX = 0;
	}
	unsigned nW = m_pFrameBuffer->GetWidth (), nH = m_pFrameBuffer->GetHeight ();
	unsigned nPitch = m_pFrameBuffer->GetPitch ();
	u8 *pFB = (u8 *) (uintptr) m_pFrameBuffer->GetBuffer ();
	memset (pFB, 0, nPitch * nH);

	static CCharGenerator Font;
	unsigned cw = Font.GetCharWidth (), ch = Font.GetCharHeight ();
	unsigned nCols = nW / cw, nRows = nH / ch;

	// collect the last lines: title first, then as many log lines as fit
	unsigned n = mTee.GetText (s_LogText, CTeeDevice::RING);
	s_LogText[n] = 0;
	const char *Lines[64];
	unsigned nLines = 0;
	char *p = s_LogText + n;
	unsigned nUsed = 2;
	while (p > s_LogText && nLines < 64)
	{
		if (p[-1] == '\n') *--p = 0;
		char *q = p;
		while (q > s_LogText && q[-1] != '\n') q--;
		unsigned nNeed = (strlen (q) + nCols - 1) / nCols;
		if (nNeed == 0) nNeed = 1;
		if (nUsed + nNeed > nRows) break;
		nUsed += nNeed;
		Lines[nLines++] = q;
		p = q;
	}

	unsigned y = 0;
	auto DrawLine = [&] (const char *s, u16 color)
	{
		do
		{
			if (y >= nRows) return;
			for (unsigned c = 0; c < nCols && s[c]; c++)
			{
				char chr = s[c] == '\r' || s[c] == '\t' ? ' ' : s[c];
				for (unsigned yy = 0; yy < ch; yy++)
				{
					u16 *pLine = (u16 *) (pFB + (y * ch + yy) * nPitch) + c * cw;
					for (unsigned xx = 0; xx < cw; xx++)
						pLine[xx] = Font.GetPixel (chr, xx, yy) ? color : 0;
				}
			}
			y++;
			s += strlen (s) > nCols ? nCols : strlen (s);
		}
		while (*s);
	};
	DrawLine (pTitle, 0xFFE0);
	CString Fix;
	Fix.Format ("alignment fixups: %u (last PC %08X addr %08X)", g_nAlignFixups, g_nAlignLastPC, g_nAlignLastAddr);
	DrawLine (Fix, 0x07FF);
	while (nLines > 0 && y < nRows) DrawLine (Lines[--nLines], 0xFFFF);
}

// one status line at the bottom of the current screen (ROM loading, decryption)
void CKernel::ShowStatus (const char *pText)
{
	Heartbeat ();
	mLogger.Write (GetKernelName (), LogDebug, "%s", pText);
	if (m_pFrameBuffer == 0) return;
	static CCharGenerator Font;
	unsigned cw = Font.GetCharWidth (), ch = Font.GetCharHeight ();
	unsigned nW = m_pFrameBuffer->GetWidth (), nH = m_pFrameBuffer->GetHeight ();
	if (nH < ch) return;
	unsigned nPitch = m_pFrameBuffer->GetPitch ();
	u8 *pFB = (u8 *) (uintptr) m_pFrameBuffer->GetBuffer ();
	unsigned y0 = nH - ch;
	for (unsigned yy = 0; yy < ch; yy++)
	{
		u16 *pLine = (u16 *) (pFB + (y0 + yy) * nPitch);
		for (unsigned x = 0; x < nW; x++)
		{
			unsigned c = x / cw;
			char chr = c < strlen (pText) ? pText[c] : ' ';
			pLine[x] = Font.GetPixel (chr, x % cw, yy) ? 0xFFE0 : 0x0000;
		}
	}
}

void CKernel::PanicHandler (void)
{
	CKernel *p = s_pThis;
	p->ShowLog ("MAME4ALL crashed (log: /mame4all/log.txt)", TRUE);
	EnableIRQs ();			// the SD card driver needs the timer interrupt
	p->SaveLog ();
}

// 100 Hz: if the main loop stops for 60 s, show the log on the screen
void CKernel::WatchdogHandler (void)
{
	CKernel *p = s_pThis;
	if (p == 0) return;
	if (p->m_nHeartbeat != p->m_nWatchBeat)
	{
		p->m_nWatchBeat = p->m_nHeartbeat;
		p->m_nWatchTicks = 0;
		p->m_bHangShown = FALSE;
		return;
	}
	if (++p->m_nWatchTicks == 60 * HZ && !p->m_bHangShown)
	{
		p->m_bHangShown = TRUE;
		p->ShowLog ("MAME4ALL: no response for 60 s (still loading, or hung)", FALSE);
	}
}

// ---------------------------------------------------------------- C glue for the OSD layer

extern "C" {

void cg_fb_set_mode (int width, int height)	{ CKernel::Get ()->SetMode (width, height); }
void cg_fb_present (const unsigned short *p, int w, int h)	{ CKernel::Get ()->Present565 (p, w, h); }
void cg_fb_present8 (const unsigned char *p, const unsigned short *pal, int w, int h)
						{ CKernel::Get ()->Present8 (p, pal, w, h); }
void cg_wait_vsync (void)			{ CKernel::Get ()->Heartbeat (); CKernel::Get ()->WaitVSync (); }
unsigned long cg_ticks_us (void)		{ return CTimer::GetClockTicks (); }
void cg_poll_input (void)			{ CKernel::Get ()->Heartbeat (); CKernel::Get ()->PollInput (); }
void cg_heartbeat (void)			{ CKernel::Get ()->Heartbeat (); }
void cg_status (const char *text)		{ CKernel::Get ()->ShowStatus (text); }
void cg_save_log (void)			{ CKernel::Get ()->SaveLog (); }
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
