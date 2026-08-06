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
