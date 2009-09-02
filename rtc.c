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
 * Author: Dag-Erling Smørgrav <des@des.no>
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/ioctl.h>
#include <sys/time.h>

#include <linux/rtc.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "rtc.h"
#include "zutil.h"

struct rtc {
	int		 fd;
};

/*
 * Convert a normalized struct tm to a positive struct timeval, assuming
 * UTC
 */
static void
tm2tv(const struct tm *tm, struct timeval *tv)
{

	/*
	 * Cf. IEEE Std 1003.1-2001, Base Definitions, Section 4.14,
	 * Seconds Since the Epoch
	 */
	tv->tv_sec =
	    tm->tm_sec +
	    tm->tm_min * 60 +
	    tm->tm_hour * 3600 +
	    tm->tm_yday * 86400 +
	    (tm->tm_year - 70) * 31536000 +
	    /* leap years */
	    ((tm->tm_year - 69) / 4) * 86400 -
	    /* 100-year rule */
	    ((tm->tm_year - 1) / 100) * 86400 +
	    /* 400-year rule */
	    ((tm->tm_year + 299) / 400) * 86400;
	tv->tv_usec = 0;
}

static int mdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/*
 * Convert a positive struct timeval to a normalized struct tm, assuming
 * UTC
 *
 * There is probably a more efficient way of doing this, but my primary
 * concern was correctness and verifiability.
 */
static void
tv2tm(const struct timeval *tv, struct tm *tm)
{
	time_t t;
	int d, l, m, y;

	t = tv->tv_sec;
	tm->tm_sec = t % 60;
	t /= 60;
	tm->tm_min = t % 60;
	t /= 60;
	tm->tm_hour = t % 24;
	t /= 24;
	tm->tm_wday = (t + 4) % 7;

	/* year and day of year */
	y = 1970;
	for (;;) {
		l = ((y % 4 == 0) && ((y % 100 != 0) || (y % 400 == 0)));
		d = 365 + l;
		if (t < d)
			break;
		t -= d;
		++y;
	}
	tm->tm_year = y - 1900;
	tm->tm_yday = t;

	/* month and day of month */
	m = 0;
	for (;;) {
		d = mdays[m] + (l && m == 1);
		if (t < d)
			break;
		t -= d;
		++m;
	}
	tm->tm_mon = m;
	tm->tm_mday = t + 1;

	/* unused in UTC */
	tm->tm_isdst = 0;
}

struct rtc *
rtc_open(const char *path)
{
	struct rtc *rtc;
	int serrno;

	rtc = zalloc(sizeof *rtc);
	rtc->fd = -1;
	if ((rtc->fd = open(path, O_RDWR)) < 0) {
		serrno = errno;
		warn("open()");
		rtc_close(rtc);
		errno = serrno;
		return (NULL);
	}
	return (rtc);
}

void
rtc_close(struct rtc *rtc)
{

	if (rtc->fd != -1)
		zclose(rtc->fd);
	zfree(rtc, sizeof *rtc);
}

int
rtc_get(struct rtc *rtc, struct timeval *tv)
{
	struct tm tm;
	int serrno;

	if (ioctl(rtc->fd, RTC_RD_TIME, &tm) != 0) {
		serrno = errno;
		warn("ioctl(RTC_RD_TIME)");
		errno = serrno;
		return (-1);
	}
	tm2tv(&tm, tv);
	return (0);
}

int
rtc_set(struct rtc *rtc, const struct timeval *tv)
{
	struct tm tm;
	int serrno;

	tv2tm(tv, &tm);
	if (ioctl(rtc->fd, RTC_SET_TIME, &tm) != 0) {
		serrno = errno;
		warn("ioctl(RTC_SET_TIME)");
		errno = serrno;
		return (-1);
	}
	return (0);
}

#ifdef RTC_MAIN
#include <stdio.h>

static time_t dates[] = {
	/* Unix epoch */
	0,			/* 1970-01-01 00:00:00 UTC */

	/* non-leap year */
	36633599,		/* 1971-02-28 23:59:59 UTC */
	36633600,		/* 1971-03-01 00:00:00 UTC */

	/* first leap year after Unix epoch */
	68169599,		/* 1972-02-28 23:59:59 UTC */
	68169600,		/* 1972-02-29 00:00:00 UTC */

	/* foo */
	251263800,		/* 1977-12-18 03:30:00 UTC */

	/* 30-bit boundary */
	536870911,		/* 1987-01-05 18:48:31 UTC */
	536870912,		/* 1987-01-05 18:48:32 UTC */

	/* 400-year rule: 2000 was a leap year */
	951782399,		/* 2000-02-28 23:59:59 UTC */
	951782400,		/* 2000-02-29 00:00:00 UTC */
	951868799,		/* 2000-02-29 23:59:59 UTC */
	951868800,		/* 2000-03-01 00:00:00 UTC */

	/* 31-bit boundary */
	1073741823,		/* 2004-01-10 13:37:03 UTC */
	1073741824,		/* 2004-01-10 13:37:04 UTC */

	/* greatest possible signed 32-bit value */
	2147483647,		/* 2038-01-19 03:14:07 UTC */
};

static const char *weekday[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};

int
main(void)
{
	struct timeval tv = { 0 };
	struct tm tm = { 0 };
	unsigned int i;

	for (i = 0; i < sizeof dates / sizeof *dates; ++i) {
		tv.tv_sec = dates[i];
		printf("%10lu -> ", (unsigned long)tv.tv_sec);
		tv2tm(&tv, &tm);
		printf("%s %04d-%02d-%02d %02d:%02d:%02d UTC",
		    weekday[tm.tm_wday],
		    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		    tm.tm_hour, tm.tm_min, tm.tm_sec);
		tv.tv_sec = 0;
		tm2tv(&tm, &tv);
		printf(" -> %-10lu", (unsigned long)tv.tv_sec);
		if (tv.tv_sec != dates[i])
			printf(" !");
		printf("\n");
	}

	return (0);
}
#endif
