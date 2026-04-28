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



bool GPS_ok = false; //FLag to indicate we have gps lock

void Look_up_entry::debug_msg() {
    Serial.println("--- Look_up_entry ---");
    Serial.print("ID:        "); Serial.println(ID);
    Serial.print("Latitude:  "); Serial.println(latitude, 6);
    Serial.print("Longitude: "); Serial.println(longitude, 6);
   // Serial.print("RSSI:      "); Serial.println(rssi);
  //  Serial.print("HasUpdate: "); Serial.println(hasUpdate ? "true" : "false");
    Serial.print("Lifetime:  "); Serial.println(lifetime);
    Serial.println("---------------------");
}










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
#ifndef BASE
    if (bearing < 0) {
        bearing += 360.0;
    }
#endif
    return bearing;
}


void send_azi_to_stepper(float angle, int i2cAddr) {
    /* 
    Serial.print("Skriver til: ");
    Serial.println(i2cAddr);
    Serial.print("Med denne vinkel: ");
    Serial.println(angle);
    */
    Wire.beginTransmission(i2cAddr);
    Wire.write((byte*)&angle, sizeof(float)); 
    Wire.endTransmission();
}



void update_switch_States(Channel_state_table *table) {

    Look_up_entry own_entry = table->get_entry(NODE_ID); //Starter lige med at hente vores egen pos.

     for (int i = 0; i < MAX_NODES; i++) {
            // Spring os selv over
            if (i == NODE_ID){
                table->switch_states[i] = DIR_TX_OMNI; 
                continue;
            } 

            Look_up_entry target_entry = table->get_entry(i); //Henter entry for given i.
            // Hvis vi ikke har GPS data, sæt til OMNI
            //? Vi skal nok have lidt flere betingelser der sætter det her. Fx hvis vi ikke kender vores egen pos endnu.
            if (target_entry.lifetime == -1) {
                table->switch_states[i] = DIR_RX_OMNI;
                continue;
            }

            
            //Tjekker lige om vi er under 10 meter fra target.
            float dist = calculate_distance(own_entry.latitude, own_entry.longitude, target_entry.latitude, target_entry.longitude);
            table->Dist[i] = dist;
            if (dist <= network_params.min_dist) {
                table->switch_states[i] = DIR_RX_OMNI;
                continue;
            }
            //Her regner vi retning. Men det er absoulut heading relativ til nord!
            float brng = calculate_bearing(own_entry.latitude, own_entry.longitude, target_entry.latitude, target_entry.longitude);
            table->Heading[i] = brng;
#ifdef BASE
        // kører kun på basen
        for (int t = 0; t < 2; t++) {
            if (dist >= network_params.min_dist) {   
                if (trackerNodes[t].nodeID == i) {
                    send_azi_to_stepper(-brng, trackerNodes[t].i2cAddress);
                    table->switch_states[i] = (AntennaDir) (DIR_RX_1 + i); // Sætter switch til antenne 0 + nodeID. 
                    continue;
                }
            }
        }
#else
            //? sector skal kun regnes hvis det er en drone og ikke basen!
            int sector = (int)((brng + 22.5f) / 45.0f) % 8; // Chatten siger at vi deler antennerne op i 8 dele
            //? Det her skal også mappes over til rigtige antenne udgange i stedet! Ligenu kører vi 0 er nord og 4 er syd.
            //table->switch_states[i] = (AntennaDir)(DIR_RX_1 + sector);
            table->switch_states[i] = DIR_RX_OMNI;
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
        
        //channel_state_table.printLookUpTable();
        
        
        i = (i+1) % periods_between_beacons;
        if(i==0){ //Every once in a while blast out some GPS coords...
            uint8_t buf[512] = {0};
            uint16_t len = channel_state_table.serialize(buf, sizeof(buf));
            router_send_data(network_params.GPS_IP, NODE_ID, buf, len, Near_cast); //Sends to direct nabours only
            router_send_data(network_params.GPS_IP, NODE_ID, buf, len, Near_cast); //Sends to direct nabours only
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
