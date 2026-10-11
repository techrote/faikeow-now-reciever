#include "fnr/platform.h"
#include "fnr/profile.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); std::abort(); } } while (0)
int main() {
    uint8_t mac[6] = {1,2,3,4,5,6};
    uint8_t data[512];
    for (unsigned i=0; i<512; ++i) data[i] = static_cast<uint8_t>(i);
    fnr_datagram out{};
    unsigned accepted=0, rejected=0;
    for (size_t n=0; n<=512; ++n) {
        std::memset(&out, 0xa5, sizeof(out));
        fnr_datagram before; std::memcpy(&before,&out,sizeof(out));
        const auto result=fnr_datagram_copy(&out,mac,data,n);
        if (n>=1 && n<=250) {
            CHECK(result==FNR_COPY_OK); CHECK(out.size==n);
            CHECK(std::memcmp(out.source_mac,mac,6)==0);
            CHECK(std::memcmp(out.payload,data,n)==0);
            for (size_t i=n; i<250; ++i) CHECK(out.payload[i]==0);
            ++accepted;
        } else {
            CHECK(result==FNR_COPY_INVALID);
            CHECK(std::memcmp(&out,&before,sizeof(out))==0); ++rejected;
        }
    }
    CHECK(fnr_datagram_copy(nullptr,mac,data,1)==FNR_COPY_INVALID);
    CHECK(fnr_datagram_copy(&out,nullptr,data,1)==FNR_COPY_INVALID);
    CHECK(fnr_datagram_copy(&out,mac,nullptr,1)==FNR_COPY_INVALID);
    CHECK(fnr_datagram_copy(&out,mac,data,SIZE_MAX)==FNR_COPY_INVALID);
    CHECK(fnr_datagram_copy(&out,mac,data,250)==FNR_COPY_OK);
    data[0]=99; mac[0]=99;
    CHECK(out.payload[0]==0); CHECK(out.source_mac[0]==1);
    fnr_profile_ops profile{}; CHECK(profile.accept==nullptr);
    std::printf("PASS lengths=513 accepted=%u rejected=%u; null=3; SIZE_MAX=1; ownership=2\n", accepted,rejected);
}
