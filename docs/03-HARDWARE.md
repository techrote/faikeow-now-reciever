# Hardware discovery and flashing

## Target hardware

The intended board is a clone "Pico W" with:

- RP2040 main MCU;
- ESP8266/ESP8285-class 2.4 GHz Wi-Fi coprocessor instead of CYW43439;
- USB connected to RP2040;
- two physical buttons on common versions: RP2040 BOOTSEL plus a radio boot/download button.

The user's boards have been identified as non-CYW43 clones. Exact radio package and board routing remain to be proven on the physical units.

## Relevant public prior art

Two public projects describe this clone family:

1. `JiriBilek/RP2040_PicoW_ESP8285_Library`
   - reports an ESP8285-based "PICO-W";
   - communicates with the radio over RP2040 UART;
   - initializes the link at 115200 baud in its MicroPython driver;
   - uses an RP2040 USB-to-serial UF2 to reflash the ESP8285;
   - uses a second button near the Wi-Fi chip to enter radio download mode.

2. `mocacinno/rp2040_with_esp8285`
   - independently documents the same RP2040 USB-to-serial flashing workflow;
   - supplies a `Serial_port_transmission.uf2` bridge in that project;
   - documents entering ESP8285 download mode with the second button.

These sources demonstrate feasibility, not identity of the user's exact board.

## FNR-002 required characterization

Before production firmware hard-codes the link, establish:

### Radio identity

- chip family/model visible from markings, boot ROM/esptool ID or equivalent;
- flash manufacturer/capacity;
- factory/current firmware state;
- station MAC address.

### Interconnect

Determine:

- which RP2040 UART/SPI/peripheral reaches the radio;
- exact RP2040 GPIO numbers;
- direction of TX/RX or equivalent;
- radio reset control if connected;
- radio boot/strap control if connected;
- whether hardware flow control exists;
- whether any header pins are shared/exposed and therefore constrain use.

Do not infer pins only from generic RP2040 UART defaults.

### Buttons

Record exact behavior of:

- RP2040 BOOTSEL button;
- second radio/download button;
- any reset action.

### Flashing

Establish a reproducible method that can always recover both MCUs:

1. RP2040 -> BOOTSEL UF2 mode;
2. temporary RP2040 USB-to-serial bridge if required;
3. radio -> ROM download mode;
4. `esptool` or pinned equivalent flashes radio;
5. production RP2040 UF2 restored;
6. production radio firmware verified.

Never overwrite the only known recovery path without preserving a tested route back.

### Link rate

Start with a conservative rate known to public prior art if UART is confirmed.

Then test candidate higher rates if useful. Record:

- transmitted bytes/frames;
- duration;
- CRC/parser errors;
- resets;
- power cycles.

Choose the lowest rate comfortably exceeding the real protocol bandwidth with zero observed corruption in the acceptance run.

## Bandwidth reality

v0.1 HID traffic is modest. A compact platform envelope plus relative-mouse payload at ordinary HID report rates should require only a small fraction of a normal UART link.

Do not optimize link speed before correctness. The internal link need only keep up with the accepted platform/profile payload rate plus diagnostics, with headroom for future profiles.

## Power/reset interactions

Test:

- RP2040 reset while radio remains powered;
- radio reset while RP2040 remains powered;
- USB unplug/replug;
- simultaneous cold boot.

Production state machines must not depend on a fragile power-up race.

## Hardware evidence file

FNR-002 should create a committed board contract such as:

`docs/BOARD-CONTRACT.md`

Include photos/diagrams only if licensing/privacy and repository size are appropriate; otherwise record text measurements and identifiers.
