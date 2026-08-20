/**
 * gps — GNSS receiver task (B&Q Station G2).
 *
 * Reads NMEA off an OPTIONAL module in the board's GROVE GPS socket
 * (autobauded {38400, 9600} — the baud names the family, u-blox-class vs
 * L76K/CASIC-class), parses every fix it can, and republishes a full snapshot
 * into ephemeral gps.* every s.gps.interval seconds. Gated by s.gps.enable —
 * DEFAULT OFF on this board, because the module is optional (the T-Deck
 * original, with its soldered receiver, defaults on). See gps.cpp for the
 * wire/parse details.
 */
#pragma once
#include "service.h"

/** The GNSS receiver as a boot-registered Service: onInit spawns the GPS task
 *  (gated by s.gps.enable). Declared in hw-station-g2 straddle.yaml `services:`. */
class GpsService : public Service {
public:
    void onInit() override;
};
