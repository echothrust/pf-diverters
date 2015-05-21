/*
 A simple divert socket daemon to automaticaly block connections.
 You redirect a bounch of unused (by you) ports to the daemon listening on 
 a divert socket and watch the magic happens.
 

 --pf.conf--
 block in log quick from <bastards>
 # Or, if you want to be evil:
 #block in log quick from <bastards> probability 67%
 # BOFH OPERATIONS
 pass in log quick on { egress } inet proto tcp from !<bastards> to port { 1024, 8080, 5900, 139, 138, 445, 3389 , 23, 110, 6001, 81, 8888, 86, 22  } divert-packet port 700 no state


 Compile:
 make bofh-divert
 
 Run:
 ./bofh-divert
 
 XXX TODO XXX
 - Add non-daemon mode with stdout/stderr logging
 - Implement privsep support
 - rc.d script ?
 - support merge lists without whitelisted entries....

 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/in_systm.h>
#include <netinet/ip_var.h>
#include <net/if.h>
#include <net/pfvar.h>
#include <netinet/tcpip.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h> // for getopt
#include <ctype.h> // for isdigit
#include <err.h>

#include "stdpf.h"
#include "daemon.h"

#define DAEMON_NAME "bofh-divert"

void usage() {
	fprintf(stderr,"usage: %s -p pnum -t tname\n",DAEMON_NAME);
	fprintf(stderr,"\tpnum   divert port number to bind (1-65535)\n");
	fprintf(stderr,"\ttname  table to add collected host IPs (up to %d chars)\n",PF_TABLE_NAME_SIZE);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
	int fd, s,n;
	struct sockaddr_in sin;
	struct in_addr *pfip;
	socklen_t sin_len;
	char pfTable[PF_TABLE_NAME_SIZE];
	int divertPort=0;
	char pidPath[64];
	char syslogLine[256];

	extern char *optarg;
	int ch, cherr=0, pflag=0, tflag=0;
	while ((ch = getopt(argc, argv, "p:t:")) != -1) {
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
				if (strlcpy(pfTable, optarg, sizeof(pfTable)) >= sizeof(pfTable))
				    	fprintf(stderr,"Table name truncated to PF_TABLE_NAME_SIZE: <%s>\n",pfTable);
			} else {
				cherr=1;
			}
			break;
		default:
			cherr=1;
		}
	}
	if (pflag==0 || tflag==0) {
		fprintf(stderr,"Error: Please specify port(-p) and table name(-t).\n");
		usage();
	}
	if (cherr!=0 || divertPort<1 || divertPort>65535) {
		fprintf(stderr,"Error: Bad input, please revise program usage.\n");
		usage();
	}


	/* Logging */
	setlogmask(LOG_UPTO(LOG_INFO));
	openlog(DAEMON_NAME, LOG_CONS | LOG_PERROR, LOG_USER);

	syslog(LOG_INFO, "Daemon starting up");

	/* PID FILE */
	snprintf(pidPath, sizeof pidPath, "%s%s%s", "/var/run/bofh-", pfTable, ".pid");

	/* Deamonize */
	daemonize("/tmp/", pidPath);

	syslog(LOG_INFO, "Daemon running");

	memset(syslogLine,0x0,sizeof(syslogLine));
	snprintf(syslogLine, sizeof syslogLine, "Using PF table <%s>", pfTable);
	syslog(LOG_INFO, syslogLine);

	// create socket of type IPPROTO_DIVERT
	fd = socket(AF_INET, SOCK_RAW, IPPROTO_DIVERT);
	if (fd == -1)
	{
		syslog(LOG_ERR, "ERROR Could not create divert socket.");
		err(1, "socket");
	}
	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(divertPort);
	sin.sin_addr.s_addr = htonl(INADDR_ANY);

	sin_len = sizeof(struct sockaddr_in);

	memset(syslogLine,0x0,sizeof(syslogLine));

	s = bind(fd, (struct sockaddr *) &sin, sin_len);
	if (s == -1)
	{
		snprintf(syslogLine, sizeof syslogLine, "ERROR binding divert socket to port [%d].", divertPort);
		syslog(LOG_ERR, syslogLine);
		err(1, "bind");
	}
	else
	{
		snprintf(syslogLine, sizeof syslogLine, "Bound to divert socket to port [%d].", divertPort);
		syslog(LOG_INFO, syslogLine);
	}

	// wait for incoming packets and add them to the table
	for (;;) {
		ssize_t n;
		char packet[10000];
		struct ip *ip_hdr;
		struct tcpiphdr *tcpip_hdr;
		char srcip[40], dstip[40];

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

		memset(syslogLine,0x0,sizeof(syslogLine));
		snprintf(syslogLine, sizeof syslogLine, "%s:%u -> %s:%u", srcip, ntohs(tcpip_hdr->ti_sport), dstip,
				ntohs(tcpip_hdr->ti_dport));
		syslog(LOG_NOTICE, syslogLine);
		/*
		 * XXX We need to come up with a better open/add/close method
		 */
		ets_pf_open();
		add(pfTable, &ip_hdr->ip_src, 32);
		ets_pf_close();
	}

	return 0;
}
