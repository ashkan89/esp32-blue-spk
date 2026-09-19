# Product implementation

The dashboard keeps all existing cards and at-a-glance information. New features add useful cards; existing cards are not moved into Settings.

## Work batches

- [x] Audio configuration ownership, safe volume policy and DSP controls.
- [x] Target-checked/signed update packaging and delayed health confirmation.
- [x] Unique provisioning, authentication hardening, bounded requests and verified stream TLS.
- [x] Persistent scenes, local favorites, named DFPlayer catalog and artwork.
- [x] Ring overlays, sunrise/alarm wake, focus timer and soundscapes.
- [x] WROVER time-shift buffer and station alternatives.
- [x] Card-preserving dashboard improvements, accessibility and bounded polling.
- [x] Support export, factory checks, optional physical controls and hardware integration documentation.
- [x] Reproducible release tooling, CI, tests and build budgets.
- [ ] Physical qualification on both assembled boards (software builds/checks are recorded in VALIDATION.md).

Physical amplifier/charger/enclosure changes require assembly. Firmware can provide integration hooks, wiring requirements and acceptance procedures, but cannot perform or certify those hardware changes. Production signing keys belong to the owner; release tooling must fail closed when they are absent.
