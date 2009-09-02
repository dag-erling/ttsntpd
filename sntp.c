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
 * $Id: sntp.c 138420 2008-02-29 13:23:39Z des $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "sntp.h"
#include "zutil.h"

/*
 * Convert a struct timeval to an NTP timestamp
 */
void
tv2nt(struct timeval *tv, struct ntptime *nt)
{
	uint64_t frac;

	nt->sec = tv->tv_sec + UNIX_EPOCH;
	frac = tv->tv_usec;
	frac <<= 32;
	frac /= 1000 * 1000;
	nt->frac = frac;
}

/*
 * Convert an NTP timestamp to a struct timeval
 */
void
nt2tv(struct ntptime *nt, struct timeval *tv)
{
	uint64_t frac;

	tv->tv_sec = nt->sec - UNIX_EPOCH;
	frac = nt->frac;
	frac *= 1000 * 1000;
	frac >>= 32;
	tv->tv_usec = frac;
}

/*
 * Convert a struct timespec to an NTP timestamp
 */
void
ts2nt(struct timespec *ts, struct ntptime *nt)
{
	uint64_t frac;

	nt->sec = ts->tv_sec + UNIX_EPOCH;
	frac = ts->tv_nsec;
	frac <<= 32;
	frac /= 1000 * 1000 * 1000;
	nt->frac = frac;
}

/*
 * Convert an NTP timestamp to a struct timespec
 */
void
nt2ts(struct ntptime *nt, struct timespec *ts)
{
	uint64_t frac;

	ts->tv_sec = nt->sec - UNIX_EPOCH;
	frac = nt->frac;
	frac *= 1000 * 1000 * 1000;
	frac >>= 32;
	ts->tv_nsec = frac;
}

/*
 * Convert struct ntptime in-place from network to host order
 */
void
n2h_ntp(struct ntptime *nt)
{

	nt->sec = ntohl(nt->sec);
	nt->frac = ntohl(nt->frac);
}

/*
 * Convert struct ntptime in-place from host to network order
 */
void
h2n_ntp(struct ntptime *nt)
{

	nt->sec = htonl(nt->sec);
	nt->frac = htonl(nt->frac);
}


/*
 * SNTP client state
 */
struct sntp {
	/* parameters from sntp_create() */
	char		*srcaddr;
	char		*srcport;
	char		*dstaddr;
	char		*dstport;

	/* DNS data */
	int		 family;
	int		 socktype;
	int		 protocol;
	struct sockaddr	*laddr;
	socklen_t	 laddrlen;
	struct sockaddr	*raddr;
	socklen_t	 raddrlen;

	/* socket and poll structure */
	int		 sd;
	struct pollfd	 pfd;

	/* protocol state */
	struct ntptime	 last_send;
	struct ntptime	 last_recv;
};

/*
 * Initialize an SNTP client context
 *
 * Multiple contexts can coexist as long as they do not use the same
 * source port.
 *
 * TODO: add support for binding to a specific source address.
 */
struct sntp *
sntp_create(const char *dstaddr, const char *dstport,
    const char *srcaddr, const char *srcport)
{
	struct sntp *sntp;

	zassert(dstaddr != NULL);

	sntp = zalloc(sizeof *sntp, 0);
	sntp->sd = -1;

	sntp->srcaddr = srcaddr ? zstrdup(srcaddr) : NULL;
	sntp->srcport = zstrdup(srcport ? srcport : "ntp");
	sntp->dstaddr = zstrdup(dstaddr);
	sntp->dstport = zstrdup(dstport ? dstport : "ntp");

	/* good to go */
	return (sntp);
}

/*
 * Look up local and remote addresses and set up the socket
 */
int
sntp_open(struct sntp *sntp)
{
	struct addrinfo hints;
	struct addrinfo *aiv, *ai;
	int ret;

	/*
	 * TODO: even if sntp->sd != -1, we may want to close and reopen
	 * if a certain amount of time has elapsed since we last did so.
	 */
	if (sntp->sd != -1)
		return (SNTP_OK);

	/* resolve the server address */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	if ((ret = getaddrinfo(sntp->dstaddr, sntp->dstport, &hints, &aiv)) != 0)
		return (SNTP_DNSERR);
	zassert(aiv != NULL);

	/*
	 * Iterate over the results until we find one we can use.  This is
	 * sometimes necessary on systems with partial IPv6 support, where
	 * the resolver may return IPv6 addresses which the network stack
	 * can't handle.
	 */
	for (ai = aiv; ai; ai = ai->ai_next) {
		sntp->sd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sntp->sd >= 0)
			break;
	}
	if (sntp->sd == -1) {
		freeaddrinfo(aiv);
		return (SNTP_SYSERR);
	}
	sntp->raddr = zmemdup(ai->ai_addr, ai->ai_addrlen);
	sntp->raddrlen = ai->ai_addrlen;
	sntp->family = ai->ai_family;
	sntp->socktype = ai->ai_socktype;
	sntp->protocol = ai->ai_protocol;
	freeaddrinfo(aiv);

	/* get a matching local address */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = sntp->family;
	hints.ai_socktype = sntp->socktype;
	hints.ai_protocol = sntp->protocol;
	hints.ai_flags = AI_PASSIVE;
	if ((ret = getaddrinfo(sntp->srcaddr, sntp->srcport, &hints, &aiv)) != 0) {
		sntp_close(sntp);
		return (SNTP_DNSERR);
	}
	zassert(aiv != NULL);

	/* TODO: assert that results match expectations */
	sntp->laddr = zmemdup(aiv->ai_addr, aiv->ai_addrlen);
	sntp->laddrlen = aiv->ai_addrlen;
	freeaddrinfo(aiv);

	/* prepare our socket */
	if (bind(sntp->sd, sntp->laddr, sntp->laddrlen) != 0) {
		sntp_close(sntp);
		return (SNTP_SYSERR);
	}
	if (connect(sntp->sd, sntp->raddr, sntp->raddrlen) != 0) {
		sntp_close(sntp);
		return (SNTP_SYSERR);
	}

	/* prepare our pollfd */
	sntp->pfd.fd = sntp->sd;
	sntp->pfd.events = POLLIN;
	sntp->pfd.revents = 0;

	return (SNTP_OK);
}

/*
 * Destroy an SNTP client context
 *
 * This is called several times during error handling in other parts of
 * the code, so we should save and restore errno
 */
void
sntp_close(struct sntp *sntp)
{
	int serrno;

	serrno = errno;
	sntp->family = 0;
	sntp->socktype = 0;
	sntp->protocol = 0;
	if (sntp->laddr)
		zfree(sntp->laddr, sntp->laddrlen);
	sntp->laddrlen = 0;
	if (sntp->raddr)
		zfree(sntp->raddr, sntp->raddrlen);
	sntp->raddrlen = 0;
	if (sntp->sd != -1)
		zclose(sntp->sd);
	memset(&sntp->pfd, 0, sizeof sntp->pfd);
	nt_zero(sntp->last_send);
	nt_zero(sntp->last_recv);
	errno = serrno;
}

/*
 * Destroy an SNTP client context
 */
void
sntp_destroy(struct sntp *sntp)
{

	(void)sntp_close(sntp);
	if (sntp->srcaddr)
		zfree(sntp->srcaddr, 0);
	if (sntp->srcport)
		zfree(sntp->srcport, 0);
	if (sntp->dstaddr)
		zfree(sntp->dstaddr, 0);
	if (sntp->dstport)
		zfree(sntp->dstport, 0);
	zfree(sntp, sizeof *sntp);
}


/*
 * Structure of an NTP message without authenticator
 */
struct ntp_msg {
	uint8_t		 flags;
	uint8_t		 stratum;
	uint8_t		 poll;
	uint8_t		 precision;
	uint32_t	 root_delay;
	uint32_t	 root_dispersion;
	uint8_t		 reference_id[4];
	struct ntptime	 reference;
	struct ntptime	 originate;
	struct ntptime	 receive;
	struct ntptime	 transmit;
};

/*
 * Send an SNTP request
 */
sntp_err_t
sntp_send(struct sntp *sntp)
{
	struct timespec ts;
	struct ntp_msg msg;
	ssize_t ret;
	sntp_err_t se;

	if ((se = sntp_open(sntp)) != SNTP_OK)
		return (se);

	memset(&msg, 0, sizeof msg);
	msg.flags = 0x23; /* version 4, client */
	if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
		return (SNTP_SYSERR);
	ts2nt(&ts, &msg.transmit);
	h2n_ntp(&msg.transmit);
	ret = send(sntp->sd, &msg, sizeof msg, 0);
	if (ret < 0)
		return (SNTP_SYSERR);
	ts2nt(&ts, &sntp->last_send);
	return (SNTP_OK);
}

/*
 * Have we sent a request to which we're still expecting a response?
 */
sntp_err_t
sntp_pending(struct sntp *sntp)
{

	/* not currently open */
	if (sntp->sd == -1)
		return (SNTP_NOREQ);

	/* last request predates last response */
	if (nt_lt(sntp->last_send, sntp->last_recv))
		return (SNTP_NOREQ);

	/* TODO: enforce a minimum timeout */

	return (SNTP_OK);
}

/*
 * Poll for the arrival of an SNTP reply
 */
sntp_err_t
sntp_poll(struct sntp *sntp, int timeout)
{
	sntp_err_t se;

	if ((se = sntp_pending(sntp)) != SNTP_OK)
		return (se);

	switch (poll(&sntp->pfd, 1, timeout)) {
	case -1:
		sntp_close(sntp);
		return (SNTP_SYSERR);
	case 0:
		return (SNTP_NORESP);
	case 1:
		if (sntp->pfd.revents & (POLLERR|POLLHUP)) {
			sntp_close(sntp);
			return (SNTP_SYSERR);
		}
		return (SNTP_OK);
	}
	zunreach();
}

/*
 * Receive and process an SNTP reply
 */
sntp_err_t
sntp_recv(struct sntp *sntp, struct ntptime *nt)
{
	struct timespec ts;
	struct ntp_msg msg;
	sntp_err_t se;

	if ((se = sntp_pending(sntp)) != SNTP_OK)
		return (se);

	/* TODO: use recvmsg() instead */
	switch (recv(sntp->sd, &msg, sizeof msg, MSG_DONTWAIT)) {
	case -1:
		if (errno == EAGAIN)
			return (SNTP_NORESP);
		sntp_close(sntp);
		return (SNTP_SYSERR);
	case 0:
		/* can this actually occur? */
		return (SNTP_NORESP);
	case sizeof msg:
		/* good! */
		break;
	default:
		/* we got something, but bob knows what */
		return (SNTP_BADRESP);
	}

	/* record time of arrival */
	if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
		return (SNTP_SYSERR);

	/* convert to host order */
	n2h_ntp(&msg.originate);
	n2h_ntp(&msg.receive);
	n2h_ntp(&msg.transmit);

	/* look for kiss packet */
	if (msg.flags == 0xe4 && msg.stratum == 0) {
		warnx("kiss: %.4s", msg.reference_id);
		/* TODO: closer look at the kiss code */
		return (SNTP_BACKOFF);
	}

	/* check validity: synchronized NTPv4 server */
	switch (msg.flags) {
	case 0x23: /* version 4 client */
		/* we're probably accidentally querying ourselves */
		return (SNTP_BADRESP);

	case 0x24: /* no warning, version 4, server */
	case 0x64: /* subtract leap second, version 4, server */
	case 0xa4: /* add leap second, version 4, server */
		/* these are the normal, useful cases */
		break;

	case 0xe4: /* unsynchronized, version 4, server */
		/* server not usable (yet?) */
		return (SNTP_LAME);

	default:
		return (SNTP_BADRESP);
	}

	/* check if this is the response we were expecting */
	if (!nt_eq(msg.originate, sntp->last_send))
		/* probably delayed response to old request */
		return (SNTP_NORESP);

	ts2nt(&ts, &sntp->last_recv);
	*nt = msg.transmit;
	return (SNTP_OK);
}
