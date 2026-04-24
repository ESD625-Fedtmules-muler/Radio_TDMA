#include <SPI.h>
#include <Arduino.h>
#include <pins.h>
#include <network_params.h>
#include <TinyGPSPlus.h>
#include <gps.h>
#include <router.h>


HardwareSerial gpsSerial(1); // Use UART1 for GPS communication
TinyGPSPlus gps;





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
            if(entries[i].lifetime >== 0){
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

    bool evaluate_packet(uint8_t *data, size_t len){
        Serial.println("Waiting for semaphore");
        if(xSemaphoreTake(table_mutex, portMAX_DELAY) == pdFALSE){
            return false;
        }

        uint16_t num_entries = data[0];
        Serial.println("The number of coords");
        Serial.println(num_entries);
        size_t offset = 1;
        for (size_t i = 0; i < num_entries; i++)
        {
            Look_up_entry incoming;
            Serial.println(incoming.latitude);
            memcpy(&incoming, data + offset, sizeof(Look_up_entry));
            Serial.printf("Valid timeslots %f", incoming.latitude);
            if(entries[incoming.ID].lifetime > incoming.lifetime || entries[incoming.ID].lifetime == -1){
                memcpy(&entries[incoming.ID], &incoming, sizeof(incoming));
            }
        }

        xSemaphoreGive(table_mutex);
        return true;
    }

    //Updates the lifetime in seconds
    void update_life_times(int8_t delta_time){
        for (size_t i = 0; i < MAX_NODES; i++)
        {
            if(entries[i].lifetime > 0){
                entries[i].lifetime += delta_time;
            }
            if(entries[i].lifetime > network_params.pos_timeout){
                entries[i].lifetime = -1;
            }
        }
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

            if (entries[i].latitude > 0) {
                Serial.print("Lat: ");
                Serial.print(entries[i].latitude, 6);
                Serial.print(", Lon: ");
                Serial.print(entries[i].longitude, 6);
                Serial.print(", Time sice: ");
                Serial.print(entries[i].lifetime);
                Serial.println("s");
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
                    channel_state_table.update_entry(NODE_ID, gps.location.lat(), gps.location.lng());
                }
            }
        }
        
        network_package block;
        if(xQueueReceive(gps_rx_queue, &block, pdMS_TO_TICKS(1)) == pdTRUE){
            Serial.println("HEY MAN I GOT A PACKAET");
            channel_state_table.evaluate_packet(block.payload.data, block.payload.len); //Parse the bitch
        
        }        


    }
}







/// @brief Task in charge of once every second updating lifetimes, and updating most adequate switches.
/// @param pvParameter 
void task_GPS_runner(void *pvparameter) {
    uint32_t period = 1000; //tid i millisekunder
    uint32_t periods_between_beacons = 10;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    int i = 0;
    for (;;) {
        channel_state_table.update_life_times(period/1000);
        i = (i+1) % periods_between_beacons;
        if(i==0){ //Every once in a while blast out some GPS coords...
            uint8_t buf[512] = {0};
            uint16_t len = channel_state_table.serialize(buf, sizeof(buf));

            channel_state_table.printLookUpTable();
            router_send_data(network_params.GPS_IP, NODE_ID, buf, len);
            Serial.println("Sending GPS pos");
        }
        vTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS(period));
    }

}  







void GPS_setup() {
    // Det reset skal sendes til nogle af dem, for at sikre at GPS'en sender mere end bare GPGLL (For at tinyGPS virker)
    byte resetConfig[] = {0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0x19, 0x98};
    gpsSerial.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
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
        0                 // Core ID (0 or 1)
    );

    xTaskCreatePinnedToCore(
        task_GPS_runner,        // Task function
        "GPS_runner",     // Name
        4096,             // Stack size
        NULL,             // Parameters
        5,                // Priority
        NULL,             // Task handle
        0                 // Core ID (0 or 1)
    );
};
