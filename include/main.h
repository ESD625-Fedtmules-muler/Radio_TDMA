#include<Arduino.h>
#include <RF24.h>


//Vores netværksparametre 
struct Network_params
{
    uint8_t TTL = 6;
    uint8_t channel = 90; //Ca. i MHz, så 90 => 2.490 GHz
    rf24_datarate_e bitRate = RF24_1MBPS;
    uint8_t number_of_nodes = 10;
    uint8_t node_id = 0;
    uint64_t pipe_name = 0x0000000042; //40 bit number to say what pipe address should be used on the radio.
    bool ready = false;
};
extern Network_params network_params;

struct GPSData 
{
    float latitude;
    float longitude;
    String rawSentence;
    bool hasUpdate;
};
extern GPSData currentGPS;


extern RF24 radio; //Radio til at skrive og læse Bits eller bytes 


/// @brief Starter alt med TDMA statemachine. Inkl. PPS isr og HW isr. XOXO Axel Olsson
/// @param my_id ID til vores drone eller base. Mellem 0-9!
void TDMA_setup(uint8_t my_id);

void setup_modem(rf24_pa_dbm_e power_level);
void modem_tx();
void modem_rx();

void GPS_setup();

//Write and read queue for le modem, kan først bruges efter network_params.ready == true
#define RX_queue_size 100
#define TX_queue_size 100
extern QueueHandle_t tx_blockqueue;
extern QueueHandle_t rx_blockqueue;



//Alt i de her fucking radioer foregår i blocks, så vi skal have styr på vores blocks
struct block_item{
    uint8_t block_payload[32] = {0};
    bool crc = false;
    void print_payload();
};