// bigheap.h - allocator for large blocks (see bigheap.cpp)
#ifndef _bigheap_h
#define _bigheap_h

#include <stddef.h>

void BigHeapInit (void);
void BigHeapStats (size_t *pFree, size_t *pLargest);

#endif
