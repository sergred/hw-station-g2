/**
 * stationg2.cpp — B&Q Station G2 board support, end to end.
 *
 * Single owner of all board hardware bring-up. See stationg2.h for the API
 * contract and the board reference:
 * https://wiki.bqvoy.com/en/meshtastic/station-g2. Layout:
 *
 *   1. LoRa CS park + program-button pull.
 *      Always compiled. Driven from StationG2Board::onStart() before
 *      spangapInit().
 *   2. Board identity (sys.board).
 *      Driven from StationG2Board::onInit() once storage is up.
 *
 * The SX1262 lives on its own SPI bus (separate from the flash bus) and is
 * powered directly — so, unlike the T-Deck, there is no shared-bus SD probe
 * to race, and unlike the Heltec V4 there is no Vext rail to bring up. The RF
 * front end (35 dBm PA + 18.5 dB LNA + switch) has NO firmware pins at all:
 * its rail is USB-PD/DC-input hardware and its switching hangs off the
 * SX1262's own DIO2, so there is nothing FEM-shaped to drive here either. We
 * still park the radio's CS HIGH before loraInit() owns it, so the deselected
 * radio doesn't drive MISO before its driver claims the pin.
 */
#include "stationg2.h"

#include "storage.h"        /* sys.board (onInit) */

#include "driver/gpio.h"

/* =========================================================================
 * 1. LoRa CS park + program-button pull
 * ========================================================================= */

static void stationg2PowerInit(void)
{
    /* Park the SX1262's CS HIGH (deselected) so it doesn't drive MISO before
     * loraInit() claims the pin. The LoRa radio CS pin comes from iface-lora's
     * Kconfig (CONFIG_LORA0_CS_PIN); defined only when iface-lora is staged. */
#if defined(CONFIG_LORA0_CS_PIN)
    gpio_config_t cs = {};
    cs.pin_bit_mask = 1ULL << CONFIG_LORA0_CS_PIN;
    cs.mode         = GPIO_MODE_OUTPUT;
    cs.pull_up_en   = GPIO_PULLUP_DISABLE;
    cs.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cs.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&cs);
    gpio_set_level((gpio_num_t)CONFIG_LORA0_CS_PIN, 1);
#endif

    /* Program button (GPIO 38): input with the INTERNAL pullup — the G2
     * routes no external pull to this pin (Meshtastic's BUTTON_NEED_PULLUP).
     * Armed here, at start, so the pin never floats — whether or not tinylcd
     * is staged to read it later. gpio_sleep_sel_dis keeps the pull applied
     * through light sleep (an isolated input floats mid-sleep and reads as
     * phantom presses on wake), same exemption the Heltec V4 gives its
     * board-owned pins. */
    gpio_config_t btn = {};
    btn.pin_bit_mask = 1ULL << BOARD_PRG_BUTTON_PIN;
    btn.mode         = GPIO_MODE_INPUT;
    btn.pull_up_en   = GPIO_PULLUP_ENABLE;
    btn.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&btn);
    gpio_sleep_sel_dis((gpio_num_t)BOARD_PRG_BUTTON_PIN);
}

/* =========================================================================
 * Public API — the board bring-up (see stationg2.h).
 * ========================================================================= */

void StationG2Board::onStart() {
    stationg2PowerInit();   /* LoRa CS park + program-button pull */
}

/* onInit — the board says what it is, once storage exists to say it into.
 * Every surface that names the hardware (the Hardware section of Settings, on
 * the browser and — once tinylcd lands — the display) reads this key, so a
 * board is identified in one place rather than by each surface knowing which
 * board it is running on. Needs the platform up, hence onInit not onStart. */
void StationG2Board::onInit() {
    storageSet("sys.board", BOARD_NAME);
}
