/**
 * detect.cpp — is the hardware under this firmware a B&Q Station G2?
 *
 * The board's one self-assertion: its own name when the OLED and the radio
 * both answer on Station G2 pins, NULL otherwise. See hw-lilygo-tdeck/esp-idf/
 * src/detect.cpp for the contract both callers hold it to, and detect_probe.h
 * for why flashmon's detector carries a hand-kept copy (as
 * `detect_hw_station_g2`) — change this, change flashmon's.
 *
 * Two traps in this pin map:
 *   - GPIO 21 is the RADIO reset here. On the Heltec V4 the same number is
 *     the OLED reset that its detect pulses — do NOT copy that body: there is
 *     no OLED reset line on the G2 and pulsing 21 would bounce the SX1262
 *     instead.
 *   - A Station G3 probes IDENTICALLY. Meshtastic's station_common.h is
 *     shared verbatim between G2 and G3 — the boards differ only in PA output
 *     power — so nothing electrical distinguishes them from here. This
 *     function answering "hw-station-g2" on a G3 is expected and cannot be
 *     fixed by probing; the README documents the PA-calibration consequence.
 *
 * No rail to drive: the G2 has no firmware-gated peripheral rail (the OLED
 * and GROVE sockets sit on the always-on 3.3 V, and the PA's 7.5 V rail is
 * USB-PD/DC hardware with no GPIO into it). Passive throughout — nothing but
 * bus reads.
 */
#include "detect_probe.h"
#include "stationg2.h"

/* OLED — I2C bus (shared with the GROVE I2C / QWIIC sockets), at whichever of
 * the two strapped addresses. */
#define DETECT_OLED_SDA    5
#define DETECT_OLED_SCL    6

/* LoRa header (straddle.yaml's CONFIG_LORA0_*, written out: those symbols only
 * exist when iface-lora is staged, and this must probe without it). */
#define DETECT_LORA_SCK   12
#define DETECT_LORA_MOSI  13
#define DETECT_LORA_MISO  14
#define DETECT_LORA_CS    11
#define DETECT_LORA_RST   21
#define DETECT_LORA_BUSY  47

extern "C" const char* detect_hw(void)
{
    if (!detect_flash_mb(16)) return NULL;

    /* Anchor: the 1.3" SH1107 OLED, which answers at 0x3C or 0x3D depending
     * on one strap. A bare ACK is all it offers — and an ACK is controller-
     * agnostic, so SH1107-vs-SSD1306 doesn't matter to the probe (it matters
     * to tinylcd, later). The panel sits on the always-on 3.3 V and the G2
     * routes no reset line to it, so it is assumed to ACK with no rail drive
     * and no reset pulse — the T3-S3's OLED answers under exactly those
     * conditions, but on THIS board that is a HARDWARE-UNVERIFIED assumption
     * (nothing in this straddle has met a physical G2 yet). If first power-on
     * halts with the radio answering and the OLED not, revisit this step
     * first: an SH1107 that needs settling time would want a short delay and
     * a retry loop here, T-Deck-keyboard style, before anything else is
     * blamed. */
    if (!detect_ack2(DETECT_OLED_SDA, DETECT_OLED_SCL, 0x3C, 0x3D)) {
        detect_dbg("no OLED on 5/6 — not a Station G2");
        return NULL;
    }
    /* Confirm with the radio, and ask WHICH radio: the G2 carries an SX1262.
     * detect_radio_is() is the chip-asserting form (the T3-S3's exact idiom);
     * plain detect_radio()'s last argument is an OUT buffer, not an assertion
     * — it answers for ANY modem it recognizes, which is not the question. */
    if (!detect_radio_is(DETECT_LORA_SCK, DETECT_LORA_MOSI, DETECT_LORA_MISO,
                         DETECT_LORA_CS, DETECT_LORA_RST, DETECT_LORA_BUSY, "sx1262")) {
        detect_dbg("OLED answered but no SX1262 — not a Station G2");
        return NULL;
    }

    detect_found("hw_station_g2");
    return "hw-station-g2";
}
