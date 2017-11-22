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

#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "rtcd.h"

#include "rtc.h"
#include "sntp.h"
#include "tod.h"
#include "zutil.h"

static struct sntp *sntp;
static const char *sntp_dstaddr;
static const char *sntp_dstport;
static const char *sntp_srcaddr;
static const char *sntp_srcport;
static int sntp_timeout = 16000;

static int init_from_rtc = 0;
static int quit_after_init = 0;

static long long tod_low_water;
static long long tod_high_water;

static struct rtc *rtc;
static const char *rtc_device = "/dev/rtc0";

static struct tod *tod;

int nothing;
int verbose;

/*
 * Send an SNTP query and wait for a response.
 *
 * tv is where the received time will be stored
 * timeout is how long to wait
 */
static int
rtcd_query(struct timeval *tv, int timeout)
{
	struct ntptime nt;
	int ret;

	vv("sending request to %s:%s", sntp_dstaddr, sntp_dstport);
	if (sntp_send(sntp) == SNTP_OK) {
		vv("waiting for response...");
		while ((ret = sntp_poll(sntp, timeout)) == SNTP_NORESP)
			/* nothing */ ;
		if (ret == SNTP_OK) {
			vv("processing respone");
			if (sntp_recv(sntp, &nt) == SNTP_OK) {
				nt2tv(&nt, tv);
				v("got time %lu.%06lu",
				    tv->tv_sec, tv->tv_usec);
				return (0);
			} else {
				warn("sntp_recv()");
			}
		} else {
			warn("sntp_poll()");
		}
	} else {
		warn("sntp_send()");
	}
	return (-1);
}

static void
rtcd(void)
{
	struct timeval tv;

	for (;;) {
		if (rtcd_query(&tv, sntp_timeout) == 0) {
			if (!nothing) {
				v("setting time-of-day clock");
				tod_set(tod, &tv);
				v("setting hardware clock");
				rtc_set(rtc, &tv);
			}
		}
		vv("sleeping");
		sleep(13 * 60);
	}
}

static void
rtcd_init(void)
{
	struct timeval tv;

	if (sntp_dstaddr) {
		sntp = sntp_create(sntp_dstaddr, sntp_dstport,
		    sntp_srcaddr, sntp_srcport);
		if (sntp == NULL)
			err(1, "sntp_create()");
	}

	if (!nothing)
		if ((rtc = rtc_open(rtc_device)) == NULL)
			err(1, "rtc_open()");

	if (!nothing)
		if ((tod = tod_open(tod_low_water, tod_high_water)) == NULL)
			err(1, "tod_open()");

	if (init_from_rtc) {
		v("initializing time-of-day clock from hardware clock");
		if (rtc_get(rtc, &tv) == 0 && !nothing)
			tod_set(tod, &tv);
	}
}

static void
usage(void)
{

	fprintf(stderr, "usage: rtcd [-inqv] "
	    "[-d device] [-l low_water] [-h high_water] "
	    "[-a srcaddr] [-s srcport] [-p dstport] [server] "
	    "\n");
	exit(1);
}

static long long
ll_optarg(const char *optarg)
{
	long long ll;
	char *end;

	ll = strtoll(optarg, &end, 10);
	if (end == optarg || *end != '\0')
		usage();
	return (ll);
}

int
main(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "a:d:h:il:np:qs:v")) != -1)
		switch (opt) {
		case 'a':
			sntp_srcaddr = optarg;
		case 'd':
			rtc_device = optarg;
			break;
		case 'h':
			tod_high_water = ll_optarg(optarg);
			if (tod_high_water < 0)
				usage();
			break;
		case 'i':
			++init_from_rtc;
			break;
		case 'l':
			tod_low_water = ll_optarg(optarg);
			if (tod_low_water < 0)
				usage();
			break;
		case 'n':
			nothing++;
			break;
		case 'p':
			sntp_dstport = optarg;
			break;
		case 'q':
			++quit_after_init;
			break;
		case 's':
			sntp_srcport = optarg;
			break;
		case 'v':
			++verbose;
			break;
		default:
			usage();
			break;
		}

	argc -= optind;
	argv += optind;

	if (argc) {
		sntp_dstaddr = *argv;
		argc--;
		argv++;
	}

	if (argc)
		usage();

	if (tod_low_water > tod_high_water)
		usage();

	if (sntp_dstaddr == NULL && !(init_from_rtc && quit_after_init)) {
		fprintf(stderr, "no server specified\n");
		exit(1);
	}

	rtcd_init();

	if (quit_after_init)
		exit(0);

	rtcd(); /* should never return */

	exit(1);
}
