// alignfix.h - emulation of unaligned LDRD/STRD/LDM/STM in the data abort handler
#ifndef _alignfix_h
#define _alignfix_h

void AlignFixInstall (void);

extern volatile unsigned g_nAlignFixups;
extern volatile unsigned g_nAlignLastPC;
extern volatile unsigned g_nAlignLastAddr;

#endif
