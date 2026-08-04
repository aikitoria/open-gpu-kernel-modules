# BAR1 policy tests

Run the source-level BAR1 P2P policy regression test with:

```sh
make -C tests check
```

The test verifies the runtime-coverage truth table, including that the GB206
partial-window exception remains accepted and that full-coverage devices are
added only when BAR1 P2P is enabled by the existing device property.
