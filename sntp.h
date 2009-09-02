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

#ifndef SNTP_H_INCLUDED
#define SNTP_H_INCLUDED

struct sntp;

/*
 * Structure of an NTP timestamp
 */
struct ntptime {
	uint32_t sec;
	uint32_t frac;
};

/*
 * Useful epochs, in seconds since NTP epoch
 */
#define UNIX_EPOCH	2208988800UL	/* 1970-01-01 00:00:00 UTC */
#define UTC_EPOCH	2272060800UL	/* 1972-01-01 00:00:00 UTC */

/* clear a struct ntptime */
#define nt_zero(nt)						\
	(void)((nt).sec = (nt).frac = 0)

/* comparison macros */
#define nt_cmp(nt1, op, nt2)					\
	((nt1).sec op (nt2).sec && (nt1).frac op (nt2).frac)
#define nt_lt(nt1, nt2)						\
	nt_cmp(nt1, <, nt2)
#define nt_le(nt1, nt2)						\
	nt_cmp(nt1, <=, nt2)
#define nt_eq(nt1, nt2)						\
	nt_cmp(nt1, ==, nt2)
#define nt_ge(nt1, nt2)						\
	nt_cmp(nt1, >=, nt2)
#define nt_gt(nt1, nt2)						\
	nt_cmp(nt1, >, nt2)

/*
 * Conversion functions
 */
void tv2nt(struct timeval *, struct ntptime *);
void nt2tv(struct ntptime *, struct timeval *);
void ts2nt(struct timespec *, struct ntptime *);
void nt2ts(struct ntptime *, struct timespec *);
void h2n_nt(struct ntptime *);
void n2h_nt(struct ntptime *);

/*
 * Error codes
 */
typedef enum sntp_err {
	SNTP_OK,		/* fine */
	SNTP_SYSERR,		/* check errno */
	SNTP_DNSERR,		/* dns error */
	SNTP_NOREQ,		/* no request sent */
	SNTP_NORESP,		/* no response received */
	SNTP_BADRESP,		/* invalid response received */
	SNTP_LAME,		/* server is lame / unsynchronized */
	SNTP_BACKOFF,		/* polling too frequently */
} sntp_err_t;

/*
 * SNTP client
 */
struct sntp *sntp_create(const char *, const char *, const char *, const char *);
int sntp_open(struct sntp *);
void sntp_close(struct sntp *);
void sntp_destroy(struct sntp *);
sntp_err_t sntp_send(struct sntp *);
sntp_err_t sntp_pending(struct sntp *);
sntp_err_t sntp_poll(struct sntp *, int);
sntp_err_t sntp_recv(struct sntp *, struct ntptime *);

#endif
