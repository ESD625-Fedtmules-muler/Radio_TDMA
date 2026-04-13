#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "ESP32-c3_pinout.h"
#include "main.h"

HardwareSerial gpsSerial(1); // Use UART1 for GPS communication
TinyGPSPlus gps;


void RX_interface(void *pvParameters){

    while (network_params.ready != true)
    {
        delay(100);
    }

    while (true)
    {   
        Package_queue_item block;
        if(xQueueReceive(rx_package_queue, &block, portMAX_DELAY) == pdTRUE){
            Serial.print("GOT SOMETHING:");
            block.debug_msg();
            
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    //Serial.println("Serial initialized");
    //gpsSerial.begin(9600); // RX, TX pins for GPS
    Serial.println("GPS serial initialized");

    TDMA_setup(NODE_ID);
    setup_chopper();
    setup_modem(RF24_PA_MAX);
    
    xTaskCreate(RX_interface, "RX_interface", 4096, NULL, 2, NULL);
    Serial.println("All good");

    while (network_params.ready != true)
    {
        delay(10);
    }
    
    
}






void loop() {
    delay(5000);
    Package_queue_item pkg_to_send;
    size_t len = snprintf((char*)pkg_to_send.data, sizeof(pkg_to_send.data), "Sut min dillerpostej %d, 123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789 %d", millis(), millis()/1000);
    pkg_to_send.length = len;
    Serial.print("sending length: \t");
    Serial.println(len);

    Serial.println("Sending");
    xQueueSend(tx_package_queue, &pkg_to_send, portMAX_DELAY);
}

