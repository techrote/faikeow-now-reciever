#include "fnr/radio_ingress.h"
#include <cassert>
#include <climits>
#include <cstring>
int main() {
    fnr_radio_ingress r{};
    const uint8_t peer[6]={2,3,4,5,6,7}, wrong[6]={4,3,4,5,6,7};
    uint8_t bytes[256]{};
    fnr_radio_config cfg{6,{2,3,4,5,6,7},true};
    assert(fnr_radio_config_valid(&cfg));
    cfg.channel=0; assert(!fnr_radio_config_valid(&cfg)); cfg.channel=6;
    cfg.allowed_mac[0]=3; assert(!fnr_radio_config_valid(&cfg)); cfg.allowed_mac[0]=2;
    cfg.enabled=false; assert(!fnr_radio_config_valid(&cfg)); cfg.enabled=true;
    fnr_radio_reset(&r,nullptr);
    assert(r.state==FNR_RADIO_UNCONFIGURED);
    fnr_radio_receive(&r,peer,bytes,1);
    fnr_radio_reset(&r,&cfg);
    assert(r.state==FNR_RADIO_STARTING);
    fnr_radio_transition(&r,FNR_RADIO_READY);
    fnr_datagram copy{};
    assert(!fnr_radio_take(&r,&copy));
    fnr_radio_receive(&r,wrong,bytes,2);
    fnr_radio_receive(&r,nullptr,bytes,2);
    fnr_radio_receive(&r,peer,nullptr,2);
    fnr_radio_receive(&r,peer,bytes,0);
    for (unsigned n=251;n<=255;++n) fnr_radio_receive(&r,peer,bytes,n);
    assert(r.counters.rejected_source==1 && r.counters.invalid_length==8);
    for (unsigned n=1;n<=250;++n) {
        for (unsigned i=0;i<n;++i) bytes[i]=static_cast<uint8_t>(i+n);
        fnr_radio_receive(&r,peer,bytes,n);
        std::memset(bytes,0xff,n);
        assert(fnr_radio_take(&r,&copy));
        assert(copy.size==n && std::memcmp(copy.source_mac,peer,6)==0);
        for (unsigned i=0;i<n;++i) assert(copy.payload[i]==static_cast<uint8_t>(i+n));
        assert(!fnr_radio_take(&r,&copy));
    }
    for (unsigned i=0;i<100000;++i) {
        bytes[0]=static_cast<uint8_t>(i);
        fnr_radio_receive(&r,peer,bytes,1);
        if (i%3==0) {
            assert(fnr_radio_take(&r,&copy));
            assert(copy.payload[0]==bytes[0]);
        }
    }
    auto snapshot=r;
    fnr_radio_snapshot(&r,&snapshot);
    assert(snapshot.counters.accepted==r.counters.accepted);
    assert(r.counters.dropped>0);
    assert(r.counters.accepted==r.counters.consumed+r.counters.dropped+(r.pending?1u:0u));
    r.counters.received=UINT32_MAX;
    fnr_radio_receive(&r,wrong,bytes,1);
    assert(r.counters.received==UINT32_MAX);
    fnr_radio_reset(&r,&cfg);
    assert(!r.pending && r.counters.restarts>=1);
    assert(!fnr_radio_take(&r,&copy));
    fnr_radio_transition(&r,FNR_RADIO_ERROR);
    assert(r.counters.errors==1 && r.state==FNR_RADIO_ERROR);
}
