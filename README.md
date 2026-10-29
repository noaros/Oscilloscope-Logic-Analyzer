# Oscilloscope-Logic-Analyzer

I just purchased an oscilloscope with a signal generator, and plan to use the latter to practice reading signals with both the oscilloscope and a logic analyzer I also have.

See [EXERCISES.md](EXERCISES.md) for a progression of hands-on exercises using the scope, its signal generator, the Innomaker LA1010 and a Nucleo-F429ZI board together.

The [firmware/](firmware/) folder holds bare-metal (register-level, no HAL) programs for those exercises, built with `arm-none-eabi-gcc` and `make`.
