# Temporary candidate UART bridge (SRAM only)

Source-built FNR-002 diagnostic, not production radio transport. It forwards raw
USB CDC bytes to UART0, GP0 TX / GP1 RX, 8N1 with no flow control. The pin route
is a hypothesis until ROM identification succeeds independently on each board.
No reset, boot strap or other unknown GPIO is driven. GP0 uses 2 mA drive.
DTR/RTS have no radio effect; use the separately observed physical boot sequence.
CDC baud controls UART baud (9600–921600); 1200 requests RP2040 BOOTSEL.
No text/logging is inserted into the transparent byte stream.
CDC byte forwarding remains active with DTR deasserted: esptool 4.8.1 opens
ports with DTR/RTS false. The Pico SDK stdio default gates on DTR and causes
write timeouts in that case; this diagnostic explicitly disables that gating.

Use the observer build procedure with this source directory, a separate build
directory, and `-DFNR_ALLOW_CANDIDATE_UART=1`. `PICO_NO_FLASH=1` is mandatory.
Validate the UF2 with `tools/hardware/check_ram_uf2.py` before loading.
Preserve factory flash and physically identify the one connected board first.
The bridge itself has no flash operations; host esptool commands can write radio
flash and must be restricted to identity/read commands until backups and recovery
have been established. Use pinned esptool 4.8.1 `--before no_reset --after no_reset`.

Source rationale:

- https://github.com/JiriBilek/RP2040_PicoW_ESP8285_Library (UART0 at 115200).
- https://github.com/mocacinno/rp2040_with_esp8285 (UART0 and far-button download).
- Board A passive observation: GPIO1 low in initial boot state and high after
  far-button reconnect; GPIO0 high in both; no output drivers enabled by observer.

The blocking stdio/USB adapter is appropriate for conservative ROM discovery,
but is not an integrity-qualified production bridge or an overrun measurement
tool. Qualification firmware must add hardware UART error counters and explicit
bounded buffering before drawing sustained-throughput conclusions.
