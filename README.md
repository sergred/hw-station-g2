# hw-station-g2 — B&Q Station G2 board HAL

**hw-station-g2** is the board-support straddle for the **B&Q Consulting
Station G2** — a high-power ESP32-S3 base-station node (16 MB flash, 8 MB
**octal** PSRAM, native USB) carrying one Semtech **SX1262** on its own SPI
bus behind an **always-in-path RF front end**: a 35 dBm-P1dB TX power
amplifier plus an 18.5 dB ultra-low-NF RX LNA, switched by the SX1262's own
DIO2. It makes the board usable by an application: it owns the LoRa CS park,
the program-button pull and the optional GROVE GNSS receiver, and it publishes
the board's pin map, FEM declaration and hardware tuning as Kconfig. Board
reference: <https://wiki.bqvoy.com/en/meshtastic/station-g2>. The product is
**discontinued** but widely fielded — exactly the sort of mains-powered
always-on node a mesh wants as infrastructure.

It is a **non-buildable** component — it decides nothing about what the device
*does*. A buildable assembler (`reticulous/reticulous`) adds it and inherits
the board: `spangap build reticulous/reticulous --with spangap/hw-station-g2`.
The mesh stack, the IP/web platform, `app_main`, the partition layout, the
update story and the browser SPA all come from the buildable and its other
straddles — not from here.

Wired: **LoRa (with the fixed FEM declaration)**, the **optional GROVE GNSS
module** (off by default), and the 1.3" SH1107 OLED via
[tinylcd](../tinylcd)'s paged status UI (staged in `straddle.yaml`; tinylcd
ships in this bundle as a clean-room straddle with the SH1107 controller
select this board needed — the panel itself is **untested until flashed**,
see the checklist below). The console is the platform's native-USB default
(USB-Serial-JTAG, VID 0x303A / PID 0x1001). No battery and no battery ADC, no
SD card, no firmware-gated power rail.

## Hardware verification — first power-on done (2026-08-20)

This straddle was assembled from Meshtastic's `station-g2` variant files, the
B&Q wiki and its PA conduction-test table, then verified on a physical
Station G2 (ESP32-S3 QFN56 rev v0.2, 16 MB quad flash, 8 MB embedded PSRAM,
plain-USB power, esptool flash over the native USB-Serial-JTAG at COM level):

- ✅ **`detect_hw`**: `detect: hw_station_g2 found` on first boot — the SH1107
  ACKs at 0x3C/0x3D on SDA 5 / SCL 6 with **no rail drive and no reset
  pulse**, and `detect_radio_is` confirms the SX1262 on the LoRa header.
- ✅ **SX1262 `begin()`** with the DIO3-supplied 1.8 V TCXO:
  `lora/0: SX1262 found (cs=11 irq=48 busy=47 rst=21)`, radio `up` at
  869.525 MHz / BW 125 / SF7 / CR4:5.
- ✅ **FEM declaration end to end**: `txp 14` accepted as antenna dBm (chip
  drive −6 via the fixed +20 dB conversion), `lora.0.tx_power_max` published
  as 35, announces transmitted (`tx 3/501 B`, airtime ledger counting).
- ✅ **RX path alive**: CSMA noise floor measured (−92 dBm on ch0, busy
  threshold −86) through the always-on LNA.
- ✅ **Partition/state floor**: `/state` mounted as 10 240 kB at 0x600000,
  first-boot factory copy OK.
- ✅ **GPS-absent path**: task up, `state: off`, no probe churn (module not
  fitted, `s.gps.enable=0` default).
- ✅ **rnsd integration**: `register: iface=lora/0 mtu=500 bitrate=4000`,
  `rns.ready: 1`, hosted announce replayed onto the radio.

Still open (needs equipment / peers / a fitted module):

- **PA output vs the mapping table on a power meter**, on both rails: plain
  USB (PA unpowered — today's setup) and USB-PD/DC (PA live).
- **Two-node RX/TX**: receive real frames from a second RNS-over-LoRa node
  (same freq/SF/BW/sync 0x42).
- **GPS autobaud** on a fitted GROVE module, at 38400 and at 9600.
- **The SH1107 panel under tinylcd** — the OLED ACKed during `detect_hw` on
  first power-on, but no frame has been drawn through the staged tinylcd
  yet: the 64×128-rotated setup, the page cycling and the program-button
  gestures (GPIO 38) are all untested until this image is flashed.

First-run note for headless boards: **rnsd blocks until the admin password is
set** (`passwd` on the console, or the browser flasher) — a factory-fresh G2
sits with `rnsd.up: 0` and the interfaces retrying `rnsd register failed`
until then. Set the clock too (`date`, NTP or GPS) or announces ride a ~1970
epoch until the bounded time-wait passes.

## What it does, and how it fits

The board contributes the hooks below; the buildable's generated init
dispatcher calls them. There is nothing to call by hand: if the straddle is in
the build, the board comes up automatically.

| Hook | Band | Present when | Brings up |
|---|---|---|---|
| `StationG2Board::onStart` | start | always | LoRa CS park HIGH, program-button internal pullup |
| `StationG2Board::onInit` | init | always | publishes `sys.board` |
| `GpsService::onInit` | init | always | GNSS task (gated by `s.gps.enable`, default **off**) |

`onStart` runs in the `start:` band, **before** `spangapInit()`. It is
bare-hardware bring-up: it parks the SX1262's CS line HIGH so the radio does
not drive MISO before `loraInit()` (in [iface-lora](../iface-lora)) claims the
pin, and arms the program button's internal pullup (the board routes no
external pull, and the pin must never float). There is no power rail to
drive: the OLED/GROVE 3.3 V is always on, and the PA's 7.5 V rail is
USB-PD/DC-input hardware the firmware cannot see or touch.

The LoRa radio engine, the IP/web platform and the mesh stack are owned by
other straddles ([iface-lora](../iface-lora), [spangap-core](../spangap-core),
[spangap-net](../spangap-net), [rns](../rns)); this board only supplies the
SX1262's pins, the FEM declaration and the CS/button glue.

## The power story — read this before trusting any dBm

The TX PA runs from its own **7.5 V rail**, and that rail is only alive when
the input supply can deliver it. The firmware has **no pin into any of this**;
the three front-panel LEDs are the only truth:

| Input | 7.5 V PA rail | TX at the antenna |
|---|---|---|
| USB 2.0 / 3.0 (plain 5 V) | **down** | chip dBm **minus** the PA-path losses — a quiet node, not a 35 dBm one |
| USB-PD (15 V profile) | up | full mapping table below |
| DC barrel, 9–19 V | up | full mapping table below |

| LED | Meaning |
|---|---|
| **LV** (red) | 3.3 V logic rail up |
| **HV** (green) | 7.5 V PA rail up — full TX power available |
| **PA** (blue) | PA amplifying |

A node on plain USB behaves identically in software — same settings, same
logs — and radiates a fraction of the configured power. When a G2 seems deaf
to distant peers *in one direction only*, check the HV LED before anything
else.

## TX power mapping

`s.lora.0.tx_power` is **antenna dBm** — iface-lora converts to chip drive at
the last moment through the declared fixed FEM (gain 20 dB, chip cap 16 dBm;
sibling patch, iface-lora branch `station-g2-fem`). The mapping, against B&Q's
published PA conduction test:

| `tx_power` (antenna dBm) | chip drive (dBm) | measured US915 (dBm) | measured EU868 (dBm) |
|---|---|---|---|
| 35 *(max)* | 15 | 34.5 | 35.0 |
| 34 | 14 | 34.0 | 34.5 |
| 33 | 13 | 33.0 | 34.0 |
| 32 | 12 | 32.0 | 33.0 |
| 31 | 11 | 31.0 | 32.0 |
| 30 | 10 | 30.0 | 31.0 |
| 29 | 9 | 29.0 | 30.0 |
| 28 | 8 | 28.0 | 29.0 |
| 27 | 7 | 27.0 | 28.0 |
| 26 | 6 | 26.0 | 27.0 |

Notes:

- **35 is the ceiling on purpose** (`CONFIG_LORA_TX_POWER_MAX=35`): it is the
  PA's P1dB compression point, not the 36.5–37 dBm saturation figure the
  conduction table tops out at. Chip 15 is the EU-band P1dB and B&Q's
  recommended EU setting; the **chip cap of 16** is the US-band P1dB, and the
  conversion never drives past it even if a build overrides the gain.
- The SX1262's own output setting is **±2 dB** accurate at best, and the
  conduction test is one board on one bench — treat every cell above as
  ±2 dB, and measure your own unit before claiming a number.
- Below-26 settings interpolate through the same fixed gain; the PA is well
  inside its linear region there.

## Regulatory

This hardware can radiate far beyond what most operators may lawfully use:

- **EU868 (ETSI EN 300 220)**: most subbands cap at **14 dBm ERP** — below
  this table's *bottom* row. Only 869.4–869.65 MHz allows **27 dBm ERP at
  ≤10 % duty cycle**. The **SUPE regime channels self-cap at 14 dBm**
  regardless of the ceiling here (the regime's figure is regulatory, see
  iface-lora's `supe.cpp`); the **hailing channel is the operator's setting**
  — the law, not the slider, is the limit, and ERP additionally includes your
  antenna gain.
- **US915 (FCC §15.247)**: 30 dBm conducted is the unlicensed ceiling — the
  top rows of this table already exceed it and are only lawful under an
  appropriate license (e.g. Part 97, with its own identification rules).

Per platform philosophy, `s.lora.0.tx_power` ships with **no default**: the
user must choose, and the radio refuses to start until it is set. On this
board that is not a formality — the honest default for an EU868 G2 is 14,
not 35.

## RX side — hot readings

The 18.5 dB LNA is **always in the receive path** (no bypass pin), so every
RSSI/SNR figure this board reports reads **~18.5 dB hot** compared to a bare
SX1262 node. iface-lora does **not yet compensate** for a fixed RX gain:
expect skewed comparisons against other boards' readings and optimistic
signal wording until it does. Sensitivity itself is excellent — the LNA's low
noise figure is the point of the board.

## GPS (optional GROVE module)

The G2 has **no receiver soldered on** — a GROVE socket takes an optional
module (ESP32-side RX 7 / TX 15, UART1). Because the socket is usually empty,
`s.gps.enable` defaults to **0** on this board; flip it in Settings (System →
GPS) or `gps on` once a module is fitted. On enable the task autobauds
{38400, 9600} and labels the family by the baud that answered (u-blox-class
vs L76K/CASIC-class — a GROVE module offers no host-visible ID). Fixes
publish to `gps.*`; valid GPS time disciplines the system clock and parks ntp
(`sys.time.ext`). The G2 carries **no RTC**, so with no GPS time for an hour
the clock is handed back to ntp.

## OLED (tinylcd)

The 1.3" **SH1107** 128×64 OLED (0x3C/0x3D) sits on the I2C bus (SDA 5 /
SCL 6) that also feeds the GROVE I2C and SparkFun QWIIC sockets. It is driven
by [tinylcd](../tinylcd)'s paged status UI, staged in `straddle.yaml`:
tinylcd ships in this bundle (clean-room — the upstream repo was never
published) and carries the controller select the SH1107 needed
(`CONFIG_TINYLCD_SH1107=y`; portrait-native 64×128, driven through u8g2's
rotated setup — the board just declares the part). The page button is the
program button (GPIO 38): click = next page, double click = the page's own
(the net page toggles WiFi), 500 ms hold = screen off, any press wakes.
Pages on this build: network (SSID / `hostname.local` / IPs), LoRa (state /
peers / RSSI), LXMF unread. **The panel is untested until this image is
flashed** — the SH1107 has only ever ACKed the detect probe on real
hardware.

## Board identity (`detect_hw`)

`esp-idf/src/detect.cpp` answers one question about this board: it returns
`"hw-station-g2"` when the hardware under the firmware is this board, and NULL
when it is not. What it asks:

16 MB flash, then the SH1107 acking on 5/6 (at 0x3C or 0x3D, whichever the
strap picked) — with no rail drive and no reset pulse, since the G2 gates
neither — confirmed by the radio reading as an **SX1262** on the LoRa header.
Passive throughout.

spangap-core calls it before the first `onStart()` — the last moment no bus is
claimed — and **halts the device awake** when the answer disagrees with the
board this image was built for, since every pin map here would then belong to
someone else's hardware. The confirmed answer is published as `sys.hw` and
announced on the console as `build: hw hw-station-g2`. flashmon's standalone
detector carries a hand-kept copy of the same function, renamed
`detect_hw_station_g2`, to identify a chip whose firmware is unknown; change
one, change the other. See
[spangap-core/docs/init.md](../spangap-core/docs/init.md) and
[flashmon/docs/detect.md](../flashmon/docs/detect.md).

## ⚠️ The Station G3 caveat

Meshtastic's `station_common.h` is shared **verbatim** between the G2 and the
G3 — the boards differ **only in PA output power**. A Station G3 therefore
**probes identically**: `detect_hw` will happily say `hw-station-g2` on a G3,
and nothing electrical can tell them apart from the MCU's side. The firmware
will *run*, but the TX-power mapping table above is **G2 calibration** — do
not flash this image on a G3 expecting calibrated antenna power. Per
Meshtastic's `station-g3` variant, the G3 reuses the G2's PA design but sets
its operating mode in **hardware jumpers** (PA-PL1/PL2); at Power Level 1 the
gain is ~12 dB (chip 19 → ~31 out), not this straddle's declared 20, so a G3
running this image **under-radiates** by ~8 dB while reporting G2 figures —
mis-stated telemetry and path-loss math, though no damage risk (our chip cap
16 sits below the 19 both boards tolerate). A G3 wants a sibling straddle
whose only difference is the FEM numbers.

## Hardware & pin map

B&Q **Station G2** — ESP32-S3, 16 MB QIO flash @ 80 MHz, 8 MB **octal** PSRAM
(`CONFIG_SPIRAM_MODE_OCT` — the platform's octal assumption holds here).
Native USB (Espressif USB-Serial-JTAG, VID 0x303A / PID 0x1001). A single
SX1262 sits on **SPI host 2** on its own bus, separate from the flash bus.

### LoRa SX1262 (owned by iface-lora, pins published here)

| Signal | GPIO | | Signal | GPIO |
|---|---|---|---|---|
| NSS / CS | 11 | | RST | 21 |
| SCK | 12 | | BUSY | 47 |
| MOSI | 13 | | DIO1 | 48 |
| MISO | 14 | | | |

The SX1262 drives **DIO2** as the RF switch of the front end
(`CONFIG_LORA0_DIO2_RF_SWITCH=y`) and **DIO3** supplies the 1.8 V TCXO
(`CONFIG_LORA0_TCXO_MV=1800`; 32 MHz ±1.5 ppm KDS DSB211SDN). One radio
(`CONFIG_LORA_COUNT=1`), `CONFIG_LORA0_RADIO_SX1262=y`. The FEM is declared —
not detected — via `CONFIG_LORA0_FEM_FIXED_GAIN_DB=20` /
`CONFIG_LORA0_FEM_MAX_CHIP_DBM=16` / `CONFIG_LORA_TX_POWER_MAX=35` (see the
TX-power section, and the `kconfig:` comments for the full rationale).

### Board-owned pins (in this straddle's `stationg2.h`)

| Signal | GPIO | Notes |
|---|---|---|
| GPS RX (host ← GPS TX) | 7 | GROVE socket, module optional |
| GPS TX (host → GPS RX) | 15 | |
| Program button | 38 | active-low, internal pullup armed by `onStart` (no external pull) |

### OLED + page button (for tinylcd, pins published under its gate)

| Signal | GPIO | Notes |
|---|---|---|
| OLED SDA / SCL | 5 / 6 | 1.3" **SH1107** 128×64 at 0x3C/0x3D — *not* an SSD1306 (`CONFIG_TINYLCD_SH1107=y`); bus shared with GROVE I2C + QWIIC |
| page button | 38 | the program button, read by tinylcd (no reset line — `TINYLCD_RST_PIN` deliberately unset) |

### Memory / flash (published from `kconfig:`)

A non-buildable straddle has no `sdkconfig.defaults` of its own — it would be
ignored under `--with` — so every value that describes this hardware is
published from `straddle.yaml`'s `kconfig:` block and consumed by the owning
straddle / IDF:

| Key | Value | Why |
|---|---|---|
| `CONFIG_ESPTOOLPY_FLASHSIZE_16MB` | `y` | 16 MB flash |
| `CONFIG_SPANGAP_MAX_FIRMWARE_KB` | `6144` | state floor at 6 MB: `app`+`fixed` (~2.8 MB) plus growth headroom sit below it, and the runtime `/state` partition fills the remaining ~10 MB. No SD card on the G2, so /state is all the storage there is — and ~10 MB on a mains-powered node makes this a natural LXMF propagation/mailbox host (`--with reticulous/rlpg`). Without the floor, `app` eats all 16 MB — leaving **no `/state`** |
| `CONFIG_SPIRAM_MODE_OCT` | `y` | 8 MB PSRAM in **octal** mode |

## Storage variables

- `s.gps.enable` (default **0**), `s.gps.interval`, `s.gps.ignore_clock` —
  owned by this straddle's `gps.cpp`; surfaced on the System page.
- `gps.*` ephemerals (model/state/fix/lat/lon/…) — published by the GPS task.
- Runtime LoRa parameters live at `s.lora.*` ([iface-lora](../iface-lora));
  `s.lora.0.tx_power` is antenna dBm and has **no default** (see Regulatory).

## Building

In a spangap workspace (WSL2 or native Linux — the build system does not run
on Windows directly). A workspace is a flat directory of straddle checkouts:
every repo sits as a direct child named by its repo basename, matched by the
`name:` in its `straddle.yaml` — see spangap's own README for `spangap init`
and the CLI install. This straddle and the patched iface-lora go in as flat
siblings; a directory that is already present is never re-cloned.

```
spangap build reticulous/reticulous --with spangap/hw-station-g2
```

**Put this bundle's patched `iface-lora/` (branch `station-g2-fem`) into the
workspace BEFORE the first build.** If it is absent, spangap auto-clones
MAINLINE iface-lora from GitHub, and mainline builds a Station G2 image that
is silently unsafe:

- The two `CONFIG_LORA0_FEM_FIXED_*` lines this board publishes don't exist
  in mainline's Kconfig, so IDF's kconfgen **warns and drops them** — the
  build succeeds, with no antenna-to-chip conversion and no chip cap.
- `s.lora.0.tx_power` **23…35** is then refused by mainline's validity gate
  (`txp > 22`): the radio stays down, `unconfigured`. Annoying but safe —
  except that it funnels the operator downward:
- `s.lora.0.tx_power` **≤ 22** is accepted and applied as RAW CHIP dBm with
  the 35 dBm PA still irremovably in path. The "EU-honest" setting of 14
  actually radiates **~34 dBm** (≈20 dB over the 14 dBm ERP limit) while every
  displayed and announced figure says 14; settings 17–22 additionally drive
  the PA input past its 16 dBm P1dB (22 is 3 dB beyond even Meshtastic's
  absolute-max row) — saturation splatter and possible PA stress.

The patched tree protects itself once in place: its FEM patch lives on the
local branch `station-g2-fem` with no upstream, and spangap's per-build
`git pull` refresh skips checkouts with no upstream tracking ref (as it skips
dirty and detached ones); `--no-pull` / `SPANGAP_NO_PULL=1` is the belt to
those suspenders.

Flash over the native USB port with `spangap flash`, or on a Windows machine
with the `build/flasher.zip` the build emits (an esptool argfile + binaries —
`esptool --chip esp32s3 @flasher_args.txt` style, per the zip's README).

## Dependencies

- [spangap-core](../spangap-core) — base runtime (storage, log, CLI, fs, ITS).
- [iface-lora](../iface-lora) — owns the SX1262 radio engine and the FEM
  conversion; this board parks its CS and supplies its pins/declaration via
  Kconfig. **Branch `station-g2-fem` until upstreamed.**
- [tinylcd](../tinylcd) — the SH1107 paged status UI and the page button
  (staged via `additional_installs`; ships in this bundle, clean-room —
  the upstream repo was never published).

## Read next

- [INTERNALS.md](INTERNALS.md) — the bring-up ordering, the fixed-FEM
  declaration and its exact math, the GPS port notes, the detect traps, and
  the board pitfalls.
