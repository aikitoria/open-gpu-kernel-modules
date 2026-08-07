# BAR1 policy tests

Run the source-level BAR1 P2P policy regression test with:

```sh
make -C tests check
```

The test verifies the runtime-coverage truth table. Display-aware placement is
available only when BAR1 P2P is enabled by the existing device property and
both the aligned client framebuffer and available static BAR1 window are
non-empty. Partial, exact, and larger-than-client coverage are accepted without
an implementation-specific exception.

It also verifies that the experimental non-coherent DMA-BUF GDR exception
remains default-off, does not replace the stock coherent path, and requires
FORCE_PCIE, static BAR1, and the existing BAR1/MIG exclusions. Its range checks
cover inside, spanning, outside, empty, and overflowing layouts.

The topology-policy test keeps the non-coherent importer exception limited to
the default-off experiment, a Linux-approved P2PDMA path, an identity IOMMU
domain, and an importer DMA mask that covers the complete BAR1 resource. Each
eligibility predicate has an explicit negative case.
