# FNR-005 implementation and evidence review

Review performed in this qualification session after all representative-DUT
cases. Firmware source is d6766e8add3a7a659dcb8ea4d87d4f68d628995a. The final
host/evidence commit containing this review must have its own exact-head CI;
PR #20 records that SHA and the subsequent merge/main verification. This is a
session review, not an implied independent maintainer approval.

## Protocol arithmetic and validation

The application limit is 250 bytes; source MAC adds six, the explicit header
adds 16 and CRC adds two: 274 decoded bytes. Canonical COBS bound
`n + floor(n/254) + 1` gives 276 encoded bytes, then one delimiter gives 277
wire bytes. Encoder capacity checks reserve the delimiter and check before each
write; decoder uses subtraction-based remaining-length checks. Declared payload
must match the exact decoded length before CRC/payload publication. Multibyte
fields and CRC are explicitly little-endian; CRC covers header plus payload.
Decoded output is published only on complete validation. The physical campaign
includes an actual valid 277-byte frame, exact encoded-capacity rejection and
oversize/resynchronization, supplementing independent native vectors/properties.

Sequence increment uses defined uint32 wrap. A discontinuity is diagnostic and
invalidates old pending state; this layer deliberately does not implement sender
duplicate/order/freshness rejection. Generation is a local transport lifecycle
discriminator, not a globally unique/persistent sender session. RESET status and
UART/silence events provide independent boundaries. Tests cover local wrap and
signed/unsigned/length limits. The strict Arm enum warning was fixed by a bounded
unsigned comparison that still rejects invalid enum values.

## Ownership, concurrency and recovery

ESP callback ingress copies into fixed storage and performs no transport UART
write. Foreground takes an owned copy. TX keeps an immutable partial active frame
and bounded newest-wins pending data/status; restart counts aborted ownership and
emits delimiter plus RESET status before new-phase data. Status snapshots preserve
pending RESET. Pinned core FIFO availability reports 128-count while count 127 is
full; reserving one byte and writing at most 32 per loop avoids the off-by-one
busy wait. Physical overload isolates 253 ESP replacements from RP loss, and
active restart independently reconciles one partial-frame abort and old RP-slot
retirement. Sender aborts are never used as receiver-drop counts.

RP IRQ reads at most 32 words and only owns UART/ring state. The 1024-byte ring
and 512-byte foreground budget are fixed. Ring take/snapshot/fault synchronization
mask interrupts; parser work occurs in foreground. A gap arriving during parsing
is synchronized before consumer publication/take, invalidating unsafe slots.
Link liveness expires before buffered bytes can refresh an old phase. Service
stall ordering also handles repeated stalls while already timed out. Consumer
getters synchronize timeout/loss before returning owned data. Sticky events
form a take barrier, survive replacement and precede generic consumer use.
Native adapter regressions and physical foreground/consumer stalls, ring overflow,
actual UART overrun and reset cases support these properties.

Parser reset discards through the next delimiter. Both an actual partial segment
and a complete unconsumed slot were retired physically. Complete-slot reset also
discarded one automatic telemetry frame through resynchronization; its 201
nonzero bytes and the source/parser frame difference are explicitly accounted,
and the next requested production telemetry was consumed. Invalid delimiters
continued across 3000 ms without refreshing liveness. No invalid opaque payload
escaped in any corruption/reset phase.

CPU reboot has distinct volatile ESP epochs and ROM UART interruption. The
qualified cold-boot case exposes 20 framing batches, one break batch, 79 retired
ring bytes and 20 discontinuities before new generation/RESET status. There is
no subsequent fault or stale delivery; sequence-1 telemetry and a clean burst
follow. Whole-reset unobserved source/TX state is not mislabeled receiver loss.
Independent RP reload retains ESP generation, clears the old consumer epoch and
delivers an exact fresh suffix. Its outage interval is intentionally unobserved;
old pre-ROM snapshots are not falsely treated as final old-epoch totals.

## Diagnostic fidelity and false-PASS review

The separately named images include actual production adapter sources and link
the shared codec/TX/ring/parser. Normal TX uses the real foreground APIs and UART
adapter; diagnostic UART RX, CDC control, bounded counters and raw fault writer
are explicit fixture differences. Corruption bypasses serialization solely to
damage the wire, and still traverses the real RP IRQ/ring/parser/handoff. No
FNR-002 SLIP/CRC32 result is substituted for production framing evidence.

Guard fixes rejected the SDK SRAM UID placeholder and the initially uninitialized
QSPI read before any campaign stimulus. Accepted read-only flash identity and
QSPI initialization now verify the actual selected DUT. Exact artifact hashes,
source/lock, deployment/readback records, fresh topology and both live identities
remain mandatory. Later host/evidence commits retain the actual firmware source
only if it is an ancestor and compiled inputs are unchanged. No image is relabeled
as a later source commit or programmed into the second board for duplication.

Assessment was strengthened to require drained initial/final ownership,
generation/sequence/source agreement, independent stage counts, wire-size and
UART/ring/parser byte reconciliation, status/data ownership, explicit fault
retirement/events and exact fresh suffixes. Unsupported-version/type/length
stimuli require the expected parser rejection reason. CPU boot faults must stop
at the observed new-generation boundary; reset is not an excuse for later errors.
The 44 physical PASS observations replay independently of their stored PASS
labels. Synthetic mutations reject duplicate delivery, missing events, stale
digests, hidden sender loss, wrong rejection reasons and post-boundary faults.
Synthetic tests are not additional physical evidence.

The immediate post-serial-upload CPU-reboot failure and the first cold retry's
watchdog/capture timing failure remain retained. Ordinary cold boot is an explicit
fixture precondition documented by the pinned core; the bounded retry passed.
The criteria for clean sustained framing were never relaxed. No second 600-second
campaign was required for identical Board B or later host/evidence-only edits.

## Review conclusion

No substantive implementation/evidence finding remains unresolved in the tested
scope. All fixes affecting firmware preceded the d6766e8 physical campaign;
later changes strengthen assessment, guards and documentation. All 44 passed
phases satisfy the final assessment, including the additional complete-slot reset
and continuing-junk watchdog cases identified in the authority audit. Full
exact-head native/sanitizer/adapter/regression/target/diagnostic/linked/reproducibility
CI remains mandatory before squash merge; post-merge main CI precedes issue closure.

FNR-006 sender/session/freshness/profile semantics and incoming-radio HID remain
unimplemented. FNR-004 channel isolation, FNR-007 provisioning/security and
FNR-008 reference interoperability are not newly established. Final fixture is
stopped/drained, handles closed, original backups preserved; Board B was unused.
