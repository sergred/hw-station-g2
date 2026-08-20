# License

This repository, **hw-station-g2** (the board-support straddle that makes the
B&Q Station G2 usable by the `reticulous/reticulous` buildable), is released
under the **Apache License, Version 2.0**.

Full license text: <https://www.apache.org/licenses/LICENSE-2.0>

Copyright (c) 2026 by reticulous project contributors.

## Third-party software

### Vendored in this repository

None. This repository carries only board-support code: the pin declarations
(`straddle.yaml`), the bring-up Service, the GPS task (a port of
`hw-lilygo-tdeck`'s, same project, same license), and the detect probe.

### What a build with this board pulls in

A device image is assembled by the `reticulous/reticulous` buildable, which
stages this straddle alongside the platform and mesh straddles; every
third-party component that lands in the built artifacts (RadioLib for
iface-lora, ESP-IDF itself, the browser SPA's npm tree, …) is declared and
licensed by the straddle that owns it — see the buildable's `LICENSE.md` and
each sibling repo's own. This board straddle adds **no third-party managed
dependency of its own** (the SH1107 OLED, when it lights up, goes through
`spangap/tinylcd`'s u8g2, declared there).
