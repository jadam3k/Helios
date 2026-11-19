# MPPT controller tests

`control_test.cpp` includes the actual `Config.h` and `Control.h` from the Arduino sketch. It checks configuration, solar qualification, stop/clear, fault latching, invalid/stale samples, voltage/current/temperature trips, completion/recharge, timeout, voltage hold, load hysteresis and timer-counter rollover.

From this directory, build and run with a C++17 host compiler:

```sh
g++ -std=c++17 -Wall -Wextra -pedantic control_test.cpp -o control_test
./control_test
```

Or use a Visual Studio developer shell:

```bat
cl /nologo /W4 /EHsc /std:c++17 control_test.cpp
control_test.exe
```

Recorded result: 25 checks passed, including 88 duty settings checked in both directions of a discrete center-aligned counter model. This models compare relationships; it does not measure dead time at the MOSFET gates.

See [build results](../BUILD_RESULTS.txt) for the Arduino target, core version and memory use. Hardware measurements and charger tests are still needed.
