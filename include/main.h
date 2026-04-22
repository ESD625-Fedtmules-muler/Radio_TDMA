#include<Arduino.h>
#include <RF24.h>

#define MAX_NODES 10


//til GPS.cpp
void GPS_setup();
struct GPSData 
{
    float latitude;
    float longitude;
    bool hasUpdate;
};
extern GPSData myGPS;

#define GPS_PAKKE_SIZE 256
extern uint8_t GPS_buffer[GPS_PAKKE_SIZE];
extern size_t GPS_pakke_length;
extern volatile bool GPS_pakkeReady;
struct GPS_pakker {
    uint8_t type;
    uint8_t node_id;
    float latitude;
    float longitude;
    float RSSI; //! OBS. RSSI type
};
extern GPS_pakker gps_pakke;

void decodeAndPrintGPSBuffer(const uint8_t* buffer, size_t length);






//Vores netværksparametre 
struct Network_params
{
    uint8_t TTL = 6;
    uint8_t channel = 90; //Ca. i MHz, så 90 => 2.490 GHz
    rf24_datarate_e bitRate = RF24_1MBPS;
    uint8_t number_of_nodes = MAX_NODES;
    uint8_t node_id = 0;
    uint64_t pipe_name = 0x0000000042; //40 bit number to say what pipe address should be used on the radio.
    bool ready = false;
};
extern Network_params network_params;


extern RF24 radio; //Radio til at skrive og læse Bits eller bytes 


/// @brief Starter alt med TDMA statemachine. Inkl. PPS isr og HW isr. XOXO Axel Olsson
/// @param my_id ID til vores drone eller base. Mellem 0-9!
void TDMA_setup(uint8_t my_id);

void setup_modem(rf24_pa_dbm_e power_level);
void modem_tx();
void modem_rx();



//Write and read queue for le modem, kan først bruges efter network_params.ready == true
#define RX_queue_size 100
#define TX_queue_size 100




//Alt i de her fucking radioer foregår i blocks, så vi skal have styr på vores blocks
struct block_item{
    uint8_t block_payload[32] = {0};
    int8_t ID = 0;
    void print_payload();
};


//* Vores chopper af pakker Koster 
void setup_chopper();
extern QueueHandle_t rx_package_queue;
extern QueueHandle_t tx_package_queue;



// De variable pakker vi bruger
struct Package_queue_item
{
    uint8_t data[1024] = {0};
    uint16_t length = 0;
    void debug_msg();
};


extern QueueHandle_t tx_blockqueue;





enum MSG_type_t{
    Near_cast = 0,
    Broadcast = 1,
    P2P = 2,
};



typedef struct {
    uint16_t len;
    uint8_t data[512];
} payload_buffer_t;




//* ALT til routeren

/// @brief Repræsenterer en netværkspakke i vores mesh protokol.
/// Brug parse_header() + parse_payload() til at deserialisere fra et Package_queue_item,
/// eller serialize() til at gøre den klar til afsendelse.
class network_package {
public: //illegal
    uint16_t source_UID;                ///< UID på afsender noden
    uint8_t  nonce;                     ///< Unikt pakke-ID til duplikat detektion
    uint16_t dest_UID;                  ///< UID på modtager noden
    MSG_type_t msg_type = P2P;          ///< P2P, broadcast eller nearcast
    uint8_t  hops = 0;                  ///< Antal hops pakken har rejst indtil nu
    uint8_t  hop_list[16] = {0};        ///< Liste over noder pakken har passeret igennem
    payload_buffer_t payload;           ///< Den egentlige payload data

    /// @brief Parser kun headeren fra et Package_queue_item.
    /// Kan bruges til tidlig drop af pakker der ikke er til os, uden at parse payload.
    /// @param original_buffer Buffer der skal parses fra
    /// @return true hvis headeren er valid og blev parset korrekt
    bool parse_header(Package_queue_item* original_buffer);

    /// @brief Parser payload fra et Package_queue_item — kald parse_header() først.
    /// @param original_buffer Samme buffer som blev brugt til parse_header()
    /// @return true hvis payload er valid og blev parset korrekt
    bool parse_payload(Package_queue_item* original_buffer);

    /// @brief Serialiserer pakken ind i et Package_queue_item klar til afsendelse.
    /// @param dest Destination buffer
    /// @return true hvis serialiseringen lykkedes
    bool serialize(Package_queue_item* dest);

    /// @brief Tilføjer en hop til hop_list og incrementerer hops tælleren.
    /// @param uid UID på noden der skal tilføjes
    /// @return true hvis der var plads i hop_list
    bool add_hop(uint8_t uid);

    /// @brief Printer hele pakken som hex dump til Serial — kun til debugging.
    void debug_msg();
};

/// @brief Starter tasken der kører vores router samt, sætter listener queues op.
void setup_router();


void router_send_data(uint16_t dest_UID, uint16_t src_UID, uint8_t* buffer, size_t length);


/// @brief Fjerner den listener og dræber køen
/// @param UID det unikke userID der lavede køen
/// @return true hvis det gik godt
bool router_remove_listener(uint16_t UID);


/// @brief Sets up a listener for the UID
/// @param UID A unique user id expected to be transmitted to
/// @param queuesize How large the queue should be
/// @return xQueueCreate(queuesize, sizeof(network_package));
QueueHandle_t router_setup_listener(uint16_t UID, uint16_t queuesize);