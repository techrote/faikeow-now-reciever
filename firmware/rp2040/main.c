#include "pico/stdlib.h"
#include "fnr/platform.h"
#include "fnr/profile.h"
extern bool (* volatile fnr_usb_api_anchor)(void);
int main(void) {
    (void)fnr_usb_api_anchor; /* Read only; never call or enumerate. */
    for (;;) tight_loop_contents();
}
