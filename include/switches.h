#include <Arduino.h>

#pragma once
void setup_RF_switches();


enum AntennaDir {
    ERROR,
    DIR_TX_OMNI,
    DIR_RX_OMNI,
    DIR_RX_0,
    DIR_RX_1,
    DIR_RX_2,
    DIR_RX_3,
    DIR_RX_4,
    DIR_RX_5,
    DIR_RX_6,
    DIR_RX_7,
};

AntennaDir get_antenna_dir(uint8_t tx_node_id);
void set_switches(AntennaDir antenna_dir);