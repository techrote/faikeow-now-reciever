# FNR-007 RP admission observer

Qualification firmware with distinct composite USB identity `cafe:f007`.
It is not the production HID image (`cafe:f003`). It compiles the production
receiver, provisioning, transport and USB/profile adapters unchanged, adding
bounded CDC observation and successful HID-submission counters. Production
profile report bytes/descriptor are reused. The fixture owns its composite USB
descriptors and foreground command loop. It contains only public provisioning.

Build with the accepted Pico SDK/board/toolchain settings, an exact
`FNR_SOURCE_ID` and explicit `FNR_PROVISIONING_SOURCE` pointing to the generated
RP public source (or factory source). This flash image can survive a power cycle;
restore the production artifact after qualification. Use the hardware
authority's guarded BOOTSEL procedure and exact readback, never guessed pins.

CDC commands:

- `INFO` / `STATS`: one bounded JSON snapshot, no key access. Captures initial
  staged/ack/admission state before the first transport foreground iteration;
  reports current state, radio status, RP admission/order/profile counters and
  successful HID submissions. Raw board UID stays in private captures.
- `LINKOFF 3500` (allowed 3500..6000 ms): temporarily disables UART0 RX IRQs
  while the actual foreground parser/watchdog and USB tasks continue. IRQs
  automatically resume. This is a bounded software fault on the real internal
  link, not a physical electrical disconnect or new reset-pin discovery.
- `BOOTSEL`: enter the RP ROM using the accepted SDK recovery call.

There is one 3072-byte output slot, at most 128 output bytes and 16 input bytes
per loop, and a 32-byte command buffer. No packet injection or RF synthesis is
performed by this observer. Evidence must distinguish its counter/state
observations from separate exact production RF-to-USB tests.
