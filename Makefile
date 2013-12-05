# Build the diverters dont write this with eclipse :P
# SAMPLE needs work
#

BINDIR = /usr/local/sbin

all: bofh-divert dnsbl-divert

install: install-bofh install-dnsbl

uninstall: uninstall-bofh uninstall-dnsbl

clean:
	rm -rf stdpf.o daemon.o dnsbl-divert bofh-divert

bofh-divert: bofh-divert.c daemon.o stdpf.o
	gcc -o bofh-divert bofh-divert.c daemon.o stdpf.o

dnsbl-divert: dnsbl-divert.c daemon.o stdpf.o
	gcc -o dnsbl-divert dnsbl-divert.c daemon.o stdpf.o

daemon.o: daemon.c
	gcc -c daemon.c 

stdpf.o: stdpf.c
	gcc -c stdpf.c 

install-bofh:
	install -Ss -o root -g wheel -m 750 bofh-divert $(BINDIR)/bofh-divert
	install -o root -g wheel -m 750 rc.bofh /etc/rc.d/rc.bofh

install-dnsbl:
	install -Ss -o root -g wheel -m 750 dnsbl-divert $(BINDIR)/dnsbl-divert
	install -o root -g wheel -m 750 rc.dnsbl /etc/rc.d/rc.dnsbl

uninstall-bofh:
	rm $(BINDIR)/bofh-divert /etc/rc.d/rc.bofh

uninstall-dnsbl:
	rm $(BINDIR)/dnsbl-divert /etc/rc.d/rc.dnsbl
