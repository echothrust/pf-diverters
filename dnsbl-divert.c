/*
 * XXX: THIS IS STILL WORK IN PROGRESS.
 * Blame yourself should this cause your equipment to crash and/or burn.

 A simple divert socket daemon to check if an IP is part of dnsbl list
 and if not forward the packet, otherwise add the user to the predefined
 pf table.

 Compile:
 make dnsbl-divert

 Run:
 ./dnsbl-divert

 Make sure you have defined the tables <dnsbl> and <dnsbl_checked> in pf.conf:
 table <dnsbl> persist counters
 table <dnsbl_checked> persist counters 
 
 Both tables are populated by this program and you can use them in your rules:
 block in log quick on {egress} inet proto tcp from <dnsbl> to any port 25
 pass in log quick on { egress } inet proto tcp from !<dnsbl_checked> to pub-mail.echothrust.com port 25 divert-packet port 800 no state
 
  You may add this to your crontab this pfctl cmd to expire table entries:
 # pfctl -t dnsbl -T expire 86400
 
 XXX TODO XXX
 - Add non-daemon mode
 - Implement privsep support
 - rc.d script ?
 - support merge lists without whitelisted entries....

 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/pfvar.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/in_systm.h>
#include <netinet/ip_var.h>
#include <netinet/tcpip.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ctype.h> // for isdigit
#include <err.h>
#include <fcntl.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h> // for getopt

#include "stdpf.h"
#include "daemon.h"

#define DAEMON_NAME "dnsbl-divert"

extern struct __res_state _res;

/* XXX: DNSBL zones might be better off in a config file */
const char *DNSBL[] = { "bl.echothrust.net", "cbl.abuseat.org",
		"zen.spamhaus.org", "dul.dnsbl.sorbs.net" };


/*
 * Function revip_str shamelesly copied from Joachim Pileborg's answer on:
 * http://stackoverflow.com/questions/16373248/convert-ip-for-reverse-ip-lookup
 */
static char *revip_str(char *ip) {
	static char reversed_ip[INET_ADDRSTRLEN];
	in_addr_t addr;

	/* Get the textual address into binary format */
	inet_pton(AF_INET, ip, &addr);

	/* Reverse the bytes in the binary address */
	addr = ((addr & 0xff000000) >> 24) | ((addr & 0x00ff0000) >> 8)
			| ((addr & 0x0000ff00) << 8) | ((addr & 0x000000ff) << 24);

	/* And lastly get a textual representation back again */
	inet_ntop(AF_INET, &addr, reversed_ip, sizeof(reversed_ip));
	return reversed_ip;
}

void usage() {
	extern char *__progname;
	printf("usage: %s -p pnum -t tbl -c tch [dns]\n",__progname);
	printf("\tpnum  divert port number to bind (1-65535)\n");
	printf("\ttbl   table to populate with DNSBLed hosts (up to %d chars)\n",PF_TABLE_NAME_SIZE);
	printf("\ttch   table to cache already-looked-up hosts (up to %d chars)\n",PF_TABLE_NAME_SIZE);
	printf("\tdns   DNS server IPaddr (default: use system-configured dns)\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
	int i, fd, s;
	struct sockaddr_in sin;
	struct hostent *hp = NULL;
	socklen_t sin_len;
	unsigned long dns = 0l;
	int sockfd;
	int rv;
	char pf_table_black[PF_TABLE_NAME_SIZE];
	char pf_table_cache[PF_TABLE_NAME_SIZE];
	int divertPort=0;
	char pidPath[64];
	char syslogLine[256];

	extern char *__progname;
	extern char *optarg;
	extern int optind;
	int ch, cherr=0, pflag=0, tflag=0, cflag=0;

	while ((ch = getopt(argc, argv, "p:t:c:")) != -1) {
		switch (ch) {
		case 'p':
			pflag=1;
			if (optarg != NULL && strspn(optarg,"0123456789")==strlen(optarg)) {
				// divertPort=atoi(optarg);
				divertPort=(int)strtol(optarg, (char **)NULL, 10);
			} else {
				cherr=1;
			}
			break;
		case 't':
			tflag=1;
			if (optarg != NULL) {
				if (strlcpy(pf_table_black, optarg, sizeof(pf_table_black)) >= sizeof(pf_table_black))
				    	fprintf(stderr,"Table name truncated to PF_TABLE_NAME_SIZE: <%s>",pf_table_black);
			} else {
				cherr=1;
			}
			break;
		case 'c':
			cflag=1;
			if (optarg != NULL) {
				if (strlcpy(pf_table_cache, optarg, sizeof(pf_table_cache)) >= sizeof(pf_table_cache))
						fprintf(stderr,"Table name truncated to PF_TABLE_NAME_SIZE: <%s>",pf_table_cache);
			} else {
				cherr=1;
			}
			break;
		default:
			usage();
		}
	}
	if (pflag==0 || tflag==0 || cflag==0) {
		fprintf(stderr,"Error: Please specify port(-p) and table names(-t and -c).\n");
		usage();
	}
	if (cherr!=0 || divertPort<1 || divertPort>65535) {
		fprintf(stderr,"Error: Bad input, please revise program usage.\n");
		usage();
	}

	/* Logging */
	setlogmask(LOG_UPTO(LOG_INFO));
	openlog(__progname, LOG_CONS | LOG_PERROR, LOG_USER);

	syslog(LOG_INFO, "Daemon starting up");

	/* PID FILE */
	snprintf(pidPath, sizeof pidPath, "%s%s%s", "/var/run/dnsbl-", pf_table_black, ".pid");

	/* Deamonize */
	daemonize("/tmp/", pidPath);

	syslog(LOG_INFO, "Daemon running");

	memset(syslogLine,0x0,sizeof(syslogLine)); // zero whole buffer (slower)
	snprintf(syslogLine, sizeof syslogLine, "DNBLed hosts blacklist table <%s>. DNSBL checks cache table <%s>.",
			pf_table_black, pf_table_cache);
	syslog(LOG_INFO, syslogLine);

	memset(&_res, 0x0, sizeof(_res));
	res_init();

	if (optind!=argc && argv[optind] != NULL && (dns = inet_addr(argv[optind])) != -1) {
		//printf("Using [%s] as dns server\n", argv[optind]);
		memset(syslogLine,0x0,sizeof(syslogLine));
		snprintf(syslogLine, sizeof syslogLine, "Using [%s] as dns server", argv[optind]);
		syslog(LOG_INFO, syslogLine);
		_res.nsaddr.sin_addr.s_addr = dns;
		_res.nscount = 1;
	}
	sethostent(1);
	setnetent(1);

	// create special type of socket (IPPROTO_DIVERT)
	fd = socket(AF_INET, SOCK_RAW, IPPROTO_DIVERT);
	if (fd == -1) {
		//err(1, "socket");
		syslog(LOG_ERR, "ERROR Could not create divert socket.");
	}
	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(divertPort);
	sin.sin_addr.s_addr = htonl(INADDR_ANY);

	sin_len = sizeof(struct sockaddr_in);

	s = bind(fd, (struct sockaddr *) &sin, sin_len);
	memset(syslogLine,0,sizeof(syslogLine));
	if (s == -1) {
		//err(1, "bind");
		snprintf(syslogLine, sizeof syslogLine, "ERROR binding divert socket to port [%d].", divertPort);
		syslog(LOG_ERR, syslogLine);
	} else {
		snprintf(syslogLine, sizeof syslogLine, "Bound to divert socket to port [%d].", divertPort);
		syslog(LOG_INFO, syslogLine);
	}

	// wait for incoming packets, process and (optionally) re-inject them back
	for (;;) {
		ssize_t n;
		char packet[10000];
		struct ip *ip_hdr;
		struct tcpiphdr *tcpip_hdr;
		char srcip[40], dstip[40];
		char fqdnbl[300];

		bzero(packet, sizeof(packet));
		n = recvfrom(fd, packet, sizeof(packet), 0, (struct sockaddr *) &sin,
				&sin_len);

		tcpip_hdr = (struct tcpiphdr *) packet;
		ip_hdr = (struct ip *) packet;

		bzero(srcip, sizeof(srcip));
		bzero(dstip, sizeof(dstip));
		if (inet_ntop(AF_INET, &ip_hdr->ip_src, srcip, sizeof(srcip)) == NULL) {
			syslog(LOG_INFO, "Invalid IPv4 source packet");
			continue;
		}
		if (inet_ntop(AF_INET, &ip_hdr->ip_dst, dstip, sizeof(dstip)) == NULL) {
			syslog(LOG_INFO, "Invalid IPv4 destination packet");
			continue;
		}

		/*
		 * XXX: This will change when dnsbl zones will lay in a file (instead of a char[][])
		 */
		i = 0;
		while (i < (sizeof(DNSBL)/sizeof(char *)) && hp == NULL) {
			memset(fqdnbl, 0x0, sizeof(fqdnbl));
			if (!hp)
				free(hp);
			hp = malloc(sizeof(struct hosten *));
			snprintf(fqdnbl, sizeof(fqdnbl), "%s.%s", revip_str(srcip),
					DNSBL[i++]);
			//printf("%s\n", fqdnbl);
			hp = gethostbyname((char *) fqdnbl);
		}

		memset(syslogLine,0,sizeof(syslogLine));
		if (hp == NULL) {
			snprintf(syslogLine, sizeof syslogLine, "CLEAN ");
			ets_pf_open();
			add(pf_table_cache, &ip_hdr->ip_src, 32);
			ets_pf_close();
			// re-inject back
			n = sendto(fd, packet, n, 0, (struct sockaddr *) &sin, sin_len);
		} else {
			snprintf(syslogLine, sizeof syslogLine, "DNSBL ");
			ets_pf_open();
			add(pf_table_cache, &ip_hdr->ip_src, 32);
			add(pf_table_black, &ip_hdr->ip_src, 32);
			ets_pf_close();
		}
		snprintf(syslogLine, sizeof syslogLine,
				"%s %s:%u -> %s:%u", syslogLine,
				srcip, ntohs(tcpip_hdr->ti_sport),
				dstip, ntohs(tcpip_hdr->ti_dport));
		if(strlen(fqdnbl)>0) {
			snprintf(syslogLine, sizeof syslogLine,
					"%s [%s]", syslogLine, fqdnbl);
		}
		syslog(LOG_INFO, syslogLine);

		hp = NULL;
	}

	return 0;
}
