/*-
 * Copyright (c) 2007 TANDBERG Telecom AS
 * Copyright (c) 2008-2009 Dag-Erling Smørgrav
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
 * Author: Dag-Erling Smørgrav <des@des.no>
 *
 * $Id$
 */

#ifndef ZMEM_H_INCLUDED
#define ZMEM_H_INCLUDED

extern void *Zalloc(size_t, size_t);
extern void *Zrealloc(void *, size_t);
extern void  Zfree(void *, size_t);
extern char *Zstrdup(const char *);
extern void *Zmemdup(const void *, size_t);
extern void  Zassert(const char *, int, const char *, const char *);
extern void  Zunreach(const char *, int, const char *);

#define zalloc(l)							\
	Zalloc(l, 0)

#define zalloca(l, a)							\
	Zalloc(l, a)

#define zrealloc(p, l)							\
	Zrealloc(p, l)

#define zfree(p, l)							\
	do {								\
		Zfree(p, l);						\
		(p) = NULL;						\
	} while (0)

#define zstrdup(s)							\
	Zstrdup(s)

#define zmemdup(s, l)							\
	Zmemdup(s, l)

#define zclose(d)							\
	do {								\
		close(d);						\
		(d) = -1;						\
	} while (0)

#define zassert(cond)							\
	do {								\
		if (!(cond))						\
			Zassert(__FILE__, __LINE__, __func__, #cond);	\
	} while (0)

#define zunreach()							\
	do {								\
		Zunreach(__FILE__, __LINE__, __func__);			\
	} while (1)

#endif /* !ZMEM_H_INCLUDED */
