#pragma once
#include <RF24.h>

void TDMA_setup(uint8_t my_id);
void setup_modem(rf24_pa_dbm_e power_level);
void modem_tx();
void modem_rx();
void setup_testcarrier(rf24_pa_dbm_e level, uint8_t channel);
void re_init_modem(rf24_pa_dbm_e power_level);
void stop_testcarrier(rf24_pa_dbm_e power_level);