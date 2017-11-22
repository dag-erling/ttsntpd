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

#ifndef RTCD_H_INCLUDED
#define RTCD_H_INCLUDED

extern int verbose;
extern int nothing;

#define vn(lvl, ...)							\
	do {								\
		if (verbose >= lvl) {					\
			fprintf(stderr, __VA_ARGS__);			\
			fputc('\n', stderr);				\
		}							\
	} while (0)
#define v(...) \
	vn(1, __VA_ARGS__)
#define vv(...) \
	vn(2, __VA_ARGS__)
#define vvv(...) \
	vn(3, __VA_ARGS__)

static inline struct timeval
tv_delta(struct timeval otv, struct timeval ntv)
{
	struct timeval dtv;

	/* XXX double-check sign calculations */
	dtv.tv_usec = ntv.tv_usec - otv.tv_usec;
	dtv.tv_sec = ntv.tv_sec - otv.tv_sec;
	v("delta { %+ld, %+07ld }", dtv.tv_sec, dtv.tv_usec);

	/* slower than above but easier to get right */
	long long lt, rt, dt;

	lt = 1000000LL * otv.tv_sec + otv.tv_usec;
	rt = 1000000LL * ntv.tv_sec + ntv.tv_usec;
	dt = rt - lt;
	v("lt %lld rt %lld dt %+lld", lt, rt, dt);
	dtv.tv_sec = dt / 1000000;
	dtv.tv_usec = dt % 1000000;
	v("delta { %+ld, %+07ld }", dtv.tv_sec, dtv.tv_usec);

	return (dtv);
}

#endif /* !RTCD_H_INCLUDED */
