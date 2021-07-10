/*
** nodebuild_events.cpp
**
** A red-black tree for keeping track of segs that get touched by a splitter.
**
**---------------------------------------------------------------------------
** Copyright 2002-2006 Randy Heit
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
** 4. When not used as part of ZDoom or a ZDoom derivative, this code will be
**    covered by the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or (at
**    your option) any later version.
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

#include <string.h>
#include "doomtype.h"
#include "nodebuild.h"

FEventTree::FEventTree ()
: Root (&Nil), Spare (NULL)
{
	memset (&Nil, 0, sizeof(Nil));
}

FEventTree::~FEventTree ()
{
	FEvent *probe;

	DeleteAll ();
	probe = Spare;
	while (probe != NULL)
	{
		FEvent *next = probe->Left;
		delete probe;
		probe = next;
	}
}

void FEventTree::DeleteAll ()
{
	DeletionTraverser (Root);
	Root = &Nil;
}

void FEventTree::DeletionTraverser (FEvent *node)
{
	if (node != &Nil && node != NULL)
	{
		DeletionTraverser (node->Left);
		DeletionTraverser (node->Right);
		node->Left = Spare;
		Spare = node;
	}
}

FEvent *FEventTree::GetNewNode ()
{
	FEvent *node;

	if (Spare != NULL)
	{
		node = Spare;
		Spare = node->Left;
	}
	else
	{
		node = new FEvent;
	}
	return node;
}

void FEventTree::Insert (FEvent *z)
{
	FEvent *y = &Nil;
	FEvent *x = Root;

	while (x != &Nil)
	{
		y = x;
		if (z->Distance < x->Distance)
		{
			x = x->Left;
		}
		else
		{
			x = x->Right;
		}
	}
	z->Parent = y;
	if (y == &Nil)
	{
		Root = z;
	}
	else if (z->Distance < y->Distance)
	{
		y->Left = z;
	}
	else
	{
		y->Right = z;
	}
	z->Left = &Nil;
	z->Right = &Nil;
}

FEvent *FEventTree::Successor (FEvent *event) const
{
	if (event->Right != &Nil)
	{
		event = event->Right;
		while (event->Left != &Nil)
		{
			event = event->Left;
		}
		return event;
	}
	else
	{
		FEvent *y = event->Parent;
		while (y != &Nil && event == y->Right)
		{
			event = y;
			y = y->Parent;
		}
		return y;
	}
}

FEvent *FEventTree::Predecessor (FEvent *event) const
{
	if (event->Left != &Nil)
	{
		event = event->Left;
		while (event->Right != &Nil)
		{
			event = event->Right;
		}
		return event;
	}
	else
	{
		FEvent *y = event->Parent;
		while (y != &Nil && event == y->Left)
		{
			event = y;
			y = y->Parent;
		}
		return y;
	}
}

FEvent *FEventTree::FindEvent (double key) const
{
	FEvent *node = Root;

	while (node != &Nil)
	{
		if (node->Distance == key)
		{
			return node;
		}
		else if (node->Distance > key)
		{
			node = node->Left;
		}
		else
		{
			node = node->Right;
		}
	}
	return NULL;
}

FEvent *FEventTree::GetMinimum ()
{
	FEvent *node = Root;

	if (node == &Nil)
	{
		return NULL;
	}
	while (node->Left != &Nil)
	{
		node = node->Left;
	}
	return node;
}

void FEventTree::PrintTree (const FEvent *event) const
{
	// Use the CRT's sprintf so that it shares the same formatting as ZDBSP's output.
	char buff[100];
	if (event != &Nil)
	{
		PrintTree(event->Left);
		sprintf(buff, " Distance %g, vertex %d, seg %u\n",
			g_sqrt(event->Distance/4294967296.0), event->Info.Vertex, (unsigned)event->Info.FrontSeg);
		Printf(PRINT_DEFAULT, PRINTF_LOGONLY, "%s", buff);
		PrintTree(event->Right);
	}
}
