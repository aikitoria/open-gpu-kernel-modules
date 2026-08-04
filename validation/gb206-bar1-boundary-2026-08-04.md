# GB206 BAR1 boundary validation — 2026-08-04

## Revisions

- Diagnostic branch: `test/gb206-bar1-boundary-pressure`
- Diagnostic tag/commit: `test/gb206-bar1-boundary-d0676cfc` / `d0676cfc`
- Production branch: `production/runtime-bar1-coverage`
- Production module commit: `d682adc5`
- Driver/kernel: `610.43.03` / `7.0.0-28-generic`
- Secure Boot signer: `sugardaddy Secure Boot Module Signature key`

The production branch does not contain the diagnostic logging, CUDA boundary
harness source, or generated harness binary.

## Diagnostic module identity

| Module | SHA-256 | srcversion |
|---|---|---|
| `nvidia.ko` | `04925f26b64b5095705cf98f2e4fefd405ec300220fe340e2c0f07220553d346` | `9020898A6D608A1C767CC72` |
| `nvidia-uvm.ko` | `58ec9925164d3dc39d5a6b54076792fe3b80548c2b0fb3ef93501d9fd01bf4f9` | `1BD7F0E70C0717835738BDB` |

Both modules used vermagic `7.0.0-28-generic SMP preempt mod_unload modversions`.

## Natural geometry and deterministic matrix

The test used natural BAR1 geometry only. The invalidating 4 GiB override was not
used.

- Full aligned coverage: `selectedStatic=0x3e1000000`, aligned client FB
  `0x3e1000000`.
- Display/console partial coverage: `selectedStatic=0x3dfe00000`, raw client FB
  `0x3e10a0000`, console reservation `0x260000`, static offset `0x20000000`.
- Inside tests passed in both GPU directions and both peer-enable orderings,
  including peer kernels, `cudaMemcpyPeer`, and beginning/middle/end probes.
- Full-coverage pressure reached normal allocation exhaustion after 1,969 passing
  tests with no unsafe classification or data error.
- The partial side produced the same 64 MiB spanning range in both orderings:
  `minOffset=0x3dcc00000`, `maxEnd=0x3e0c00000`,
  `dmaSize=0x3dfe00000`. Peer-before rejected during allocation; peer-after
  rejected during peer enablement. Each had 491 prior passing tests.
- Prepared boundary mode reported an outside range
  `0x3e0c00000..0x3e1000000`, rejected it with `localHealthy=1`, and passed the
  three-iteration recovery.

## Rejection/recovery cycles

From `2026-08-04T14:53:04-07:00` through
`2026-08-04T15:02:57-07:00`, 100 independent prepared-boundary cycles completed.
Every cycle required:

- one spanning candidate rejection;
- one outside rejection with `localHealthy=1`;
- `rejected=2 apiErrors=2 dataErrors=0`; and
- a three-iteration inside recovery.

Both GPUs returned to zero MiB used at every ten-cycle checkpoint. The cycle
window contained no assertion, Xid, mailbox setup failure, IOMMU/AER fault,
stale state, invalid state, or cleanup warning.

The individual logs and their checksum manifest were written under `/tmp` and
were cleared by the required reboot. The pass count, checkpoints, and journal
result were captured before reboot; this file records the durable summary.

## Production policy and routing audit

The production predicate is additive: property-enabled GPUs use display-aware
placement only when runtime geometry covers all aligned client FB, while GB206
retains the tested partial-window exception. Other partial geometries are not
generalized. `make -C tests check` runs `tests/bar1_p2p_policy_test.c` to verify
the policy truth table and that the existing exception cannot become a
rejection.

Generated HAL dispatch, the global `pcieP2PType` default, registry precedence,
and the GH100 BAR1 routing source were unchanged. Their pre/post hashes matched.
The live BAR1 encoder bounds checks remain present.

## Signed production modules

Installed path: `/lib/modules/7.0.0-28-generic/updates/local/`.

| Module | SHA-256 | srcversion |
|---|---|---|
| `nvidia.ko` | `9bf54a3665eea55e839b93feb44f4c92c994db88baf7e31331341ab1d712a626` | `9020898A6D608A1C767CC72` |
| `nvidia-modeset.ko` | `8e3217b97a4432cc9b84f5d1eb475d8f76f0fc7985565bd4e20a986f1a3a65ef` | `0BCB09E2E1D4422BB162693` |
| `nvidia-drm.ko` | `7402ab6be81127e636d5bd29b9920a9aaa204c6bc5991848781add2a7fa75b9f` | `65769FC23A53EFDFC4A2DB5` |
| `nvidia-uvm.ko` | `b5c2d10652954ff40c566b596ebbf2d56b094d6be722f5123de37bf29da3dfea` | `1BD7F0E70C0717835738BDB` |
| `nvidia-peermem.ko` | `61363a4d58f1f0fccf29b944b833e35502e0a584a64fd5202190b18858b4f9ba` | `05E8CF2F419E46C7D3D974E` |

## Production validation

- Full module build and source policy regression: passed.
- `nvidia-smi`: both RTX 5060 Ti GPUs healthy.
- P2P read/write capability: `OK` both directions.
- P2P atomics: `NS`, not `DR` (not disabled by the registry default).
- `simpleP2P`: passed before reload, after reload, after resume, and after reboot;
  13.09 GB/s.
- `p2pBandwidthLatencyTest`: 14.09 GB/s each unidirectional P2P direction and
  27.79–27.80 GB/s bidirectional.
- Both peer-enable orderings passed bidirectional inside correctness.
- The known partial-boundary sequence rejected in both orderings and recovered
  immediately.
- Modeset/DRM stack load and full driver unload/reload: passed.
- Deep suspend via the enabled NVIDIA systemd suspend/resume hooks: entered at
  15:17:13 and exited at 15:17:39; post-resume P2P passed.
- Boot from the updated initramfs: candidate hashes/signer matched and post-boot
  P2P passed. No GPU-specific boot journal fault was present.

## Suspend configuration notes

`NVreg_PreserveVideoMemoryAllocations=1` correctly requires the NVIDIA procfs
suspend hook; a raw `rtcwake -m mem` attempt was rejected, while the supported
systemd path succeeded. `NVreg_TemporaryFilePath=/var` selects the root filesystem
on `/dev/nvme6n1p5` for preservation files, but the journal I/O errors referenced
the separate `/dev/nvme6n1p3` partition. That partition is intentionally inaccessible
while OPAL-locked for BitLocker, so those messages are expected and are unrelated to
the NVIDIA validation.

The restricted Codex mount namespace exposes `/` with a read-only VFS mount flag
while the ext4 filesystem reports `rw`; this does not indicate that the host root
filesystem was remounted read-only. No storage failure is inferred from this test.
