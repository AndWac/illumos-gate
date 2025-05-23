/* $OpenBSD: netcat.c,v 1.89 2007/02/20 14:11:17 jmc Exp $ */
/*
 * Copyright (c) 2001 Eric Jackson <ericj@monkey.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Re-written nc(1) for OpenBSD. Original implementation by
 * *Hobbit* <hobbit@avian.org>.
 */

/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Portions Copyright 2008 Erik Trauschke
 * Copyright 2024 Oxide Computer Company
 * Copyright 2024 MNX Cloud, Inc.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <arpa/telnet.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>

#include "atomicio.h"

#ifndef	SUN_LEN
#define	SUN_LEN(su) \
	(sizeof (*(su)) - sizeof ((su)->sun_path) + strlen((su)->sun_path))
#endif

#define	PORT_MIN	1
#define	PORT_MAX	65535
#define	PORT_MAX_LEN	6
#define	PLIST_SZ	32	/* initial capacity of the portlist */

/* Command Line Options */
int	dflag;		/* detached, no stdin */
unsigned int iflag;	/* Interval Flag */
int	kflag;		/* More than one connect */
int	lflag;		/* Bind to local port */
int	nflag;		/* Don't do name lookup */
const char *Pflag;		/* Proxy username */
char	*pflag;		/* Localport flag */
int	rflag;		/* Random ports flag */
const char *sflag;		/* Source Address */
int	tflag;		/* Telnet Emulation */
int	uflag;		/* UDP - Default to TCP */
int	vflag;		/* Verbosity */
int	xflag;		/* Socks proxy */
int	Xflag;		/* indicator of Socks version set */
int	zflag;		/* Port Scan Flag */
int	Bflag;		/* Use IP_SEC_OPT to bypass policy */
int	Dflag;		/* sodebug */
int	Sflag;		/* TCP MD5 signature option */
int	Tflag = -1;	/* IP Type of Service */

int	timeout = -1;
int	family = AF_UNSPEC;
int	ttl = -1;
int	minttl = -1;

/*
 * portlist structure
 * Used to store a list of ports given by the user and maintaining
 * information about the number of ports stored.
 */
struct {
	uint16_t *list; /* list containing the ports */
	uint_t listsize;   /* capacity of the list (number of entries) */
	uint_t numports;   /* number of ports in the list */
} ports;

void	atelnet(int, unsigned char *, unsigned int);
void	build_ports(char *);
void	help(void);
int	local_listen(const char *, const char *, struct addrinfo);
void	readwrite(int);
int	remote_connect(const char *, const char *, struct addrinfo);
int	socks_connect(const char *, const char *,
	    const char *, const char *, struct addrinfo, int, const char *);
int	udptest(int);
int	unix_connect(const char *);
int	unix_listen(const char *);
void	set_common_sockopts(int, int);
int	parse_iptos(const char *);
void	usage(int);
const char *print_addr(char *, size_t, struct sockaddr *, int, int);

int
main(int argc, char *argv[])
{
	int ch, s, ret, socksv;
	char *host, *uport, *proxy;
	struct addrinfo hints;
	struct servent *sv;
	socklen_t len;
	struct sockaddr_storage cliaddr;
	const char *errstr, *proxyhost = "", *proxyport = NULL;
	struct addrinfo proxyhints;
	char port[PORT_MAX_LEN];

	ret = 1;
	s = -1;
	socksv = 5;
	host = NULL;
	uport = NULL;
	sv = NULL;

	while ((ch = getopt(argc, argv,
	    "46BDdhi:klm:M:nP:p:rs:ST:tUuvw:X:x:z")) != -1) {
		switch (ch) {
		case '4':
			family = AF_INET;
			break;
		case '6':
			family = AF_INET6;
			break;
		case 'U':
			family = AF_UNIX;
			break;
		case 'B':
			Bflag = 1;
			break;
		case 'X':
			Xflag = 1;
			if (strcasecmp(optarg, "connect") == 0)
				socksv = -1; /* HTTP proxy CONNECT */
			else if (strcmp(optarg, "4") == 0)
				socksv = 4; /* SOCKS v.4 */
			else if (strcmp(optarg, "5") == 0)
				socksv = 5; /* SOCKS v.5 */
			else
				errx(1, "unsupported proxy protocol");
			break;
		case 'd':
			dflag = 1;
			break;
		case 'h':
			help();
			break;
		case 'i':
			iflag = strtonum(optarg, 0, UINT_MAX, &errstr);
			if (errstr)
				errx(1, "interval %s: %s", errstr, optarg);
			break;
		case 'k':
			kflag = 1;
			break;
		case 'M':
			ttl = strtonumx(optarg, 1, 255, &errstr, 0);
			if (errstr != NULL) {
				errx(1, "ttl is %s: %s, valid values are "
				    "between 1 and 255", errstr, optarg);
			}
			break;
		case 'm':
			minttl = strtonumx(optarg, 0, 255, &errstr, 0);
			if (errstr != NULL) {
				errx(1, "minimum ttl is %s: %s, valid values "
				    "are between 0 and 255", errstr, optarg);
			}
			break;
		case 'l':
			lflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'P':
			Pflag = optarg;
			break;
		case 'p':
			pflag = optarg;
			break;
		case 'r':
			rflag = 1;
			break;
		case 's':
			sflag = optarg;
			break;
		case 't':
			tflag = 1;
			break;
		case 'u':
			uflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		case 'w':
			timeout = strtonum(optarg, 0, INT_MAX / 1000, &errstr);
			if (errstr)
				errx(1, "timeout %s: %s", errstr, optarg);
			timeout *= 1000;
			break;
		case 'x':
			xflag = 1;
			if ((proxy = strdup(optarg)) == NULL)
				err(1, NULL);
			break;
		case 'z':
			zflag = 1;
			break;
		case 'D':
			Dflag = 1;
			break;
		case 'S':
			Sflag = 1;
			break;
		case 'T':
			Tflag = parse_iptos(optarg);
			break;
		default:
			usage(1);
		}
	}
	argc -= optind;
	argv += optind;

	/* Cruft to make sure options are clean, and used properly. */
	if (argv[0] && !argv[1] && family == AF_UNIX) {
		if (uflag)
			errx(1, "cannot use -u and -U");
		host = argv[0];
		uport = NULL;
	} else if (argv[0] && !argv[1]) {
		if (!lflag)
			usage(1);
		uport = argv[0];
		host = NULL;
	} else if (argv[0] && argv[1]) {
		if (family == AF_UNIX)
			usage(1);
		host = argv[0];
		uport = argv[1];
	} else {
		if (!(lflag && pflag))
			usage(1);
	}

	if (argc > 2)
		usage(1);

	if (lflag && sflag)
		errx(1, "cannot use -s and -l");
	if (lflag && rflag)
		errx(1, "cannot use -r and -l");
	if (lflag && Bflag)
		errx(1, "cannot use -B and -l");
	if (lflag && (timeout >= 0))
		warnx("-w has no effect with -l");
	if (lflag && pflag) {
		if (uport)
			usage(1);
		uport = pflag;
	}
	if (lflag && zflag)
		errx(1, "cannot use -z and -l");
	if (!lflag && kflag)
		errx(1, "must use -l with -k");
	if (lflag && (Pflag || xflag || Xflag))
		errx(1, "cannot use -l with -P, -X or -x");

	/* Initialize addrinfo structure. */
	if (family != AF_UNIX) {
		(void) memset(&hints, 0, sizeof (struct addrinfo));
		hints.ai_family = family;
		hints.ai_socktype = uflag ? SOCK_DGRAM : SOCK_STREAM;
		hints.ai_protocol = uflag ? IPPROTO_UDP : IPPROTO_TCP;
		if (nflag)
			hints.ai_flags |= AI_NUMERICHOST;
	}

	if (xflag) {
		if (uflag)
			errx(1, "no proxy support for UDP mode");

		if (lflag)
			errx(1, "no proxy support for listen");

		if (family == AF_UNIX)
			errx(1, "no proxy support for unix sockets");

		if (family == AF_INET6)
			errx(1, "no proxy support for IPv6");

		if (sflag)
			errx(1, "no proxy support for local source address");

		if ((proxyhost = strtok(proxy, ":")) == NULL)
			errx(1, "missing port specification");
		proxyport = strtok(NULL, ":");

		(void) memset(&proxyhints, 0, sizeof (struct addrinfo));
		proxyhints.ai_family = family;
		proxyhints.ai_socktype = SOCK_STREAM;
		proxyhints.ai_protocol = IPPROTO_TCP;
		if (nflag)
			proxyhints.ai_flags |= AI_NUMERICHOST;
	}

	if (lflag) {
		int connfd;
		ret = 0;

		if (family == AF_UNIX) {
			if (host == NULL)
				usage(1);
			s = unix_listen(host);
		}

		/* Allow only one connection at a time, but stay alive. */
		for (;;) {
			if (family != AF_UNIX) {
				/* check if uport is valid */
				if (strtonum(uport, PORT_MIN, PORT_MAX,
				    &errstr) == 0)
					errx(1, "port number %s: %s",
					    uport, errstr);
				s = local_listen(host, uport, hints);
			}
			if (s < 0)
				err(1, NULL);
			/*
			 * For UDP, we will use recvfrom() initially
			 * to wait for a caller, then use the regular
			 * functions to talk to the caller.
			 */
			if (uflag) {
				int rv, plen;
				char buf[8192];
				struct sockaddr_storage z;

				len = sizeof (z);
				plen = 1024;
				rv = recvfrom(s, buf, plen, MSG_PEEK,
				    (struct sockaddr *)&z, &len);
				if (rv < 0)
					err(1, "recvfrom");

				rv = connect(s, (struct sockaddr *)&z, len);
				if (rv < 0)
					err(1, "connect");

				connfd = s;
			} else {
				len = sizeof (cliaddr);
				connfd = accept(s, (struct sockaddr *)&cliaddr,
				    &len);
				if ((connfd != -1) && vflag) {
					char ntop[NI_MAXHOST + NI_MAXSERV];
					(void) fprintf(stderr,
					    "Received connection from %s\n",
					    print_addr(ntop, sizeof (ntop),
					    (struct sockaddr *)&cliaddr, len,
					    nflag ? NI_NUMERICHOST : 0));
				}
			}

			readwrite(connfd);
			(void) close(connfd);
			if (family != AF_UNIX)
				(void) close(s);

			if (!kflag)
				break;
		}
	} else if (family == AF_UNIX) {
		ret = 0;

		if ((s = unix_connect(host)) > 0 && !zflag) {
			readwrite(s);
			(void) close(s);
		} else
			ret = 1;

		exit(ret);

	} else {	/* AF_INET or AF_INET6 */
		int i;

		/* Construct the portlist. */
		build_ports(uport);

		/* Cycle through portlist, connecting to each port. */
		for (i = 0; i < ports.numports; i++) {
			(void) snprintf(port, sizeof (port), "%u",
			    ports.list[i]);

			if (s != -1)
				(void) close(s);

			if (xflag)
				s = socks_connect(host, port,
				    proxyhost, proxyport, proxyhints, socksv,
				    Pflag);
			else
				s = remote_connect(host, port, hints);

			if (s < 0)
				continue;

			ret = 0;
			if (vflag || zflag) {
				/* For UDP, make sure we are connected. */
				if (uflag) {
					if (udptest(s) == -1) {
						ret = 1;
						continue;
					}
				}

				/* Don't look up port if -n. */
				if (nflag)
					sv = NULL;
				else {
					sv = getservbyport(
					    ntohs(ports.list[i]),
					    uflag ? "udp" : "tcp");
				}

				(void) fprintf(stderr, "Connection to %s %s "
				    "port [%s/%s] succeeded!\n",
				    host, port, uflag ? "udp" : "tcp",
				    sv ? sv->s_name : "*");
			}
			if (!zflag)
				readwrite(s);
		}
		free(ports.list);
	}

	if (s != -1)
		(void) close(s);

	return (ret);
}

/*
 * print IP address and (optionally) a port
 */
const char *
print_addr(char *ntop, size_t ntlen, struct sockaddr *addr, int len, int flags)
{
	char port[NI_MAXSERV];
	int e;

	/* print port always as number */
	if ((e = getnameinfo(addr, len, ntop, ntlen,
	    port, sizeof (port), flags|NI_NUMERICSERV)) != 0) {
		return ((char *)gai_strerror(e));
	}

	(void) strlcat(ntop, " port ", ntlen);
	(void) strlcat(ntop, port, ntlen);

	return (ntop);
}

/*
 * unix_connect()
 * Returns a socket connected to a local unix socket. Returns -1 on failure.
 */
int
unix_connect(const char *path)
{
	struct sockaddr_un sunaddr;
	int s;

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return (-1);

	(void) memset(&sunaddr, 0, sizeof (struct sockaddr_un));
	sunaddr.sun_family = AF_UNIX;

	if (strlcpy(sunaddr.sun_path, path, sizeof (sunaddr.sun_path)) >=
	    sizeof (sunaddr.sun_path)) {
		(void) close(s);
		errno = ENAMETOOLONG;
		return (-1);
	}
	if (connect(s, (struct sockaddr *)&sunaddr, SUN_LEN(&sunaddr)) < 0) {
		(void) close(s);
		return (-1);
	}
	return (s);
}

/*
 * unix_listen()
 * Create a unix domain socket, and listen on it.
 */
int
unix_listen(const char *path)
{
	struct sockaddr_un sunaddr;
	int s;

	/* Create unix domain socket. */
	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return (-1);

	(void) memset(&sunaddr, 0, sizeof (struct sockaddr_un));
	sunaddr.sun_family = AF_UNIX;

	if (strlcpy(sunaddr.sun_path, path, sizeof (sunaddr.sun_path)) >=
	    sizeof (sunaddr.sun_path)) {
		(void) close(s);
		errno = ENAMETOOLONG;
		return (-1);
	}

	if (bind(s, (struct sockaddr *)&sunaddr, SUN_LEN(&sunaddr)) < 0) {
		(void) close(s);
		return (-1);
	}

	if (listen(s, 5) < 0) {
		(void) close(s);
		return (-1);
	}
	return (s);
}

/*
 * remote_connect()
 * Returns a socket connected to a remote host. Properly binds to a local
 * port or source address if needed. Returns -1 on failure.
 */
int
remote_connect(const char *host, const char *port, struct addrinfo hints)
{
	struct addrinfo *res, *res0;
	int s, error;

	if ((error = getaddrinfo(host, port, &hints, &res)))
		errx(1, "getaddrinfo: %s", gai_strerror(error));

	res0 = res;
	do {
		if ((s = socket(res0->ai_family, res0->ai_socktype,
		    res0->ai_protocol)) < 0) {
			warn("failed to create socket");
			continue;
		}

		/* Bind to a local port or source address if specified. */
		if (sflag || pflag) {
			struct addrinfo ahints, *ares;

			(void) memset(&ahints, 0, sizeof (struct addrinfo));
			ahints.ai_family = res0->ai_family;
			ahints.ai_socktype = uflag ? SOCK_DGRAM : SOCK_STREAM;
			ahints.ai_protocol = uflag ? IPPROTO_UDP : IPPROTO_TCP;
			ahints.ai_flags = AI_PASSIVE;
			if ((error = getaddrinfo(sflag, pflag, &ahints, &ares)))
				errx(1, "getaddrinfo: %s", gai_strerror(error));

			if (bind(s, (struct sockaddr *)ares->ai_addr,
			    ares->ai_addrlen) < 0)
				errx(1, "bind failed: %s", strerror(errno));
			freeaddrinfo(ares);

			if (vflag && !lflag) {
				if (sflag != NULL)
					(void) fprintf(stderr,
					    "Using source address: %s\n",
					    sflag);
				if (pflag != NULL)
					(void) fprintf(stderr,
					    "Using source port: %s\n", pflag);
			}
		}

		set_common_sockopts(s, res0->ai_family);

		if (connect(s, res0->ai_addr, res0->ai_addrlen) == 0)
			break;
		else if (vflag) {
			char ntop[NI_MAXHOST + NI_MAXSERV];
			warn("connect to %s [host %s] (%s) failed",
			    print_addr(ntop, sizeof (ntop),
			    res0->ai_addr, res0->ai_addrlen, NI_NUMERICHOST),
			    host, uflag ? "udp" : "tcp");
		}

		(void) close(s);
		s = -1;
	} while ((res0 = res0->ai_next) != NULL);

	freeaddrinfo(res);

	return (s);
}

/*
 * local_listen()
 * Returns a socket listening on a local port, binds to specified source
 * address. Returns -1 on failure.
 */
int
local_listen(const char *host, const char *port, struct addrinfo hints)
{
	struct addrinfo *res, *res0;
	int s, ret, x = 1;
	int error;

	/* Allow nodename to be null. */
	hints.ai_flags |= AI_PASSIVE;

	if ((error = getaddrinfo(host, port, &hints, &res)))
		errx(1, "getaddrinfo: %s", gai_strerror(error));

	res0 = res;
	do {
		if ((s = socket(res0->ai_family, res0->ai_socktype,
		    res0->ai_protocol)) < 0) {
			warn("failed to create socket");
			continue;
		}

		ret = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &x, sizeof (x));
		if (ret == -1)
			err(1, NULL);

		set_common_sockopts(s, res0->ai_family);

		if (bind(s, (struct sockaddr *)res0->ai_addr,
		    res0->ai_addrlen) == 0)
			break;

		(void) close(s);
		s = -1;
	} while ((res0 = res0->ai_next) != NULL);

	if (!uflag && s != -1) {
		if (listen(s, 1) < 0)
			err(1, "listen");
	}

	freeaddrinfo(res);

	return (s);
}

/*
 * readwrite()
 * Loop that polls on the network file descriptor and stdin.
 */
void
readwrite(int nfd)
{
	struct pollfd pfd[2];
	unsigned char buf[8192];
	int n, wfd = fileno(stdin);
	int lfd = fileno(stdout);
	int plen;

	plen = 1024;

	/* Setup Network FD */
	pfd[0].fd = nfd;
	pfd[0].events = POLLIN;

	/* Set up STDIN FD. */
	pfd[1].fd = wfd;
	pfd[1].events = POLLIN;

	while (pfd[0].fd != -1) {
		if (iflag)
			(void) sleep(iflag);

		if ((n = poll(pfd, 2 - dflag, timeout)) < 0) {
			(void) close(nfd);
			err(1, "Polling Error");
		}

		if (n == 0)
			return;

		if (pfd[0].revents & (POLLIN|POLLHUP)) {
			if ((n = read(nfd, buf, plen)) < 0)
				return;
			else if (n == 0) {
				(void) shutdown(nfd, SHUT_RD);
				pfd[0].fd = -1;
				pfd[0].events = 0;
			} else {
				if (tflag)
					atelnet(nfd, buf, n);
				if (atomicio(vwrite, lfd, buf, n) != n)
					return;
			}
		}

		/*
		 * handle the case of disconnected pipe: after pipe
		 * is closed (indicated by POLLHUP) there may still
		 * be some data lingering (POLLIN). After we read
		 * the data, only POLLHUP remains, read() returns 0
		 * and we are finished.
		 */
		if (!dflag && (pfd[1].revents & (POLLIN|POLLHUP))) {
			if ((n = read(wfd, buf, plen)) < 0)
				return;
			else if (n == 0) {
				(void) shutdown(nfd, SHUT_WR);
				pfd[1].fd = -1;
				pfd[1].events = 0;
			} else {
				if (atomicio(vwrite, nfd, buf, n) != n)
					return;
			}
		}
	}
}

/* Deal with RFC 854 WILL/WONT DO/DONT negotiation. */
void
atelnet(int nfd, unsigned char *buf, unsigned int size)
{
	unsigned char *p, *end;
	unsigned char obuf[4];

	end = buf + size;
	obuf[0] = '\0';

	for (p = buf; p < end; p++) {
		if (*p != IAC)
			break;

		obuf[0] = IAC;
		obuf[1] = 0;
		p++;
		/* refuse all options */
		if ((*p == WILL) || (*p == WONT))
			obuf[1] = DONT;
		if ((*p == DO) || (*p == DONT))
			obuf[1] = WONT;
		if (obuf[1]) {
			p++;
			obuf[2] = *p;
			obuf[3] = '\0';
			if (atomicio(vwrite, nfd, obuf, 3) != 3)
				warn("Write Error!");
			obuf[0] = '\0';
		}
	}
}

/*
 * build_ports()
 * Build an array of ports in ports.list[], listing each port
 * that we should try to connect to.
 */
void
build_ports(char *p)
{
	const char *errstr;
	const char *token;
	char *n;
	int lo, hi, cp;
	int i;

	/* Set up initial portlist. */
	ports.list = malloc(PLIST_SZ * sizeof (uint16_t));
	if (ports.list == NULL)
		err(1, NULL);
	ports.listsize = PLIST_SZ;
	ports.numports = 0;

	/* Cycle through list of given ports sep. by "," */
	while ((token = strsep(&p, ",")) != NULL) {
		if (*token == '\0')
			errx(1, "Invalid port/portlist format: "
			    "zero length port");

		/* check if it is a range */
		if ((n = strchr(token, '-')) != NULL)
			*n++ = '\0';

		lo = strtonum(token, PORT_MIN, PORT_MAX, &errstr);
		if (errstr)
			errx(1, "port number %s: %s", errstr, token);

		if (n == NULL) {
			hi = lo;
		} else {
			hi = strtonum(n, PORT_MIN, PORT_MAX, &errstr);
			if (errstr)
				errx(1, "port number %s: %s", errstr, n);
			if (lo > hi) {
				cp = hi;
				hi = lo;
				lo = cp;
			}
		}

		/*
		 * Grow the portlist if needed.
		 * We double the size and add size of current range
		 * to make sure we don't have to resize that often.
		 */
		if (hi - lo + ports.numports + 1 >= ports.listsize) {
			ports.listsize = ports.listsize * 2 + hi - lo;
			ports.list = realloc(ports.list,
			    ports.listsize * sizeof (uint16_t));
			if (ports.list == NULL)
				err(1, NULL);
		}

		/* Load ports sequentially. */
		for (i = lo; i <= hi; i++)
			ports.list[ports.numports++] = i;
	}

	/* Randomly swap ports. */
	if (rflag) {
		int y;
		uint16_t u;

		if (ports.numports < 2) {
			warnx("can not swap %d port randomly",
			    ports.numports);
			return;
		}
		srandom(time(NULL));
		for (i = 0; i < ports.numports; i++) {
			y = random() % (ports.numports - 1);
			u = ports.list[i];
			ports.list[i] = ports.list[y];
			ports.list[y] = u;
		}
	}
}

/*
 * udptest()
 * Do a few writes to see if the UDP port is there.
 * XXX - Better way of doing this? Doesn't work for IPv6.
 * Also fails after around 100 ports checked.
 */
int
udptest(int s)
{
	int i, ret;

	for (i = 0; i <= 3; i++) {
		if (write(s, "X", 1) == 1)
			ret = 1;
		else
			ret = -1;
	}
	return (ret);
}

void
set_common_sockopts(int s, int af)
{
	int x = 1;

	if (Sflag) {
		if (setsockopt(s, IPPROTO_TCP, TCP_MD5SIG,
		    &x, sizeof (x)) == -1) {
			err(1, NULL);
		}
	}

	if (Bflag) {
		ipsec_req_t req = { IPSEC_PREF_NEVER, IPSEC_PREF_NEVER,
			IPSEC_PREF_NEVER, 0, 0, 0 };
		int level;

		switch (af) {
		case AF_INET:
			level = IPPROTO_IP;
			break;
		case AF_INET6:
			level = IPPROTO_IPV6;
			break;
		default:
			err(1, "cannot set IPsec bypass on unsupported socket "
			    "family 0x%x", af);
		}
		/* IP_SEC_OPT == IPV6_SEC_OPT, so we're good regardless... */
		if (setsockopt(s, level, IP_SEC_OPT, &req, sizeof (req)) < 0) {
			err(1, "IPsec bypass attempt");
		}
	}

	if (Dflag) {
		if (setsockopt(s, SOL_SOCKET, SO_DEBUG, &x, sizeof (x)) == -1)
			err(1, NULL);
	}
	if (Tflag != -1) {
		switch (af) {
		case AF_INET:
			if (setsockopt(s, IPPROTO_IP, IP_TOS, &Tflag,
			    sizeof (Tflag)) == -1) {
				err(1, "failed to set IP ToS socket option");
			}
			break;
		case AF_INET6:
			if (setsockopt(s, IPPROTO_IPV6, IPV6_TCLASS, &Tflag,
			    sizeof (Tflag)) == -1) {
				err(1, "failed to set IPv6 traffic class "
				    "socket option");
			}
			break;
		default:
			err(1, "cannot set TTL/Hops on unsupported socket "
			    "family 0x%x", af);
		}
	}

	if (ttl != -1) {
		switch (af) {
		case AF_INET:
			if (setsockopt(s, IPPROTO_IP, IP_TTL, &ttl,
			    sizeof (ttl)) != 0) {
				err(1, "failed to set IP TTL socket option");
			}
			break;
		case AF_INET6:
			if (setsockopt(s, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &ttl,
			    sizeof (ttl)) != 0) {
				err(1, "failed to set IPv6 unicast hops socket "
				    "option");
			}
			break;
		default:
			err(1, "cannot set TTL/Hops on unsupported socket "
			    "family 0x%x", af);
		}
	}

	if (minttl != -1) {
		switch (af) {
		case AF_INET:
			if (setsockopt(s, IPPROTO_IP, IP_MINTTL, &minttl,
			    sizeof (minttl)) != 0) {
				err(1, "failed to set IP minimum TTL socket "
				    "option");
			}
			break;
		case AF_INET6:
			if (setsockopt(s, IPPROTO_IPV6, IPV6_MINHOPCOUNT,
			    &minttl, sizeof (minttl)) != 0) {
				err(1, "failed to set IPv6 minimum hop count "
				    "socket option");
			}
			break;
		default:
			err(1, "cannot set minimum TTL/Hops on unsupported "
			    "socket family 0x%x", af);
		}
	}

}

int
parse_iptos(const char *s)
{
	int tos;
	const char *errstr;

	if (strcmp(s, "lowdelay") == 0)
		return (IPTOS_LOWDELAY);
	if (strcmp(s, "throughput") == 0)
		return (IPTOS_THROUGHPUT);
	if (strcmp(s, "reliability") == 0)
		return (IPTOS_RELIABILITY);

	tos = strtonumx(s, 0, 255, &errstr, 0);
	if (errstr != NULL) {
		errx(1, "IP ToS/IPv6 TC is %s: %s, valid values are "
		    "between 0 and 255", errstr, optarg);
	}

	return (tos);
}

void
help(void)
{
	usage(0);
	(void) fprintf(stderr, "\tCommand Summary:\n\
	\t-4		Use IPv4\n\
	\t-6		Use IPv6\n\
	\t-B		Bypass IPsec policy to force cleartext\n\
	\t-D		Enable the debug socket option\n\
	\t-d		Detach from stdin\n\
	\t-h		This help text\n\
	\t-i secs\t	Delay interval for lines sent, ports scanned\n\
	\t-k		Keep inbound sockets open for multiple connects\n\
	\t-l		Listen mode, for inbound connects\n\
	\t-l		Listen mode, for inbound connects\n\
	\t-M ttl\t	Set the outbound IPv4 TTL / IPv6 Hop Limit\n\
	\t-m minttl	Set the inbound minimum IPv4 TTL / IPv6 Hop Limit\n\
	\t-n		Suppress name/port resolutions\n\
	\t-P proxyuser\tUsername for proxy authentication\n\
	\t-p port\t	Specify local port or listen port\n\
	\t-r		Randomize remote ports\n\
	\t-S		Enable TCP MD5 signature socket option\n\
	\t-s addr\t	Local source address\n\
	\t-T ToS\t	Set IP Type of Service\n\
	\t-t		Answer TELNET negotiation\n\
	\t-U		Use UNIX domain socket\n\
	\t-u		UDP mode\n\
	\t-v		Verbose\n\
	\t-w secs\t	Timeout for connects and final net reads\n\
	\t-X proto	Proxy protocol: \"4\", \"5\" (SOCKS) or \"connect\"\n\
	\t-x addr[:port]\tSpecify proxy address and port\n\
	\t-z		Zero-I/O mode [used for scanning]\n\
	Port numbers can be individuals, ranges (lo-hi; inclusive) and\n\
	combinations of both separated by comma (e.g. 10,22-25,80)\n");
	exit(1);
}

void
usage(int ret)
{
	(void) fprintf(stderr,
	    "usage: nc [-46BDdhklnrStUuvz] [-i interval] [-M ttl] [-m minttl]\n"
	    "\t  [-P proxy_username] [-p port] [-s source_ip_address] "
	    "[-T ToS]\n"
	    "\t  [-w timeout] [-X proxy_protocol] [-x proxy_address[:port]]\n"
	    "\t  [hostname] [port[s]]\n");
	if (ret)
		exit(1);
}
