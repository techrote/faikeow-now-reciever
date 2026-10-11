#include "link_diag.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); std::abort(); } } while (0)
static std::array<uint8_t, 600> wire;
static size_t used;
static unsigned accepted;
static uint32_t expected_seq;
static unsigned expected_length;
static std::array<uint8_t,250> expected;
static void write_byte(uint8_t b) {CHECK(used<wire.size()); wire[used++]=b;}
static void receive_frame(uint8_t type,uint32_t seq,const uint8_t *data,unsigned n) {
    CHECK(type==DIAG_PING); CHECK(seq==expected_seq); CHECK(n==expected_length);
    CHECK(std::memcmp(data,expected.data(),n)==0); accepted++;
}
static void feed(diag_parser &parser) {
    for(size_t i=0;i<used;i++) diag_receive(&parser,wire[i],receive_frame);
}
int main() {
    CHECK(diag_crc((const uint8_t *)"123456789",9)==0xcbf43926u);
    diag_parser parser{};
    for(uint32_t seq=0;seq<256;seq++) {
        expected_seq=seq; expected_length=diag_length(seq);
        diag_payload(expected.data(),expected_length,seq,false);
        CHECK(diag_pattern(expected.data(),expected_length,seq,false));
        used=0; diag_send(write_byte,DIAG_PING,seq,expected.data(),expected_length); feed(parser);
    }
    CHECK(accepted==256 && parser.crc_errors==0 && parser.length_errors==0);
    expected_seq=17; expected_length=250; expected.fill(0xc0);
    expected[0]=0xdb; used=0; diag_send(write_byte,DIAG_PING,17,expected.data(),250);
    CHECK(used>500); feed(parser); CHECK(accepted==257);
    // Corrupt an unescaped header byte; integrity checking must reject it.
    wire[2]^=0x10; feed(parser); CHECK(parser.crc_errors==1 && accepted==257);
    diag_receive(&parser,0xc0,receive_frame);
    for(unsigned i=0;i<DIAG_MAX+20;i++) diag_receive(&parser,0x55,receive_frame);
    diag_receive(&parser,0xc0,receive_frame); CHECK(parser.overflows==1);
    diag_receive(&parser,0xdb,receive_frame); diag_receive(&parser,0x33,receive_frame);
    diag_receive(&parser,0xc0,receive_frame); CHECK(parser.length_errors==1);
    // After malformed and truncated input, a fresh delimiter restores framing.
    diag_receive(&parser,0x11,receive_frame); diag_receive(&parser,0xc0,receive_frame);
    CHECK(parser.length_errors==2);
    used=0; diag_send(write_byte,DIAG_PING,17,expected.data(),250); feed(parser);
    CHECK(accepted==258 && parser.crc_errors==1 && parser.overflows==1);
    used=0; diag_send(write_byte,DIAG_PING,0,expected.data(),251); CHECK(used==0);
    puts("PASS: diagnostic CRC vector, escaped max payload, corruption, bounds and resynchronization");
}
