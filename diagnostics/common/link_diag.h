/* FNR-002 temporary test protocol, unrelated to production FNR-005 framing. */
#ifndef FNR_LINK_DIAG_H
#define FNR_LINK_DIAG_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#define DIAG_MAX 261u
#define DIAG_SEED 0x002f4a91u
enum { DIAG_PING=1, DIAG_STREAM=2, DIAG_START=3, DIAG_STOP=4,
       DIAG_INFO=5, DIAG_STATS=6, DIAG_REBOOT=7, DIAG_ECHO=129 };
static inline uint32_t diag_get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static inline void diag_put32(uint8_t *p, uint32_t v) {
    for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(i*8));
}
static inline uint32_t diag_crc(const uint8_t *p, size_t n) {
    uint32_t v=0xffffffffu;
    for(size_t i=0;i<n;i++) {
        v^=p[i];
        for(unsigned b=0;b<8;b++) v=(v>>1)^((0u-(v&1u))&0xedb88320u);
    }
    return ~v;
}
static inline unsigned diag_length(uint32_t seq) {
    const unsigned lengths[4]={1,32,128,250}; return lengths[seq&3u];
}
static inline void diag_payload(uint8_t *p, unsigned n, uint32_t seq, bool stream) {
    uint32_t v=DIAG_SEED^seq^(stream?0xa583174du:0x7139e2b5u);
    for(unsigned i=0;i<n;i++) {v^=v<<13; v^=v>>17; v^=v<<5; p[i]=(uint8_t)v;}
}
static inline bool diag_pattern(const uint8_t *p, unsigned n, uint32_t seq, bool stream) {
    uint8_t expected[250];
    if(n!=diag_length(seq)) return false;
    diag_payload(expected,n,seq,stream); return memcmp(p,expected,n)==0;
}
typedef void (*diag_byte_writer)(uint8_t);
static inline void diag_send(diag_byte_writer out, uint8_t type, uint32_t seq,
                             const uint8_t *payload, unsigned n) {
    uint8_t p[DIAG_MAX];
    if(n>250) return;
    p[0]=type; diag_put32(p+1,seq); p[5]=(uint8_t)n; p[6]=(uint8_t)(n>>8);
    if(n) memcpy(p+7,payload,n);
    diag_put32(p+7+n,diag_crc(p,7+n));
    out(0xc0);
    for(unsigned i=0;i<n+11;i++) {
        if(p[i]==0xc0) {out(0xdb); out(0xdc);}
        else if(p[i]==0xdb) {out(0xdb); out(0xdd);}
        else out(p[i]);
    }
    out(0xc0);
}
typedef struct {
    uint8_t bytes[DIAG_MAX]; unsigned used;
    bool active, escaped, overflow;
    uint32_t crc_errors, length_errors, overflows;
} diag_parser;
typedef void (*diag_frame_handler)(uint8_t, uint32_t, const uint8_t *, unsigned);
static inline void diag_receive(diag_parser *s, uint8_t b, diag_frame_handler handle) {
    if(b==0xc0) {
        if(s->active && s->used && !s->overflow) {
            unsigned n=s->used>=7?(unsigned)s->bytes[5]+((unsigned)s->bytes[6]<<8):251;
            if(s->escaped || n>250 || s->used!=n+11) s->length_errors++;
            else if(diag_get32(s->bytes+7+n)!=diag_crc(s->bytes,7+n)) s->crc_errors++;
            else handle(s->bytes[0],diag_get32(s->bytes+1),s->bytes+7,n);
        }
        s->active=true; s->used=0; s->escaped=false; s->overflow=false; return;
    }
    if(!s->active || s->overflow) return;
    if(s->escaped) {
        s->escaped=false;
        if(b==0xdc) b=0xc0;
        else if(b==0xdd) b=0xdb;
        else {s->length_errors++; s->overflow=true; return;}
    } else if(b==0xdb) {s->escaped=true; return;}
    if(s->used==DIAG_MAX) {s->overflows++; s->overflow=true; return;}
    s->bytes[s->used++]=b;
}
#endif
