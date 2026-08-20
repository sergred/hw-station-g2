# hw-station-g2 — internals

Maintainer reference for the B&Q Station G2 board HAL. The
[README](README.md) is the operator guide, power story and pin map; this
document is for changing the board code without breaking the bring-up. It is
self-authoritative.

## 1. What this straddle adds

A non-buildable spangap component (`idf_component_register`, no `app_main`).
It exports the board's Service classes for the buildable's generated
dispatcher, and publishes the board's hardware description as `kconfig:`
values. Source layout:

```
esp-idf/
├── CMakeLists.txt          component registration (+ SPANGAP_CONDITIONAL_SRCS glob)
├── include/stationg2.h     board API + GNSS/button pin macros
├── include/gps.h           GpsService declaration
└── src/
    ├── stationg2.cpp       LoRa CS park + program-button pull; sys.board
    ├── gps.cpp             optional GROVE GNSS receiver (T-Deck port, RTC stripped)
    └── detect.cpp          the board's one self-assertion (detect_hw)
```

The subsystems:

- **LoRa CS park + button pull** (`stationg2PowerInit`, from
  `StationG2Board::onStart`) — parks the SX1262's CS HIGH so the radio stays
  deselected until `loraInit()` claims the pin, and arms the program button's
  internal pullup (GPIO 38 has no external pull; `gpio_sleep_sel_dis` holds it
  through light sleep).
- **Board identity** (`StationG2Board::onInit`) — publishes `sys.board`, the
  one place every surface reads the hardware's name from. Needs storage,
  hence init band, not start.
- **GNSS** (`GpsService::onInit` → `gps.cpp` task) — §4.

The board also injects its hardware description as `kconfig:` values in
`straddle.yaml`, consumed by the owning straddle / IDF when staged under
`--with`:

- **Memory** — `CONFIG_ESPTOOLPY_FLASHSIZE_16MB`, `CONFIG_SPIRAM_MODE_OCT`
  (8 MB octal — the platform default assumption holds, unlike the Heltec V4 /
  T3-S3 quads), `CONFIG_SPANGAP_MAX_FIRMWARE_KB=6144`.
- **LoRa SX1262 + fixed FEM** (consumed by [iface-lora](../iface-lora),
  gated `when: reticulous/iface-lora`) — SCK 12 / MOSI 13 / MISO 14, CS 11,
  DIO1 48, BUSY 47, RST 21, TCXO 1800 mV, DIO2 switch, plus the FEM trio (§3).
- **tinylcd pins** (gated `when: spangap/tinylcd`, currently never staged —
  §6) — SDA 5 / SCL 6 / button 38.

There is no `sdkconfig.defaults` here on purpose: a non-buildable straddle's
`sdkconfig.defaults` is ignored under `--with`, so every value that must
survive into the buildable lives in `kconfig:` instead.

## 2. Bring-up ordering

Declared in `straddle.yaml` `services:`, walked in listed order:

```
StationG2Board   onStart (LoRa CS park + button pull)   onInit (sys.board)
GpsService                                              onInit (GNSS task)
```

**`StationG2Board::onStart` runs in the `start:` band, before
`spangapInit()`.** It is bare-hardware bring-up with no platform dependency.
Two jobs:

1. **LoRa CS parked HIGH before `loraInit()`.** The SX1262 shares no bus with
   the flash and there is no SD card, so nothing races — but `loraInit()` (in
   iface-lora) runs later and only then owns `CONFIG_LORA0_CS_PIN`; until then
   the radio's CS floats. Park it HIGH (deselected) at start so the live
   SX1262 cannot drive MISO before its driver claims the pin. Guarded by
   `#if defined(CONFIG_LORA0_CS_PIN)` — defined only when iface-lora is
   staged; a radio-less build simply skips the park.
2. **Program-button pullup.** GPIO 38 has **no external pull** (Meshtastic's
   `BUTTON_NEED_PULLUP`). Armed here so the pin never floats, whether or not
   tinylcd is staged to read it later, and exempted from light-sleep pin
   isolation (`gpio_sleep_sel_dis`) so it does not float mid-sleep and read
   phantom presses on wake — the same exemption the Heltec V4 gives Vext.

There is no rail work at all: the G2 has **no firmware-gated rail**. The
OLED/GROVE 3.3 V is always on, and the PA's 7.5 V rail is created by the
USB-PD/DC input hardware — no GPIO reaches it (that is also why there is no
`BOARD_*_PWR` macro in `stationg2.h`).

`onInit` publishes `sys.board`. It cannot run at start (storage is not up),
and the board is listed first in `services:` so `sys.board` exists before
anything reads it.

## 3. The FEM: declared, not detected

iface-lora's stock FEM support (lora_fem.cpp) is a boot-time *detector*: rail
up, sense the enable net's pull, pick GC1109 vs KCT8103L, drive EN/TXSEL on
every mode change. **None of that can run here** — the G2's front end has no
rail-enable, no chip-enable, no TX-select and no detect pin; it is wired
permanently into the RF path and switched by the SX1262's own DIO2. So the
board *declares* it, through the fixed-FEM support added on iface-lora branch
**`station-g2-fem`** (sibling patch):

```
CONFIG_LORA0_FEM_FIXED_GAIN_DB=20    flat TX gain, antenna = chip + 20
CONFIG_LORA0_FEM_MAX_CHIP_DBM=16     never drive the chip past this
CONFIG_LORA_TX_POWER_MAX=35          slider/runtime ceiling at the antenna
```

The exact math, as `femChipDbm` runs it with a flat gain table:

- The walk finds the smallest chip drive with `chip + 20 >= antennaDbm`,
  i.e. `chip = antennaDbm - 20`, then clamps to the 16 dBm cap.
- `tx_power=35` → chip **15** — the EU-band P1dB and B&Q's recommended EU
  setting (measured 35.0 EU / 34.5 US at the antenna).
- `tx_power=30` → chip 10 (Meshtastic's default drive; ~30/31 measured).
- Cap at work: if a build overrode the gain to, say, 18, a requested 35 would
  want chip 17 — the cap stops it at **16**, the US-band P1dB, so the walk
  can never push the PA past its linear region. Compare Meshtastic's
  `SX126X_MAX_POWER 19`, which is the *absolute-max* conduction row and
  already 3 dB into compression; we deliberately stop at P1dB.
- The ceiling **35 is P1dB, not saturation** (36.5/37 at chip 19). Raising
  `LORA_TX_POWER_MAX` alone buys almost nothing: with the chip cap still 16,
  a ceiling of 37 only moves the drive from 15 to 16 (37−20=17, clamped) —
  +0.5 dB at the antenna. Reaching the 36.5–37 saturation rows would need
  `FEM_MAX_CHIP_DBM` raised to 19 as well, which trades the linear-region
  guarantee for splatter, not range. Don't.

Where the numbers come from: B&Q's PA conduction table (chip → antenna,
US915/EU868), which shows gain ≈ 20–21 dB below compression and band-dependent
by ~0.5 dB; 20 is the EU-mid-table value, so EU output lands exact and US
reads 0.5 dB conservative. Good trade — the table is one bench's board and
the SX1262 itself is only ±2 dB.

**Mainline iface-lora** (without the branch) warns the two `FEM_FIXED`
symbols as unknown and applies no conversion. The failure mode is two-sided,
and the dangerous side is the LOW settings: `tx_power` 23…35 never reaches
the chip at all — mainline's validity gate (`txp > 22`) refuses it and the
radio sits `unconfigured` — while `tx_power` ≤ 22 is accepted as RAW CHIP
dBm, so with the PA rail up the node radiates ~20 dB above every displayed
figure (a "compliant" 14 flies at ~34 dBm) and 17–22 overdrives the PA input
past its 16 dBm P1dB. The refusal above 22 actively funnels an operator into
that regime. Do not run a G2 against mainline; the README's BUILDING section
carries the operator-facing version.

## 4. The GPS port (what differs from the T-Deck original)

`gps.cpp` is hw-lilygo-tdeck's GPS task, ported. Kept intact: the
{38400, 9600} autobaud with the wake edge, the baud→family inference, the
family-specific standby (UBX-RXM-PMREQ vs $PMTK225,4 + `s_needsPowerCycle`),
the u-blox PSMCT rate control, the PM lock across autobaud, the PSRAM task
stack, `cliRegisterCmd("gps", …)`, and the atomic `storageBegin/End` snapshot
publication. Deliberate differences:

- **All PCF8563 RTC code is stripped** — includes, `s_rtcPresent`,
  `rtcBootSync`, the per-fix RTC mirror, the heartbeat's RTC keeper. The G2
  has no RTC and its I2C bus has no 0x51. `gpsHeartbeat()` survives, reduced
  to the ntp-staleness reconcile — the hand-back must still fire on time.
  The `sys.time.valid` / `sys.time.ext` clock-authority contract is
  unchanged.
- **The ntp staleness window is a single constant** (`kNtpStaleUs`, 1 hour):
  the T-Deck's 3-day RTC-backed variant is meaningless without a crystal to
  hold time, so the cautious no-RTC figure is the only one left.
- **`s.gps.enable` defaults 0** (T-Deck: 1). The receiver is an optional
  GROVE module; most sockets are empty, and the autobaud probe costs a
  2×1.5 s listen window per enable. The probe handles absence gracefully
  either way ("not detected").
- **Model strings are families, not parts** — "u-blox-class (38400)" /
  "L76K/CASIC-class (9600)". A GROVE module has no host-visible ID and,
  unlike the T-Deck's two known production fits, could be anything; the label
  states only what the baud actually implies. The 9600 standby still sends
  PMTK — a non-MediaTek part ignores the unknown sentence harmlessly.
- Pins/UART come from `stationg2.h`'s `BOARD_GPS_*` (UART1, RX 7 / TX 15 —
  board-private, so they live in the header, not in Kconfig).

The settings rows (System → GPS) are declared in `straddle.yaml`; the
defaults stay owned by `gps.cpp` (`storageDefault` under `s.gps.version`), so
the yaml rows are deliberately defaultless.

## 5. detect.cpp — the trap list

- **GPIO 21 is the RADIO reset here.** On the Heltec V4 the same GPIO number
  is the OLED reset that its detect pulses. Do not copy that body onto this
  board: there is no OLED reset line on the G2, and pulsing 21 bounces the
  SX1262 instead.
- **A Station G3 probes identically.** `station_common.h` is shared verbatim
  between G2 and G3 — the boards differ only in PA output power, which no bus
  read can see. `detect_hw` saying `hw-station-g2` on a G3 is expected and
  unresolvable by probing; the README documents the calibration consequence.
  A G3 straddle would be this one with different FEM numbers.
- **The OLED-ACK-without-reset assumption is hardware-unverified.** The
  anchor (`detect_ack2(5, 6, 0x3C, 0x3D)`) assumes the SH1107 ACKs with no
  rail drive (there is none to drive) and no reset pulse (there is no reset
  line) — true of the T3-S3's panel, untested on a physical G2. If first
  power-on halts with the radio answering and the OLED not, add a short
  settle-and-retry loop (T-Deck-keyboard style) before blaming anything else.
- **The radio names the chip**: `detect_radio_is(…, "sx1262")` — the
  chip-ASSERTING form the T3-S3 uses, so a lookalike with a different modem
  answers NULL. Do not "simplify" to plain `detect_radio(…)`: its seventh
  argument is an OUT buffer (it answers for any modem it recognizes and
  strcpy's the slug into it), not an assertion — the flashmon copy must carry
  the `_is` form too.
- **flashmon's copy is hand-kept.** flashmon's standalone detector carries
  this function renamed `detect_hw_station_g2`; the copy is manual and
  deliberately so (see detect_probe.h). Change this file, change
  flashmon/esp-idf/main/detect.c.
- The `DETECT_*` literals restate `straddle.yaml`'s pins on purpose: the
  `CONFIG_LORA0_*` symbols only exist when iface-lora is staged, and the
  probe must work in a radio-less image.

## 6. Why tinylcd is commented out

Two independent blockers, both stated at the commented-out
`additional_installs` in `straddle.yaml`:

1. **github.com/spangap/tinylcd is not published.** `additional_installs`
   entries are cloned at build time; a live entry fails every build of this
   board until the repo exists.
2. **The SH1107 is not an SSD1306.** tinylcd's controller support today is
   SSD1306-default; the SH1107 differs in controller and addressing (page
   layout, 128×128-native RAM window on a 128×64 panel), so it needs a
   controller-select knob tinylcd-side before pin values mean anything.

The `when: spangap/tinylcd` kconfig group (SDA 5 / SCL 6 / BUTTON 38) is
already committed and inert — gated groups apply only when the straddle is
staged, so it costs nothing now and works the day both blockers fall. Note
the group carries no `TINYLCD_RST_PIN` (the G2 has no OLED reset line — the
T3-S3 shows omitting it is fine) and that the board's `onStart` arms the
button pull regardless, in case tinylcd doesn't.

## 7. Pitfalls

- **`CONFIG_SPANGAP_MAX_FIRMWARE_KB` relocates `/state`.** Changing it moves
  the offset where the runtime read-write partition begins → existing devices
  factory-reset on the next boot. Warn users before bumping it. (6 MB floor,
  ~10 MB `/state`, no SD card — `/state` is all the storage this node has,
  which is also what makes it a good `rlpg` mailbox host.)
- **Mainline iface-lora = no FEM model.** Until branch `station-g2-fem` is
  upstreamed, mainline warns the `FEM_FIXED` symbols as unknown and performs
  no antenna↔chip conversion (§3). The build succeeds — and settings ≤ 22
  transmit ~20 dB hotter than displayed while 23…35 refuse to start. Place
  the patched sibling in the workspace before the first build.
- **The firmware cannot see the PA rail.** Plain-USB nodes radiate at
  chip-minus-losses with identical settings and logs; the HV/PA LEDs are the
  only truth. Any future "TX power applied" logging must not imply the
  antenna figure was achieved.
- **RSSI/SNR read ~18.5 dB hot.** The RX LNA is always in path and iface-lora
  does not yet subtract a fixed RX gain. Don't tune links by comparing this
  board's readings against a bare-chip node's.
- **Nothing is hardware-verified.** Every pin, the OLED-ACK assumption, the
  TCXO setting, the DIO2 switching and the whole power table come from
  Meshtastic variant files and B&Q's wiki, not from a board in hand. The
  README's verification checklist is the gate; keep its warning in place
  until each line has been ticked on a physical G2.
