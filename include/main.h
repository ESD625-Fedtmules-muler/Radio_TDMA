#include<Arduino.h>
#include <RF24.h>


//Vores netværksparametre 
struct Network_params
{
    uint8_t TTL = 6;
    uint8_t channel = 10;
    rf24_datarate_e bitRate = RF24_1MBPS;
    uint8_t number_of_nodes = 10;
    uint8_t node_id = 0;
};
extern Network_params network_params;


extern RF24 radio; //Radio til at skrive og læse Bits eller bytes 


/// @brief Starter alt med TDMA statemachine. Inkl. PPS isr og HW isr. XOXO Axel Olsson
/// @param my_id ID til vores drone eller base. Mellem 0-9!
void TDMA_setup(uint8_t my_id);

void setup_modem(rf24_pa_dbm_e power_level);
void modem_tx();
void modem_rx();