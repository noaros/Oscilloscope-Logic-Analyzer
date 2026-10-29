# Handoff

**Still to do:** none of the firmware in `firmware/` has been tested on the Nucleo-F429ZI yet. It only compiles cleanly (`make all`).

Test `ex04` first: `make flash EX=04` in `firmware/`, then check that PC9 reads **36 MHz** on the scope. That confirms `clock_init_180mhz()` in `common/clock.c`, which every exercise from `ex05` on depends on.
