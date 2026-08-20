# hw-station-g2 board HAL — an ordinary spangap component (not a buildable).
#
# Compiles the B&Q Station G2 board code: the LoRa CS park + program-button
# pull + sys.board (stationg2.cpp) and the optional GROVE GNSS receiver
# (gps.cpp). No on-device UI (the SH1107 OLED waits on spangap/tinylcd — see
# straddle.yaml).
#
# app_main is NOT here — it's generated into the reticulous/reticulous buildable
# that adds this board with `--with`. This component just exports the board
# Service classes the generated dispatcher constructs and walks.
include(${CMAKE_CURRENT_LIST_DIR}/spangap_requires.cmake)
idf_component_register(
    SRCS
        "src/stationg2.cpp"
        # This board's one self-assertion (detect_hw), called by
        # spangapInit() before any bus is claimed.
        "src/detect.cpp"
        "src/gps.cpp"
        ${SPANGAP_CONDITIONAL_SRCS}
    INCLUDE_DIRS "include" ${SPANGAP_CONDITIONAL_INCLUDE_DIRS}
    REQUIRES
        ${SPANGAP_REQUIRES}
        driver
        esp_timer        # gps.cpp: esp_timer_get_time drives the fix/publish cadence
        esp_event
        esp_netif
        esp_wifi
        lwip
)
