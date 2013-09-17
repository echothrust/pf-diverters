/* mostly stripped from pftabled.h */
#ifndef PFDEV
#define PFDEV "/dev/pf"

#define cleanmask(ip, mask) { \
        uint8_t *b = (uint8_t *)ip; \
        if (mask < 32) b[3] &= (0xFF << (32 - mask)); \
        if (mask < 24) b[2] &= (0xFF << (24 - mask)); \
        if (mask < 16) b[1] &= (0xFF << (16 - mask)); \
        if (mask <  8) b[0] &= (0xFF << ( 8 - mask)); \
}

void add(char *tname, struct in_addr *ip, uint8_t mask);
void ets_pf_open();
void ets_pf_close();
#endif
