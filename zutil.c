/*-
 * Copyright (c) 2007 TANDBERG Telecom AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Author: Dag-Erling Smørgrav <des@linpro.no>
 *
 * $Id: zutil.c 136662 2008-02-12 15:16:24Z des $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zutil.h"

#define ZMAGIC 0x5254505a
#define ZALIGN (sizeof(void *))

struct zheader {
	uint32_t	 zmagic;
	size_t		 zlen;
	size_t		 zalign;
	void		*zptr;
	char		 zdata[];
};

/*
 * On modern systems with memory overcommit, malloc() will not fail when
 * physical memory runs out, so checking its return value is mostly
 * pointless.  However, if malloc() should return NULL, the subsequent
 * assignment to zh->zmagic will cause a segfault, just like a real
 * out-of-memory condition.
 */

void *
Zalloc(size_t len, size_t align)
{
	struct zheader *zh;
	void *zptr;

	zh = zptr = malloc(sizeof *zh + len + align);
	zassert(((uintptr_t)zh % ZALIGN) == 0);
	if (align) {
		zassert((align % ZALIGN) == 0);
		while ((uintptr_t)zh->zdata % align)
			zh = (struct zheader *)((char *)zh + ZALIGN);
	}
	zh->zmagic = ZMAGIC;
	zh->zlen = len;
	zh->zalign = align;
	zh->zptr = zptr;
	memset(zh->zdata, 0, len);
	zassert(align == 0 || ((uintptr_t)zh->zdata % align) == 0);
	return (zh->zdata);
}

void *
Zrealloc(void *ptr, size_t len)
{
	struct zheader *zh = NULL;
	size_t olen = 0;

	if (ptr) {
		zh = (struct zheader *)ptr - 1;
		zassert(zh->zmagic == ZMAGIC);
		zassert(zh->zalign == 0);
		zassert(zh->zptr == zh);
		if (len < zh->zlen)
			memset(zh->zdata + len, 0, zh->zlen - len);
		olen = zh->zlen;
	}
	zh = realloc(zh, sizeof *zh + len);
	zh->zmagic = ZMAGIC;
	if (len > olen)
		memset(zh->zdata + olen, 0, len - olen);
	zh->zlen = len;
	zh->zalign = 0;
	zh->zptr = zh;
	return (zh->zdata);
}

void
Zfree(void *ptr, size_t len)
{
	struct zheader *zh;

	if (!ptr) {
		zassert(len == 0);
		return;
	}
	zh = (struct zheader *)ptr - 1;
	zassert(zh->zmagic == ZMAGIC);
	zassert(len == 0 || len == zh->zlen);
	memset(zh->zdata, 0, zh->zlen);
	free(zh->zptr);
}

char *
Zstrdup(const char *str)
{
	char *dup;
	size_t len;

	zassert(str != NULL);
	len = strlen(str);
	dup = Zalloc(len + 1, 0);
	memcpy(dup, str, len + 1);
	return (dup);
}

void *
Zmemdup(const void *buf, size_t len)
{
	char *dup;

	zassert(buf != NULL);
	zassert(len != 0);
	dup = Zalloc(len, 0);
	memcpy(dup, buf, len);
	return (dup);
}

void
Zassert(const char *file, int line, const char *func, const char *cond)
{

	fprintf(stderr, "assertion failed in %s() on %s:%d: %s\n",
	    func, file, line, cond);
	abort();
}

void
Zunreach(const char *file, int line, const char *func)
{

	fprintf(stderr, "unreachable code reached in %s() on %s:%d\n",
	    func, file, line);
	abort();
}
