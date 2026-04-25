#include <SPI.h>
#include <Arduino.h>
#include <pins.h>
#include <network_params.h>
#include <TinyGPSPlus.h>
#include <gps.h>
#include <router.h>


HardwareSerial gpsSerial(1); // Use UART1 for GPS communication
TinyGPSPlus gps;


bool GPS_ok = false; //FLag to indicate we have gps lock
AntennaDir switch_states[MAX_NODES];


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
        size_t offset = 0;
        uint8_t num_entries = get_known_positions();
        data[0] = num_entries;
        offset++;
        for (size_t i = 0; i < MAX_NODES; i++)
        {
            if(entries[i].lifetime >= 0){
                if(offset + sizeof(Look_up_entry) > len){
                    return offset; 
                }
                memcpy(data + offset, &entries[i], sizeof(Look_up_entry));
                offset += sizeof(Look_up_entry);
            }
        }
        xSemaphoreGive(table_mutex);
        return offset;
    };

    bool evaluate_packet(uint8_t *data, size_t len, uint8_t src = -1){
        if(xSemaphoreTake(table_mutex, portMAX_DELAY) == pdFALSE){
            return false;
        }

        uint16_t num_entries = data[0];
        if(num_entries == 0){
            Serial.printf("Drone no.: %d does not know any positions, including his own", src);
        }

        size_t offset = 1;
        for (size_t i = 0; i < num_entries; i++)
        {
            Look_up_entry incoming;
            memcpy(&incoming, data + offset, sizeof(Look_up_entry));
            if((entries[incoming.ID].lifetime > incoming.lifetime) || (entries[incoming.ID].lifetime == -1)){
                memcpy(&entries[incoming.ID], &incoming, sizeof(incoming));
            }
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
            return ENTRY;
        }
        
        return entries[node_id];
    }


    void printLookUpTable() {
        Serial.println("---- LOOK UP TABLE ----");
        for (int i = 0; i < network_params.number_of_nodes; i++) {
            Serial.print("Node ");
            Serial.print(i);
            if (i == NODE_ID) {
            Serial.print("(ME)");
            }
            Serial.print(": ");

            if (entries[i].lifetime >= 0) {
                Serial.print("Lat: ");
                Serial.print(entries[i].latitude, 6);
                Serial.print(", Lon: ");
                Serial.print(entries[i].longitude, 6);
                Serial.print(", Time sice: ");
                Serial.print(entries[i].lifetime);
                Serial.println("s");
                Serial.print("Dist to node: ");
                Serial.println(calculate_distance(entries[NODE_ID].latitude, entries[NODE_ID].longitude, entries[i].latitude, entries[i].longitude));
                Serial.print("Heading to node: ");
                Serial.println(calculate_bearing(entries[NODE_ID].latitude, entries[NODE_ID].longitude, entries[i].latitude, entries[i].longitude));
            } else {
                Serial.println("No data");
            }
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

void update_switch_States(Channel_state_table *table) {

    Look_up_entry own_entry = table->get_entry(NODE_ID); //Starter lige med at hente vores egen pos.

     for (int i = 0; i < MAX_NODES; i++) {
            // Spring os selv over
            if (i == NODE_ID) continue;

            Look_up_entry target_entry = table->get_entry(i); //Henter entry for given i.
            
            // Hvis vi ikke har GPS data, sæt til OMNI
            //! Vi skal nok have lidt flere betingelser der sætter det her. Fx hvis vi ikke kender vores egen pos endnu.
            if (target_entry.lifetime == -1) {
                switch_states[i] = DIR_RX_OMNI;
                continue;
            }

            //Tjekker lige om vi er under 10 meter fra target.
            float dist = calculate_distance(own_entry.latitude, own_entry.longitude, target_entry.latitude, target_entry.longitude);
            if (dist <= 10) {
                switch_states[i] = DIR_RX_OMNI;
                continue;
            }
            //Her regner vi retning. Men det er absoulut heading relativ til nord!
            float brng = calculate_bearing(own_entry.latitude, own_entry.longitude, target_entry.latitude, target_entry.longitude);
            
            //! sector skal kun regnes hvis det er en drone og ikke basen!
            int sector = (int)((brng + 22.5f) / 45.0f) % 8; // Chatten siger at vi deler antennerne op i 8 dele
            //! Det her skal også mappes over til rigtige antenne udgange i stedet! Ligenu kører vi 0 er nord og 4 er syd.
            switch_states[i] = (AntennaDir)(DIR_RX_0 + sector);
            }
}




/// @brief Task in charge of once every second updating lifetimes, and updating most adequate switches.
/// @param pvParameter 
void task_GPS_runner(void *pvparameter) {
    uint32_t period = 1000; //tid i millisekunder
    uint32_t periods_between_beacons = 3;
    
    while (!GPS_ok)
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
            router_send_data(network_params.GPS_IP, NODE_ID, buf, len);
            
        }
        vTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS(period));
    }

}  






void GPS_setup() {
    // Det reset skal sendes til nogle af dem, for at sikre at GPS'en sender mere end bare GPGLL (For at tinyGPS virker)
    byte resetConfig[] = {0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0x19, 0x98};
    gpsSerial.begin(9600, SERIAL_8N1, PIN_GPS_TX, PIN_GPS_RX);
    delay(100);
    gpsSerial.write(resetConfig, sizeof(resetConfig));

    Serial.println("GPS serial initialized");

    //Look-up laves her.
    //tableMutex = xSemaphoreCreateMutex();    
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
};
