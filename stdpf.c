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
#include <arpa/inet.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <err.h>


#include "stdpf.h"

int pfdev = -1;
int timeout = 0;

TAILQ_HEAD( pftimeout_head, pftimeout) timeouts;
struct pftimeout {
	TAILQ_ENTRY(pftimeout) queue;
	struct in_addr ip;
	uint8_t mask;
	time_t timeout;
	char table[PF_TABLE_NAME_SIZE];
};

/*
 *  Borrowed from pftabled (was static in pftabled.c)
 */
void add(char *tname, struct in_addr *ip, uint8_t mask) {
	struct pfioc_table io;
	struct pfr_table table;
	struct pfr_addr addr;
	struct pftimeout *t;

	bzero(&io, sizeof io);
	bzero(&table, sizeof(table));
	bzero(&addr, sizeof(addr));

	strncpy(table.pfrt_name, tname, sizeof(table.pfrt_name));

	bcopy(ip, &addr.pfra_ip4addr, 4);
	addr.pfra_af = AF_INET;
	addr.pfra_net = mask;

	io.pfrio_table = table;
	io.pfrio_buffer = &addr;
	io.pfrio_esize = sizeof(addr);
	io.pfrio_size = 1;

	if (ioctl(pfdev, DIOCRADDADDRS, &io))
		err(1, "ioctl");

	if (timeout) {
		if ((t = malloc(sizeof(struct pftimeout))) == NULL)
			err(1, "malloc");
		t->ip = *ip;
		t->mask = mask;
		t->timeout = time(NULL) + timeout;
		strncpy(t->table, tname, sizeof(t->table));
		TAILQ_INSERT_HEAD(&timeouts, t, queue);
	}
}

void ets_pf_open() {
	// Open
	pfdev = open(PFDEV, O_RDWR);
	if (pfdev == -1)
		err(1, "open " PFDEV);
}
void ets_pf_close() {
	// and close..
	close(pfdev);
}
