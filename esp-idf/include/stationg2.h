/**
 * stationg2.h — B&Q Station G2 board support for reticulous.
 *
 * The Station G2 is a high-power base-station node (discontinued but fielded):
 * an ESP32-S3 (16 MB flash, 8 MB *octal* PSRAM, native USB) carrying one
 * Semtech SX1262 on its own SPI bus, behind an always-in-path RF front end
 * (35 dBm-P1dB TX PA + 18.5 dB RX LNA + a switch driven by the SX1262's own
 * DIO2). A 1.3" SH1107 OLED and GROVE/QWIIC sockets share the I2C bus; a
 * GROVE socket takes an optional GNSS module (gps.cpp). No battery, no SD
 * card. See stationg2.cpp for the implementation and the board reference:
 * https://wiki.bqvoy.com/en/meshtastic/station-g2
 *
 * What this module provides:
 *   - Compile-time constants for the board's own pins (GNSS UART, program
 *     button).
 *   - The always-on board bring-up entry point StationG2Board::onStart(): the
 *     LoRa CS park and the program-button pull.
 *
 * There is NO board-owned power-rail pin here (unlike the Heltec V4's Vext):
 * the PA's 7.5 V rail is USB-PD/DC-input hardware with no GPIO into it, and
 * the OLED/GROVE 3.3 V is always on. The SX1262's pins (NSS/SCK/MOSI/MISO/
 * RST/BUSY/DIO1, TCXO, DIO2 RF switch) and the FEM declaration are NOT wired
 * here: they belong to iface-lora's CONFIG_LORA* knobs, set as board VALUES
 * in this straddle's straddle.yaml `kconfig:` block. The GNSS pins ARE here —
 * they are board-private (only gps.cpp consumes them), so no straddle defines
 * a Kconfig symbol for them.
 */
#pragma once

#include "sdkconfig.h"
#include "service.h"

#define BOARD_NAME              "B&Q Station G2"

/* GNSS receiver — NOT soldered on: a GROVE socket takes an optional module
 * (NMEA over UART 8N1; gps.cpp autobauds {38400, 9600} and labels the family
 * by the baud). Bare ints, no driver/uart.h dependency here — gps.cpp types
 * the port. Host RX <- GPS TX = 7; host TX -> GPS RX = 15. */
#define BOARD_GPS_UART_NUM      1
#define BOARD_GPS_RX_PIN        7
#define BOARD_GPS_TX_PIN        15

/* Program button, active-low on GPIO 38. The board routes NO external pull to
 * it (Meshtastic's BUTTON_NEED_PULLUP), so onStart arms the internal pullup —
 * the pin must never float, whether or not tinylcd is staged to read it. The
 * BOOT and RESET buttons are strap/EN hardware, not firmware inputs. */
#define BOARD_PRG_BUTTON_PIN    38

/**
 * Board bring-up, as a registered Service. StationG2Board::onStart is the
 * always-on hardware bring-up: it parks the LoRa radio's CS line HIGH so the
 * SX1262 doesn't drive MISO before the LoRa interface claims it, and arms the
 * program button's internal pullup. It runs in the start band, before
 * spangapInit(). onInit publishes sys.board — that needs storage, hence the
 * init band. The OLED UI, when tinylcd lands, is tinylcd's own service, not a
 * board hook.
 */
class StationG2Board : public Service {
public:
    void onStart() override;
    void onInit()  override;   /* publishes sys.board once storage is up */
};
