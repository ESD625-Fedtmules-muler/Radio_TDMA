
#include <Arduino.h>
#include <ESP32-c3_pinout.h>
#include <main.h>
#include <math.h>


// Tænker at sætte det op med en switch task der kan kaldes, som selv regner hvilken switch der skal sættes afhængig af hvilken Tx node der er.
void Task_headings(void *pvParameter);
float calculate_bearing(float lat1, float lon1, float lat2, float lon2);
AntennaDir get_antenna_dir(uint8_t tx_node_id);


void switch_setup() {

    //TODO setup af switch pins


    xTaskCreatePinnedToCore(
    Task_headings,        // Task function
    "Heading",     // Name
    4096,             // Stack size
    NULL,             // Parameters
    2,                // Priority
    NULL,             // Task handle
    0                 // Core ID (0 or 1)
    );

}


void Task_headings(void *pvParameter){
    for(;;){
        for (int i = 0; i < network_params.number_of_nodes; i++) {
            // Spring os selv over
            if (i == network_params.node_id) continue;

            float target_lat = look_up[i].latitude;
            float target_lon = look_up[i].longitude;

            // Hvis vi ikke har GPS data, sæt til OMNI
            if (target_lat == 0) {
                look_up[i].switchState = DIR_RX_OMNI;
                continue;
            }
            Serial.println("Starter heading udregning: ");
            float brng = calculate_bearing(currentGPS.latitude, currentGPS.longitude, target_lat, target_lon);
            // TODO: Her skal vi lige trække vores egen "heading" fra, hvis vi ikke skal have absolut heading
            Serial.println("Slutter heading her");
            int sector = (int)((brng + 22.5f) / 45.0f) % 8; // Chatten siger at vi deler antennerne op i 8 dele
            look_up[i].switchState = (AntennaDir)(DIR_RX_0 + sector);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
};


//Den her regner altså lige nu kun den absolutte heading! Vi skal have egen retning med for at få det sidste.
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
    if (bearing < 0) {
        bearing += 360.0;
    }
    
    return bearing;
}


AntennaDir get_antenna_dir(uint8_t tx_node) {
    // Hvis Tx er os selv
    if (tx_node == network_params.node_id) {
        return DIR_TX_OMNI;
    }
    else return look_up[tx_node].switchState;
}