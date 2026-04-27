#include <SPI.h>
#include <Arduino.h>
#include <pins.h>
#include <network_params.h>
#include <TinyGPSPlus.h>
#include <gps.h>
#include <router.h>


HardwareSerial gpsSerial(1); // Use UART1 for GPS communication
TinyGPSPlus gps;

TrackerNode trackerNodes[2];


const char* antennaDirToStr(AntennaDir dir) {
    switch(dir) {
        case ERROR:       return "ERR ";
        case DIR_TX_OMNI: return "TX_O";
        case DIR_RX_OMNI: return "RX  ";
        case DIR_RX_0:    return "RX_0";
        case DIR_RX_1:    return "RX_1";
        case DIR_RX_2:    return "RX_2";
        case DIR_RX_3:    return "RX_3";
        case DIR_RX_4:    return "RX_4";
        case DIR_RX_5:    return "RX_5";
        case DIR_RX_6:    return "RX_6";
        case DIR_RX_7:    return "RX_7";
        default:          return "????";
    }
}
bool GPS_ok = false; //FLag to indicate we have gps lock

void Look_up_entry::debug_msg() {
    Serial.println("--- Look_up_entry ---");
    Serial.print("ID:        "); Serial.println(ID);
    Serial.print("Latitude:  "); Serial.println(latitude, 6);
    Serial.print("Longitude: "); Serial.println(longitude, 6);
    Serial.print("RSSI:      "); Serial.println(rssi);
    Serial.print("HasUpdate: "); Serial.println(hasUpdate ? "true" : "false");
    Serial.print("Lifetime:  "); Serial.println(lifetime);
    Serial.println("---------------------");
}
extern AntennaDir switch_states[MAX_NODES];
extern float P_Signal[MAX_NODES];
extern float P_Channel[MAX_NODES];

struct Channel_state_table{
    Look_up_entry entries[MAX_NODES];
    SemaphoreHandle_t table_mutex = NULL;

    void update_entry(size_t ID, float lat, float lng){
        
        entries[ID].latitude = gps.location.lat();
        entries[ID].longitude = gps.location.lng();
        entries[ID].hasUpdate = true;
        entries[ID].lifetime = 0;
    }

    void begin(){
        table_mutex = xSemaphoreCreateMutex();

        for(int i = 0; i < network_params.number_of_nodes; i++) {
            entries[i].ID = i;
            entries[i].latitude = 0;
            entries[i].longitude = 0;
            entries[i].rssi = -100;
            //look_up[i].switchState = ERROR; // Se med i næste afsnit...
            entries[i].lifetime = -1;
            entries[i].hasUpdate = false;
        }
    }

    /// @brief Runs through the entry table to find the number of known positions
    /// @return Returns the number of known positions including your own.... If you know that one of course O_o 
    uint8_t get_known_positions(){
        uint8_t count = 0;
        for (size_t i = 0; i < MAX_NODES; i++) {
            if (entries[i].lifetime >= 0) count++;
        }
        return count;
    }


    uint16_t serialize(uint8_t *data, size_t len){
        if(!xSemaphoreTake(table_mutex, portMAX_DELAY)){
            return 0;
        }

        size_t offset = 1; // reserver plads til count-byte
        uint8_t packed = 0;

        for (size_t i = 0; i < MAX_NODES; i++){
            if(entries[i].lifetime >= 0){
                if(offset + sizeof(Look_up_entry) > len) break;
                memcpy(data + offset, &entries[i], sizeof(Look_up_entry));
                offset += sizeof(Look_up_entry);
                packed++;
            }
        }

        data[0] = packed; // skriv den reelle count til sidst

        xSemaphoreGive(table_mutex);
        return offset;
    }

    bool evaluate_packet(uint8_t *data, size_t len, uint8_t src = -1){
        if(xSemaphoreTake(table_mutex, portMAX_DELAY) == pdFALSE){
            return false;
        }
        uint16_t num_entries = data[0];

        if(num_entries == 0){
            Serial.printf("Drone no.: %d does not know any positions, including his own", src);
        }
        if(num_entries > network_params.number_of_nodes){
            Serial.printf("KAT DER SKRIGER, fik %d nodes i gps table", num_entries);
            xSemaphoreGive(table_mutex);
            return false;
        }

        size_t offset = 1;
        for (size_t i = 0; i < num_entries; i++)
        {
            Look_up_entry incoming;
            memcpy(&incoming, data + offset, sizeof(Look_up_entry));
            if((entries[incoming.ID].lifetime > incoming.lifetime) || (entries[incoming.ID].lifetime == -1)){
                
                memcpy(&entries[incoming.ID], &incoming, sizeof(incoming));
            }
            offset += sizeof(Look_up_entry);
        }

        xSemaphoreGive(table_mutex);
        return true;
    }

    //Updates the lifetime in seconds
    bool update_life_times(int8_t delta_time){
        if(xSemaphoreTake(table_mutex, portMAX_DELAY) == pdFALSE){
            return false;
        }
        for (size_t i = 0; i < MAX_NODES; i++)
        {
            if(entries[i].lifetime >= 0){
                entries[i].lifetime += delta_time;
            }
            if(entries[i].lifetime > network_params.pos_timeout){
                entries[i].lifetime = -1;
            }
        }
        xSemaphoreGive(table_mutex);
        return true;
    }

    Look_up_entry get_entry(uint8_t node_id) {
        if(xSemaphoreTake(table_mutex, portMAX_DELAY) == pdFALSE){
            Look_up_entry ENTRY;
            Serial.println("Kunne ikke få mutex til entry");
            xSemaphoreGive(table_mutex); //Returns the semaphore after use

            return ENTRY;
        }
        xSemaphoreGive(table_mutex); //Returns the semaphore after use
        return entries[node_id];
    }


    void printLookUpTable() {
        Serial.println("---- LOOK UP TABLE ----");
        Serial.println("ID   | ANT  | P_SIG(dBm) | P_CH(dBm) | LAT        | LON        | AGE | DIST     | HDG");
        Serial.println("-----|------|------------|-----------|------------|------------|-----|----------|-----");
        for (int i = 0; i < network_params.number_of_nodes; i++) {
            char line[150];
            
            char p_sig_str[12], p_ch_str[12];
            if(P_Signal[i] <= -200.0f) snprintf(p_sig_str, sizeof(p_sig_str), "    N/A   ");
            else                        snprintf(p_sig_str, sizeof(p_sig_str), "%7.2f dBm", P_Signal[i]);
            
            if(P_Channel[i] <= -200.0f) snprintf(p_ch_str, sizeof(p_ch_str), "   N/A  ");
            else                         snprintf(p_ch_str, sizeof(p_ch_str), "%6.2f dBm", P_Channel[i]);

            if (entries[i].lifetime >= 0) {
                snprintf(line, sizeof(line), "%2d%s | %s | %s | %s | %10.6f | %10.6f | %3ds | %8.2fm | %.1f°",
                    i,
                    (i == NODE_ID) ? "*" : " ",
                    antennaDirToStr(switch_states[i]),
                    p_sig_str,
                    p_ch_str,
                    entries[i].latitude,
                    entries[i].longitude,
                    entries[i].lifetime,
                    calculate_distance(entries[NODE_ID].latitude, entries[NODE_ID].longitude, entries[i].latitude, entries[i].longitude),
                    calculate_bearing(entries[NODE_ID].latitude, entries[NODE_ID].longitude, entries[i].latitude, entries[i].longitude)
                );
            } else {
                snprintf(line, sizeof(line), "%2d%s | %s | %s | %s | NO DATA",
                    i,
                    (i == NODE_ID) ? "*" : " ",
                    antennaDirToStr(switch_states[i]),
                    p_sig_str,
                    p_ch_str
                );
            }
            Serial.println(line);
        }
        Serial.println("-----------------------");
    }
};





Channel_state_table channel_state_table;



void Task_GPS_rx(void *pvParameter) {
    char c;
    while (network_params.ready != true)
    {
        delay(100);
    }

    QueueHandle_t gps_rx_queue = router_setup_listener(network_params.GPS_IP, 10);
    
    Serial.println("GPS_Rx task running");
    for(;;) {
        
        while (gpsSerial.available() > 0) {
            c = gpsSerial.read();
            //Serial.print(c);
            if (gps.encode(c)) { 
                if (gps.location.isUpdated()) {
                    GPS_ok = true;
                    channel_state_table.update_entry(NODE_ID, gps.location.lat(), gps.location.lng());
                }
            }
        }
        
        network_package block;
        if(xQueueReceive(gps_rx_queue, &block, pdMS_TO_TICKS(1)) == pdTRUE){
            channel_state_table.evaluate_packet(block.payload.data, block.payload.len, block.source_UID); //Parse the bitch
        }        
    }
}



float calculate_distance(float lat1, float lon1, float lat2, float lon2) {
    //Fin formel fra: https://www.movable-type.co.uk/scripts/latlong.html
    const float R = 6371000.0f; // Jordens radius i meter
    const float TO_RAD = M_PI / 180.0f;

    // Omregn til radianer og find delta med det samme
    float phi1 = lat1 * TO_RAD;
    float phi2 = lat2 * TO_RAD;
    float dphi = (lat2 - lat1) * TO_RAD;
    float dlambda = (lon2 - lon1) * TO_RAD;

    // Haversine formlen med float-specifikke funktioner (sinf, cosf, sqrtf)
    // Vi gemmer resultaterne af sinf for at undgå at beregne det samme to gange
    float sdphi = sinf(dphi * 0.5f);
    float sdlambda = sinf(dlambda * 0.5f);

    float a = sdphi * sdphi +
              cosf(phi1) * cosf(phi2) *
              sdlambda * sdlambda;

    // atan2f(sqrtf(a), sqrtf(1.0f - a)) er det samme som asinf(sqrtf(a))
    // Det sparer en sqrtf og en tungere atan2-beregning.
    float c = 2.0f * asinf(sqrtf(a));

    return R * c;
}


float calculate_bearing(float lat1, float lon1, float lat2, float lon2) {
    // Grader til radianer
    float phi1 = lat1 * (M_PI / 180.0);
    float phi2 = lat2 * (M_PI / 180.0);
    float delta_lambda = (lon2 - lon1) * (M_PI / 180.0);

    // Fin formel fra: https://www.movable-type.co.uk/scripts/latlong.html
    float y = sin(delta_lambda) * cos(phi2);
    float x = cos(phi1) * sin(phi2) - sin(phi1) * cos(phi2) * cos(delta_lambda);
    
    float theta = atan2(y, x);

    // Konverter tilbage til grader
    float bearing = theta * (180.0 / M_PI);

    // Normaliser til 0-360 grader (Kan vi lige selv vælge om vi gider)
    /* if (bearing < 0) {
        bearing += 360.0;
    }
     */
    return bearing;
}


void send_azi_to_stepper(float angle, int i2cAddr) {
    Serial.print("Skriver til: ");
    Serial.println(i2cAddr);
    Serial.print("Med denne vinkel: ");
    Serial.println(angle);
    Wire.beginTransmission(i2cAddr);
    Wire.write((byte*)&angle, sizeof(float)); 
    Wire.endTransmission();
}



void update_switch_States(Channel_state_table *table) {

    Look_up_entry own_entry = table->get_entry(NODE_ID); //Starter lige med at hente vores egen pos.

     for (int i = 0; i < MAX_NODES; i++) {
            // Spring os selv over
            if (i == NODE_ID){
                switch_states[i] = DIR_TX_OMNI; 
                continue;
            } 

            Look_up_entry target_entry = table->get_entry(i); //Henter entry for given i.
            // Hvis vi ikke har GPS data, sæt til OMNI
            //? Vi skal nok have lidt flere betingelser der sætter det her. Fx hvis vi ikke kender vores egen pos endnu.
            if (target_entry.lifetime == -1) {
                switch_states[i] = DIR_RX_OMNI;
                continue;
            }

            
            //Tjekker lige om vi er under 10 meter fra target.
            float dist = calculate_distance(own_entry.latitude, own_entry.longitude, target_entry.latitude, target_entry.longitude);
            if (dist <= network_params.min_dist) {
                switch_states[i] = DIR_RX_OMNI;
                continue;
            }
            //Her regner vi retning. Men det er absoulut heading relativ til nord!
            float brng = calculate_bearing(own_entry.latitude, own_entry.longitude, target_entry.latitude, target_entry.longitude);
#ifdef BASE
        // kører kun på basen
        for (int t = 0; t < 2; t++) {
            if (dist >= network_params.min_dist) {   
                if (trackerNodes[t].nodeID == i) {
                    send_azi_to_stepper(-brng, trackerNodes[t].i2cAddress);
                    switch_states[i] = (AntennaDir) (DIR_RX_1 + i); // Sætter switch til antenne 0 + nodeID. 
                    continue;
                }
            }
        }
#else
            //? sector skal kun regnes hvis det er en drone og ikke basen!
            int sector = (int)((brng + 22.5f) / 45.0f) % 8; // Chatten siger at vi deler antennerne op i 8 dele
            //? Det her skal også mappes over til rigtige antenne udgange i stedet! Ligenu kører vi 0 er nord og 4 er syd.
            switch_states[i] = (AntennaDir)(DIR_RX_0 + sector);
#endif            
            }
}




/// @brief Task in charge of once every second updating lifetimes, and updating most adequate switches.
/// @param pvParameter 
void task_GPS_runner(void *pvparameter) {
    uint32_t period = 1000; //tid i millisekunder
    uint32_t periods_between_beacons = 2;
    
    while (!GPS_ok & !network_params.ready)
    {
        delay(50);
    }
    
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    int i = 0;
    
    for (;;) {
        channel_state_table.update_life_times(period/1000);
        update_switch_States(&channel_state_table); // Her får vi lige maskinen til at regne hvilke switches skal sætte for alle timeslots.
        channel_state_table.printLookUpTable();
        i = (i+1) % periods_between_beacons;
        if(i==0){ //Every once in a while blast out some GPS coords...
            uint8_t buf[512] = {0};
            uint16_t len = channel_state_table.serialize(buf, sizeof(buf));
            router_send_data(network_params.GPS_IP, NODE_ID, buf, len, Near_cast); //Sends to direct nabours only
            
        }
        vTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS(period));
    }

}  






void GPS_setup() {

#ifdef BASE
    Wire.begin(PIN_COMPASS_SDA, PIN_COMPASS_SCL); // Start I2C som Master på C3
    Serial.println("Base Mode: I2C Master initialized");

    trackerNodes[0].nodeID = 2;
    trackerNodes[0].i2cAddress = 0x08;
    trackerNodes[1].nodeID = 3;
    trackerNodes[1].i2cAddress = 0x09;
#endif

    byte resetConfig[] = {0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0x19, 0x98};
    gpsSerial.begin(9600, SERIAL_8N1, PIN_GPS_TX, PIN_GPS_RX);
    delay(100);
    gpsSerial.write(resetConfig, sizeof(resetConfig));
    channel_state_table.begin();
    xTaskCreatePinnedToCore(
        Task_GPS_rx,        // Task function
        "GPS_rx",     // Name
        4096,             // Stack size
        NULL,             // Parameters
        5,                // Priority
        NULL,             // Task handle
        1                 // Core ID (0 or 1)
    );
    xTaskCreatePinnedToCore(
        task_GPS_runner,        // Task function
        "GPS_runner",     // Name
        4096,             // Stack size
        NULL,             // Parameters
        5,                // Priority
        NULL,             // Task handle
        1                 // Core ID (0 or 1)
    );
    Serial.println("GPS serial initialized");

};
