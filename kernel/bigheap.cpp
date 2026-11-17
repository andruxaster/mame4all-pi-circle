//
// bigheap.cpp - allocator for large blocks
//
// Circle's heap keeps freed blocks in free lists of fixed sizes up to 512 KB.
// A larger block is lost when it is freed, so after a big game (Neo-Geo carts
// load up to ~100 MB of ROM regions) the memory is gone and the next game
// fails with "Unable to allocate ...". malloc() and friends are wrapped
// (ld --wrap, see Makefile): blocks above BIG_THRESHOLD come from one arena,
// taken from Circle's heap at startup and managed with an address ordered
// first-fit list that splits and merges blocks.
//
#include "bigheap.h"
#include <circle/memory.h>
#include <circle/heapallocator.h>
#include <circle/logger.h>
#include <circle/synchronize.h>
#include <circle/util.h>
#include <stddef.h>

#define BIG_THRESHOLD	(512 * 1024)		// larger blocks than Circle's biggest bucket
#define BIG_ALIGN	64
#define KEEP_FOR_CIRCLE	(48 * 1024 * 1024)	// left in Circle's heap for small blocks

extern "C" {
void *__real_malloc (size_t nSize);
void __real_free (void *pBlock);
void *__real_calloc (size_t nBlocks, size_t nSize);
void *__real_realloc (void *pBlock, size_t nSize);
void *__real_memalign (size_t nAlign, size_t nSize);
}

struct TBigBlock			// header in front of every block, BIG_ALIGN bytes
{
	TBigBlock	*pPrev, *pNext;		// address order
	size_t		nSize;			// usable bytes after the header
	unsigned	bFree;
	unsigned	nMagic;
	u8		Pad[BIG_ALIGN - 2 * sizeof (void *) - sizeof (size_t) - 2 * sizeof (unsigned)];
};

#define BIG_MAGIC	0x42494748

static u8 *s_pArena = 0, *s_pArenaEnd = 0;
static TBigBlock *s_pFirst = 0;

void BigHeapInit (void)
{
	size_t nFree = CMemorySystem::Get ()->GetHeapFreeSpace (HEAP_ANY);
	if (nFree <= KEEP_FOR_CIRCLE + 16 * 1024 * 1024)
	{
		return;
	}
	size_t nSize = (nFree - KEEP_FOR_CIRCLE) & ~(size_t) (BIG_ALIGN - 1);
	u8 *p = (u8 *) __real_malloc (nSize + BIG_ALIGN);
	if (p == 0)
	{
		return;
	}
	p = (u8 *) (((uintptr) p + BIG_ALIGN - 1) & ~(uintptr) (BIG_ALIGN - 1));
	s_pArena = p;
	s_pArenaEnd = p + nSize;
	s_pFirst = (TBigBlock *) p;
	s_pFirst->pPrev = s_pFirst->pNext = 0;
	s_pFirst->nSize = nSize - sizeof (TBigBlock);
	s_pFirst->bFree = 1;
	s_pFirst->nMagic = BIG_MAGIC;
	CLogger::Get ()->Write ("bigheap", LogNotice, "Arena of %u MB for large blocks", (unsigned) (nSize >> 20));
}

static inline boolean InArena (const void *p)
{
	return (const u8 *) p >= s_pArena && (const u8 *) p < s_pArenaEnd;
}

static void *BigAlloc (size_t nSize)
{
	nSize = (nSize + BIG_ALIGN - 1) & ~(size_t) (BIG_ALIGN - 1);

	EnterCritical ();
	for (TBigBlock *b = s_pFirst; b != 0; b = b->pNext)
	{
		if (!b->bFree || b->nSize < nSize) continue;

		if (b->nSize >= nSize + sizeof (TBigBlock) + BIG_ALIGN)	// split
		{
			TBigBlock *n = (TBigBlock *) ((u8 *) (b + 1) + nSize);
			n->pPrev = b;
			n->pNext = b->pNext;
			if (n->pNext) n->pNext->pPrev = n;
			n->nSize = b->nSize - nSize - sizeof (TBigBlock);
			n->bFree = 1;
			n->nMagic = BIG_MAGIC;
			b->pNext = n;
			b->nSize = nSize;
		}
		b->bFree = 0;
		LeaveCritical ();
		return b + 1;
	}
	LeaveCritical ();
	return 0;
}

static void BigFree (void *p)
{
	TBigBlock *b = (TBigBlock *) p - 1;
	if (b->nMagic != BIG_MAGIC || b->bFree)
	{
		return;				// not ours or freed twice: ignore
	}

	EnterCritical ();
	b->bFree = 1;
	if (b->pNext && b->pNext->bFree)	// merge with the next block
	{
		TBigBlock *n = b->pNext;
		b->nSize += sizeof (TBigBlock) + n->nSize;
		b->pNext = n->pNext;
		if (b->pNext) b->pNext->pPrev = b;
		n->nMagic = 0;
	}
	if (b->pPrev && b->pPrev->bFree)	// merge with the previous block
	{
		TBigBlock *pr = b->pPrev;
		pr->nSize += sizeof (TBigBlock) + b->nSize;
		pr->pNext = b->pNext;
		if (pr->pNext) pr->pNext->pPrev = pr;
		b->nMagic = 0;
	}
	LeaveCritical ();
}

void BigHeapStats (size_t *pFree, size_t *pLargest)
{
	size_t nFree = 0, nLargest = 0;
	EnterCritical ();
	for (TBigBlock *b = s_pFirst; b != 0; b = b->pNext)
	{
		if (!b->bFree) continue;
		nFree += b->nSize;
		if (b->nSize > nLargest) nLargest = b->nSize;
	}
	LeaveCritical ();
	*pFree = nFree;
	*pLargest = nLargest;
}

extern "C" {

void *__wrap_malloc (size_t nSize)
{
	if (nSize > BIG_THRESHOLD && s_pArena != 0)
	{
		void *p = BigAlloc (nSize);
		if (p != 0) return p;
	}
	return __real_malloc (nSize);
}

void *__wrap_memalign (size_t nAlign, size_t nSize)
{
	if (nAlign > 16 || (nSize > BIG_THRESHOLD && s_pArena != 0))	// arena blocks are 64 byte aligned
	{
		if (nAlign <= BIG_ALIGN && s_pArena != 0)
		{
			void *p = BigAlloc (nSize > BIG_THRESHOLD ? nSize : BIG_THRESHOLD + 1);
			if (p != 0) return p;
		}
	}
	return __real_memalign (nAlign, nSize);
}

void __wrap_free (void *pBlock)
{
	if (pBlock == 0) return;
	if (InArena (pBlock))
	{
		BigFree (pBlock);
		return;
	}
	__real_free (pBlock);
}

void *__wrap_calloc (size_t nBlocks, size_t nSize)
{
	size_t nTotal = nBlocks * nSize;
	if (nTotal == 0) nTotal = 1;
	void *p = __wrap_malloc (nTotal);
	if (p != 0) memset (p, 0, nTotal);
	return p;
}

void *__wrap_realloc (void *pBlock, size_t nSize)
{
	if (pBlock == 0) return __wrap_malloc (nSize);
	if (nSize == 0) { __wrap_free (pBlock); return 0; }

	size_t nOld;
	if (InArena (pBlock))
	{
		nOld = ((TBigBlock *) pBlock - 1)->nSize;
		if (nSize <= nOld) return pBlock;
	}
	else
	{
		if (nSize <= BIG_THRESHOLD || s_pArena == 0)
		{
			return __real_realloc (pBlock, nSize);
		}
		nOld = ((THeapBlockHeader *) pBlock - 1)->nSize;	// Circle block header
		if (nSize <= nOld) return pBlock;
	}

	void *pNew = __wrap_malloc (nSize);
	if (pNew == 0) return 0;
	memcpy (pNew, pBlock, nOld < nSize ? nOld : nSize);
	__wrap_free (pBlock);
	return pNew;
}

}
