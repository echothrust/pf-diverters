# Build the diverters dont write this with eclipse :P
# SAMPLE needs work
#

all: bofh-divert dnsbl-divert

bofh-divert: bofh-divert.c daemon.o stdpf.o
	gcc -o bofh-divert bofh-divert.c daemon.o stdpf.o

dnsbl-divert: dnsbl-divert.c daemon.o stdpf.o
	gcc -o dnsbl-divert dnsbl-divert.c daemon.o stdpf.o

daemon.o: daemon.c
	gcc -c daemon.c 

stdpf.o: stdpf.c
	gcc -c stdpf.c 

clean:
	rm -rf stdpf.o daemon.o dnsbl-divert bofh-divert

install: all
	install -c bofh-divert /usr/local/sbin
	install -c dnsbl-divert /usr/local/sbin
