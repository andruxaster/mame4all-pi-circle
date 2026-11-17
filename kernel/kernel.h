//
// kernel.h - MAME4ALL for Raspberry Pi Zero / Pi 1 without Linux (Circle + circle-stdlib)
//
#ifndef _kernel_h
#define _kernel_h

#include <circle_stdlib_app.h>
#include <circle/bcmframebuffer.h>
#include <circle/cputhrottle.h>
#include <circle/usb/usbkeyboard.h>
#include <circle/usb/usbgamepad.h>
#include <circle/sound/soundbasedevice.h>

#define MAX_PADS 4

// output device that copies everything (logger, printf) into a RAM ring buffer,
// so the log can be shown on screen and written to /mame4all/log.txt
class CTeeDevice : public CDevice
{
public:
	CTeeDevice (CDevice *pOut) : m_pOut (pOut), m_nPos (0), m_bWrapped (FALSE) {}
	int Write (const void *pBuffer, size_t nCount);
	unsigned GetText (char *pDest, unsigned nMax);	// oldest to newest
	enum { RING = 32768 };
private:
	CDevice *m_pOut;
	static char s_Ring[RING];	// static: CKernel lives on the stack
	unsigned m_nPos;
	boolean m_bWrapped;
};

class CKernel : public CStdlibApp
{
public:
	CKernel (void);

	bool Initialize (void);
	TShutdownMode Run (void);

	// glue for the MAME OSD layer (see the cg_* functions in kernel.cpp)
	void SetMode (int nWidth, int nHeight);
	void Present565 (const unsigned short *pSrc, int nWidth, int nHeight);
	void Present8 (const unsigned char *pSrc, const unsigned short *pPal, int nWidth, int nHeight);
	void WaitVSync (void);
	void PollInput (void);
	void SoundStart (int nRate, int bStereo);
	void SoundStop (void);
	void SoundWrite (const short *pSamples, int nFrames);
	int JoyAxis (int nJoy, int nAxis);
	void Heartbeat (void) { m_nHeartbeat++; }
	void SaveLog (void);				// /mame4all/log.txt
	void ShowStatus (const char *pText);
	void ShowLog (const char *pTitle, boolean bNewScreen);

	static CKernel *Get (void) { return s_pThis; }

private:
	static void KeyStatusHandlerRaw (unsigned char ucModifiers, const unsigned char RawKeys[6]);
	static void GamePadStatusHandler (unsigned nDeviceIndex, const TGamePadState *pState);
	static void KeyboardRemovedHandler (CDevice *pDevice, void *pContext);
	static void GamePadRemovedHandler (CDevice *pDevice, void *pContext);
	void Fatal (const char *pMessage);
	static void PanicHandler (void);
	static void WatchdogHandler (void);

private:
	CSerialDevice		mSerial;
	CTeeDevice		mTee;
	CTimer			mTimer;
	CLogger			mLogger;
	CCPUThrottle		mCPUThrottle;
	CUSBHCIDevice		mUSBHCI;
	CEMMCDevice		mEMMC;
	FATFS			mFileSystem;
	CConsole		mConsole;

	CBcmFrameBuffer		*m_pFrameBuffer;
	unsigned		m_nFBWidth, m_nFBHeight, m_nOffsetX;
	unsigned		m_nAspect;		// 169 or 43
	CSoundBaseDevice	*m_pSound;
	boolean			m_bSoundOn;
	int			m_nSoundChannels;
	unsigned		m_nLastUSB;

	CUSBKeyboardDevice * volatile m_pKeyboard;
	CUSBGamePadDevice * volatile m_pPad[MAX_PADS];
	unsigned char		m_PrevKeys[6];
	unsigned char		m_PrevMods;
	unsigned		m_PadButtons[MAX_PADS];
	int			m_PadAxis[MAX_PADS][2];

	volatile unsigned	m_nHeartbeat;
	unsigned		m_nWatchBeat, m_nWatchTicks;
	volatile boolean	m_bHangShown;
	unsigned		m_nPresented;
	unsigned		m_nLoggedFixups;

	static CKernel		*s_pThis;
};

#endif
