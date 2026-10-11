#include "fnr/platform.h"
#include "fnr/profile.h"
_Static_assert(sizeof(void *) == 4, "ARM probe must be ILP32");
_Static_assert(sizeof(uint16_t) == 2, "size field");
_Static_assert(FNR_DATAGRAM_CAPACITY == 250, "ESP8266 interoperability bound");
/* Do not assert struct packing: this is not a serialized ABI. */
unsigned fnr_target_owned_record_size(void) { return sizeof(fnr_datagram); }
