/*
** m_alloc.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __M_ALLOC_H__
#define __M_ALLOC_H__

#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <malloc/malloc.h>
#define _msize(p)				malloc_size(p)
#elif defined(__solaris__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__sun)
#define _msize(p)				(*((size_t*)(p)-1))
#elif !defined(_WIN32)
#ifdef __FreeBSD__
#include <malloc_np.h>
#else
#include <malloc.h>
#endif
#define _msize(p)				malloc_usable_size(p)	// from glibc/FreeBSD
#endif

// These are the same as the same stdlib functions,
// except they bomb out with a fatal error
// when they can't get the memory.

#if defined(_DEBUG)
#define M_Calloc(s,t)		M_Calloc_Dbg(s, t, __FILE__, __LINE__)
#define M_Malloc(s)		M_Malloc_Dbg(s, __FILE__, __LINE__)
#define M_Realloc(p,s)	M_Realloc_Dbg(p, s, __FILE__, __LINE__)

void *M_Malloc_Dbg (size_t size, const char *file, int lineno);
void *M_Realloc_Dbg (void *memblock, size_t size, const char *file, int lineno);
inline void* M_Calloc_Dbg(size_t v1, size_t v2, const char* file, int lineno)
{
	auto p = M_Malloc_Dbg(v1 * v2, file, lineno);
	memset(p, 0, v1 * v2);
	return p;
}

#else
void *M_Malloc (size_t size);
void *M_Realloc (void *memblock, size_t size);
inline void* M_Calloc(size_t v1, size_t v2)
{
	auto p = M_Malloc(v1 * v2);
	memset(p, 0, v1 * v2);
	return p;
}

#endif


void M_Free (void *memblock);

#endif //__M_ALLOC_H__
