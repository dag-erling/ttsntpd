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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/time.h>
#ifdef HAVE_ADJTIMEX
#include <sys/timex.h>
#endif

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "rtcd.h"

#include "tod.h"
#include "zutil.h"

#if defined(HAVE_ADJTIME) || defined(HAVE_ADJTIMEX)
#define CAN_SLEW 1
#else
#define CAN_SLEW 0
#endif

/*
 * Some terminology used in this file:
 *
 *  - true time: the time provided by the caller, presumably obtained from
 *    an NTP server, a GPS receiver, or some other means.
 *
 *  - kernel time: the time reported by gettimeofday().
 *
 *  - delta: difference between true time and kernel time; positive if
 *    true time is ahead of kernel time, negative otherwise.
 *
 * In the code below, true time may be represented as a struct timeval
 * named rtv, a struct timespec named rts, or a long long named rt.
 * Similarily, kernel time is represented as ltv, lts or lt, and delta as
 * dtv, dts or dt.  The 'r' and 'l' refer to 'remote' and 'local' time,
 * respectively, since the true time is assumed to originate from an
 * external source; 'd', obviously, is short for 'delta'.
 */

/*
 * In order to do something more meaningful with the kernel clock than
 * simply step it every so often, which would seriously aggravate code
 * that relies on gettimeofday() for precise measurements of elapsed time
 * (they should use clock_gettime() instead, but very few developers are
 * even aware of it), we must keep a certain amount of state.
 *
 * The most important information we need to keep track of is the true
 * time at which we last stepped the kernel clock, which is in effect the
 * true time of the moment when we last knew that kernel time and true
 * time were in synch (within a small delta due to system call overhead
 * etc.)  Among other things, this allows us to calculate clock drift in
 * ppm.
 *
 * We also keep track of the true time at which we last adjusted the
 * clock.  This is not quite so useful, but it does allow us to perform
 * certain sanity checks (such as verifying that the clock did not go
 * backwards)
 *
 * The low-water and high-water marks are compared to the delta between
 * the kernel's idea of the time, and the actual time reported by an
 * external reference.
 *
 * Assuming a reliable external reference and a well-disciplined software
 * clock, the delta should be a fairly constant, low value.  Therefore, if
 * the delta is below the low-water mark, the clock is not adjusted, as
 * doing so may cause more harm than good.
 *
 * The author's best guess at a reasonable value for the low-water mark is
 * 1 ms, or 1,000 µs.
 *
 * Normally, the delta should not exceed the high-water mark except at
 * boot time, or if the software clock hasn't been adjusted in a long
 * time.  In these cases, slewing the clock would take too long, so step
 * it instead.
 *
 * The author's best guess at a reasonable value for the high-water mark
 * is 1 s, or 1,000,000 µs.
 */

#define DEFAULT_LOW_WATER 1000
#define DEFAULT_HIGH_WATER 1000000

struct tod {
	long long	 last_step;
	long long	 last_adjust;
	long long	 low_water;
	long long	 high_water;
};

struct tod *
tod_open(long long low_water, long long high_water)
{
	struct tod *tod;

	tod = zalloc(sizeof *tod);
	tod->low_water = low_water ? low_water : DEFAULT_LOW_WATER;
	tod->high_water = high_water ? high_water : DEFAULT_HIGH_WATER;
	return (tod);
}

void
tod_close(struct tod *tod)
{

	zfree(tod, sizeof *tod);
}

int
tod_get(struct tod *tod, struct timeval *tv)
{

	(void)tod;
	if (gettimeofday(tv, NULL) != 0) {
		warn("gettimeofday()");
		return (-1);
	}
	return (0);
}

static int
tod_step(struct tod *tod, long long lt, long long rt)
{
	struct timeval tv = {
		.tv_sec = rt / 1000000,
		.tv_usec = rt % 1000000,
	};

	(void)lt;
	if (settimeofday(&tv, NULL) != 0) {
		warn("settimeofday()");
		return (-1);
	}
	tod->last_step = tod->last_adjust = rt;
	return (0);
}

#if CAN_SLEW
static int
tod_slew(struct tod *tod, long long lt, long long rt)
{
	long long dt = rt - lt;

#if HAVE_ADJTIMEX
	struct timex tx = {
		.offset = dt,
	};

	if (adjtimex(&tx) == -1) {
		warn("adjtimex()");
		return (-1);
	}
#elif HAVE_ADJTIME
	struct timeval tv = {
		.tv_sec = dt / 1000000,
		.tv_usec = dt % 1000000,
	};

	if (adjtime(&tv, NULL) != 0) {
		warn("adjtime()");
		return (-1);
	}
#else
#error "no adjtime() or adjtimex()"
#endif
	tod->last_adjust = rt;
	return (0);
}
#endif

int
tod_set(struct tod *tod, struct timeval *rtv)
{
	struct timeval ltv;
	long long lt, rt, dt, adt;

	if (gettimeofday(&ltv, NULL) != 0)
		err(1, "gettimeofday()");
	lt = 1000000LL * ltv.tv_sec + ltv.tv_usec;
	rt = 1000000LL * rtv->tv_sec + rtv->tv_usec;

	if (tod->last_adjust && rt < tod->last_adjust) {
		v("remote time went backwards");
		tod_step(tod, lt, rt);
		return (0);
	}

	vv("computing delta");
	dt = rt - lt;
	v("lt %lld rt %lld dt %+lld", lt, rt, dt);

	if (tod->last_step)
		v("drift %lld ppm",
		    (long long)(1000000.0 * dt / (rt - tod->last_step)));

	if (nothing)
		/* don't actually set the clock */
		return (0);

	adt = dt < 0 ? -dt : dt;

	if (adt < tod->low_water) {
		/* delta beneath low-water level, avoid flap */
		v("%llu µs < %llu µs, no update",
		    adt, tod->low_water);
		return (0);
	}

	if (adt > tod->high_water) {
		v("%llu µs > %llu µs, stepping software clock",
		    adt, tod->high_water);
		tod_step(tod, lt, rt);
		return (0);
	}

#ifdef CAN_SLEW
	v("%llu µs < %llu µs < %llu µs, slewing software clock",
	    tod->low_water, adt, tod->high_water);
	tod_slew(tod, lt, lt);
#else
	v("unable to slew, stepping software clock");
	tod_step(tod, lt, rt);
#endif
	return (0);
}
