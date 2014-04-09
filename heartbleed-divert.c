/*
 * heartbleed-divert quick and dirty mitigation trick for openbsd servers
 * when the TLS heartbeat negotiation is detected (0x1801:0x1803) the packet is 
 * not re-injected into the system.
 * No daemon support since this is quick-n-dirty and you have to patch your 
 * servers. 
 * suggested way of running this is from within tmux and on the actual server 
 * that hosts the ssl service to be protected.
 * njoy
 *
 * pass in quick on egress inet proto tcp to $web_server port 443 divert-packet port 700
 * or if on different server than pf also add
 * pass ou quick on egress inet proto tcp from $web_server divert-reply  
 * Note: This setup doesnt make a lot of sense with nat :)
 * Pantelis Roditis proditis]at[echothrust.com
 * Echothrust Solutions
 */
 
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#define DIVERT_PORT 700
#define DAEMON_NAME "heartbleed-divert"

	int
main (int argc, char *argv[])
{
	int fd, s, i;
	struct sockaddr_in sin;
	socklen_t sin_len;

	fd = socket (AF_INET, SOCK_RAW, IPPROTO_DIVERT);
	if (fd == -1)
		err (1, "socket");

	bzero (&sin, sizeof (sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons (DIVERT_PORT);
	sin.sin_addr.s_addr = 0;

	sin_len = sizeof (struct sockaddr_in);

	s = bind (fd, (struct sockaddr *) &sin, sin_len);
	if (s == -1)
		err (1, "bind");

	for (;;)
	{
		ssize_t n;
		char packet[10000];
		struct ip *ip_hdr;
		struct tcpiphdr *tcpip_hdr;
		char srcip[40], dstip[40];

		bzero (packet, sizeof (packet));
		n = recvfrom (fd, packet, sizeof (packet), 0,
				(struct sockaddr *) &sin, &sin_len);

		tcpip_hdr = (struct tcpiphdr *) packet;
		ip_hdr = (struct ip *) packet;

		bzero (srcip, sizeof (srcip));
		bzero (dstip, sizeof (dstip));


		if (inet_ntop (AF_INET, &ip_hdr->ip_src, srcip, sizeof (srcip)) == NULL)
		{
			fprintf (stderr, "Invalid IPv4 source packet\n");
			continue;
		}
		if (inet_ntop (AF_INET, &ip_hdr->ip_dst, dstip, sizeof (dstip)) == NULL)
		{
			fprintf (stderr, "Invalid IPv4 destination " "packet\n");
			continue;
		}

		if (n > 55 
				&& (packet[52] == 0x18
					&& (packet[53] == 0x03 || packet[53] == 0x02
						|| packet[53] == 0x01)))
		{
			printf ("heartbleed from -> %s\n", srcip);
			printf ("%s:%u -> %s:%u\n",
					srcip,
					ntohs (tcpip_hdr->ti_sport),
					dstip, ntohs (tcpip_hdr->ti_dport));
		}
    else
      n = sendto (fd, packet, n, 0, (struct sockaddr *) &sin, sin_len);
	}

	return 0;
}
