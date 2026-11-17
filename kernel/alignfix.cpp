//
// alignfix.cpp - emulation of unaligned LDRD/STRD/LDM/STM (see alignfix.S)
//
#include "alignfix.h"
#include <circle/exceptionstub.h>
#include <circle/synchronize.h>
#include <circle/types.h>

extern "C" void AlignFixStub (void);

volatile unsigned g_nAlignFixups = 0;
volatile unsigned g_nAlignLastPC = 0;
volatile unsigned g_nAlignLastAddr = 0;

static inline u32 Read32 (u32 nAddress)
{
	const volatile u8 *p = (const volatile u8 *) nAddress;
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32) p[3] << 24);
}

static inline void Write32 (u32 nAddress, u32 nValue)
{
	volatile u8 *p = (volatile u8 *) nAddress;
	p[0] = nValue; p[1] = nValue >> 8; p[2] = nValue >> 16; p[3] = nValue >> 24;
}

// regs[0..15]: r0-r12, sp, lr, pc (of the aborted instruction)
extern "C" int AlignFixup (u32 *regs, u32 nSPSR)
{
	if ((nSPSR & 0x1F) != 0x1F && (nSPSR & 0x1F) != 0x10)	// only system/user mode code
	{
		return 1;
	}
	if (nSPSR & 0x20)				// Thumb
	{
		return 1;
	}

	u32 nFSR, nFAR;
	asm volatile ("mrc p15, 0, %0, c5, c0, 0" : "=r" (nFSR));
	asm volatile ("mrc p15, 0, %0, c6, c0, 0" : "=r" (nFAR));

	u32 nStatus = (nFSR & 0xF) | ((nFSR >> 6) & 0x10);
	if (nStatus != 0x01 && nStatus != 0x03)		// alignment fault?
	{
		return 1;
	}

	u32 nPC = regs[15];
	u32 nInstr = *(const u32 *) nPC;

	// base register value as the instruction sees it
	#define REG(n)	((n) == 15 ? nPC + 8 : regs[n])

	if ((nInstr & 0x0E0000D0) == 0x000000D0)	// LDRD / STRD (extra load/store)
	{
		boolean bStore = (nInstr & 0x20) != 0;	// bits 6:5 = 11 STRD, 10 LDRD
		if (nInstr & (1 << 20))			// L bit set: LDRH/LDRSB/LDRSH, not handled
		{
			return 1;
		}
		unsigned nRt = (nInstr >> 12) & 0xF;
		unsigned nRn = (nInstr >> 16) & 0xF;
		if (nRt & 1 || nRt == 14)
		{
			return 1;
		}
		u32 nOffset = (nInstr & (1 << 22)) ? (((nInstr >> 4) & 0xF0) | (nInstr & 0xF)) : regs[nInstr & 0xF];
		boolean bPre = (nInstr & (1 << 24)) != 0;
		boolean bUp = (nInstr & (1 << 23)) != 0;
		boolean bWB = (nInstr & (1 << 21)) != 0;

		u32 nBase = REG (nRn);
		u32 nNew = bUp ? nBase + nOffset : nBase - nOffset;
		u32 nAddr = bPre ? nNew : nBase;

		if (bStore)
		{
			Write32 (nAddr, regs[nRt]);
			Write32 (nAddr + 4, regs[nRt + 1]);
		}
		else
		{
			u32 a = Read32 (nAddr), b = Read32 (nAddr + 4);
			regs[nRt] = a;
			regs[nRt + 1] = b;
		}
		if ((!bPre || bWB) && nRn != 15)
		{
			regs[nRn] = nNew;
		}
	}
	else if ((nInstr & 0x0E000000) == 0x08000000)	// LDM / STM
	{
		if (nInstr & (1 << 22))			// S bit: user bank / exception return
		{
			return 1;
		}
		boolean bLoad = (nInstr & (1 << 20)) != 0;
		boolean bWB = (nInstr & (1 << 21)) != 0;
		boolean bUp = (nInstr & (1 << 23)) != 0;
		boolean bPre = (nInstr & (1 << 24)) != 0;
		unsigned nRn = (nInstr >> 16) & 0xF;
		u32 nList = nInstr & 0xFFFF;
		unsigned nCount = 0;
		for (unsigned i = 0; i < 16; i++) nCount += (nList >> i) & 1;
		if (nCount == 0 || nRn == 15)
		{
			return 1;
		}

		u32 nBase = regs[nRn];
		u32 nAddr;
		if (bUp) nAddr = bPre ? nBase + 4 : nBase;
		else     nAddr = bPre ? nBase - 4 * nCount : nBase - 4 * nCount + 4;
		u32 nNewBase = bUp ? nBase + 4 * nCount : nBase - 4 * nCount;

		boolean bPCLoaded = FALSE;
		u32 nNewPC = 0;
		for (unsigned i = 0; i < 16; i++)
		{
			if (!(nList & (1 << i))) continue;
			if (bLoad)
			{
				u32 v = Read32 (nAddr);
				if (i == 15) { bPCLoaded = TRUE; nNewPC = v; }
				else regs[i] = v;
			}
			else
			{
				Write32 (nAddr, i == 15 ? nPC + 8 : (i == nRn ? nBase : regs[i]));
			}
			nAddr += 4;
		}
		if (bWB && !(bLoad && (nList & (1 << nRn))))
		{
			regs[nRn] = nNewBase;
		}
		if (bPCLoaded)
		{
			if (nNewPC & 1)			// interworking to Thumb not supported
			{
				return 1;
			}
			regs[15] = nNewPC;
			g_nAlignFixups++;
			g_nAlignLastPC = nPC;
			g_nAlignLastAddr = nFAR;
			return 0;
		}
	}
	else if ((nInstr & 0x0C000000) == 0x04000000 && !(nInstr & (1 << 22)))	// LDR / STR word
	{
		if (nInstr & (1 << 25))			// register offset: only LSL #n (bit 4 set = media instr.)
		{
			if ((nInstr & 0x70) != 0)
			{
				return 1;
			}
		}
		unsigned nRt = (nInstr >> 12) & 0xF;
		unsigned nRn = (nInstr >> 16) & 0xF;
		if (nRt == 15)
		{
			return 1;
		}
		u32 nOffset = (nInstr & (1 << 25)) ? regs[nInstr & 0xF] << ((nInstr >> 7) & 0x1F) : (nInstr & 0xFFF);
		boolean bPre = (nInstr & (1 << 24)) != 0;
		boolean bUp = (nInstr & (1 << 23)) != 0;
		boolean bWB = (nInstr & (1 << 21)) != 0;
		u32 nBase = REG (nRn);
		u32 nNew = bUp ? nBase + nOffset : nBase - nOffset;
		u32 nAddr = bPre ? nNew : nBase;
		if (nInstr & (1 << 20)) regs[nRt] = Read32 (nAddr);
		else Write32 (nAddr, regs[nRt]);
		if ((!bPre || bWB) && nRn != 15) regs[nRn] = nNew;
	}
	else
	{
		return 1;				// VFP or other: not handled
	}

	regs[15] = nPC + 4;
	g_nAlignFixups++;
	g_nAlignLastPC = nPC;
	g_nAlignLastAddr = nFAR;
	return 0;
}

void AlignFixInstall (void)
{
	TExceptionTable * volatile pTable = (TExceptionTable * volatile) ARM_EXCEPTION_TABLE_BASE;
	pTable->DataAbort = ARM_OPCODE_BRANCH (ARM_DISTANCE (pTable->DataAbort, AlignFixStub));
	SyncDataAndInstructionCache ();
}
