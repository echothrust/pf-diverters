# taken from pfstatd
SUBDIR=		bofh
SUBDIR+=	dnsbl
#SUBDIR+=	heartbleed
CFLAGS+=	-Wall
CFLAGS+=	-I${.CURDIR}

.include <bsd.prog.mk>