# FNR-007 final evidence and security review

Review date: 2026-10-13, Europe/London. Owning issue #8 / PR #22.
Qualified firmware source: `9d813d7143124a8dcadc3b2a339db00ac792392b`.
Subsequent candidate changes are evidence/documentation only; firmware, shared
code, diagnostics, build tooling, dependency lock and CI configuration match
that source. The final PR head and exact-head CI are recorded in PR #22.

No unresolved substantive finding remains after the three physical defects
below were repaired and the affected phases repeated. This is an agent review;
it is not presented as an independent human approval.

| Boundary | Review and evidence |
| --- | --- |
| Public record | Exact 40-byte bounds before reads; magic/version/flags/reserved/CRC-32/IEEE checked; semantic validation before publishing a candidate. Generic numeric profile ID/schema. Golden, corruption and deterministic hostile tests retained. |
| Secret separation | One local configuration generates matching public records; only ESP source contains the deployment key. Factory accessors return null/zero. RP UF2/BIN/ELF inspected for both deployment keys and derived authentication keys; generated secret sources and local inputs are untracked/ignored. Redacted manifests and diagnostics report presence, never values. |
| Radio authentication | Production requires successful encrypted peer registration and a mandatory bounded FNA1 wrapper. HMAC-SHA-256 uses a distinct derived key and binds config ID/generation, both MACs, header and enclosed packet. Actual target known-answer tests run before READY. All 16 tag bytes are compared. No unwrapped production fallback; legacy plaintext seam is isolated diagnostic code. |
| Exact source and profile | ESP rejects wrong MAC before crypto/forwarding. Physical wrong-source rejection increments ESP counters; RP sees zero packets, so its authorization counter remains unchanged. Native RP tests establish its separate MAC gate. Valid wrong-profile traffic reaches RP and is rejected before session/profile mutation. Correct traffic works afterward without receiver reflash. |
| Staging and supported schemas | The target checks its registered profile/schema before staging. Staging prepares a disabled candidate and publishes metadata only after core reconfiguration succeeds. CRC-valid unsupported schema fails closed after repair. |
| READY and recovery | RP starts staged with both gates disabled; matching READY/channel enables them. Link/radio faults revoke acknowledgement, invalidate freshness and submit neutral HID barriers. Temporary admission transitions preserve session/sequence/retired-session history. Actual reconfiguration resets state. Physical 5000 ms interruption, fresh READY recovery, stale/duplicate rejection and fresh traffic pass after repair. |
| Factory/reset/reconfiguration | Normal unprovisioned production images produce no output under bare/encrypted/wrong-source traffic. Observer reports EMPTY, radio OFF, disabled gates and no admission. Key absence is separately established from source/images. Both documented MCU recovery paths work without soldering. Config 2 has a distinct key/identity/generation; old relationships fail and new operation passes. Final normal production pair is restored and cold-booted. |
| Channel and SDK limits | Channel is configuration, never authentication/isolation. This 180 cm requested-channel mismatch measurement recorded no reception; retained FNR-004 mismatch reception remains valid. SDK callback cannot attest per-packet encryption. A key-holding sender's valid wrapper sent without link encryption was accepted and is explicitly retained as a limitation, not an encrypted-only claim. |
| Evidence consistency | Public record, key presence, radio READY, RP acknowledgement, packet/profile admission and HID output are separate observations. Source/artifact/lock/toolchain identities are retained. Original firmware failures and regression-first CI failures remain. Host predicate corrections retain original results and explicit reconciliations. |

The original plaintext admission defect was reproduced physically and in the
actual adapter test before the operator-authorized wrapper repair. The second
regression-first change reproduced unsupported-schema admission and ordering
loss across acknowledgement transitions before their minimal RP repairs.
Exact source CI passed after each repair. No FNR-005 wire change or generic-core
profile payload semantics were introduced.

The wrapper leaves 228 enclosed platform bytes / 210 profile bytes within the
250-byte ESP8266 limit. It does not provide secrecy for deliberately plaintext
link traffic, secure boot, resistance to firmware extraction, persistent replay
protection across receiver restarts or cryptographic wall-clock freshness.
FNR-006 retains session/order/freshness authority. FNR-008 must implement wrapper
v1 in its real sender and independently qualify TiltMouse interoperability,
button behavior, UX and performance; none is claimed here.

Final merge gates remain procedural: exact final-head push and PR CI green,
current/mergeable against main, PR ready, squash merge, verified resulting main
and green post-merge main CI, then tracker update and issue closure.
