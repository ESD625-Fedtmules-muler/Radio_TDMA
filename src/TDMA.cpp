#include <Arduino.h>
#include "pins.h"
#include "main.h"
#include <switches.h>
#include "esp_task_wdt.h"
#include <rssi.h>

Network_params network_params;

volatile uint8_t node_counter = 0; 


extern Channel_state_table channel_state_table;

hw_timer_t * hw_timer = NULL;
SemaphoreHandle_t TDMA_mux;
//AntennaDir switch_states[MAX_NODES]; // Skal være look up for hvordan switch skal stå for hver nodeID. Regnes i GPS.cpp
//float P_Signal[MAX_NODES] = {-200.0};
//float P_Channel[MAX_NODES]= {-200.0};

void Task_TDMA(void *pvParameters);
//void IRAM_ATTR TimerAlarm();
void IRAM_ATTR pps_isr();


QueueHandle_t tx_blockqueue = NULL;
QueueHandle_t rx_blockqueue[MAX_NODES] = {NULL};

const uint32_t t_margin = 1000;
const uint32_t t_RSSI_sampling = 5000;

//TIden af et timeslot i mikrosekunder
uint32_t t_slot; 


void TDMA_setup(uint8_t my_id) {


    pinMode(PIN_GPS_PPS, INPUT_PULLUP);


    t_slot = 250000 / network_params.number_of_nodes;
    network_params.node_id = my_id;
    Serial.print("Tids slots er:");
    Serial.print(t_slot / 1000);
    Serial.println("ms");

    TDMA_mux = xSemaphoreCreateBinary();

    esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(0));


    SPI.begin(PIN_RF_SCK, PIN_RF_MISO, PIN_RF_MOSI);
    SPI.setFrequency(8000000);
    
    setup_modem(RF24_PA_MAX);

    xTaskCreatePinnedToCore(
        Task_TDMA,
        "TDMA_Logic",
        4096,
        NULL,
        10,
        NULL,
        0
    );

    pinMode(PIN_I2C_SCL, OUTPUT);
}

                    
void wait_for_falling(){
    while(digitalRead(PIN_GPS_PPS) == LOW){esp_task_wdt_reset(); };
    while(digitalRead(PIN_GPS_PPS) == HIGH){esp_task_wdt_reset(); };
}



void Task_TDMA(void *pvParameters) {
    esp_task_wdt_delete(NULL);
    char rssi_package_buf[32];
    memset(rssi_package_buf, 0xFF, sizeof(rssi_package_buf));
    uint32_t num_loops = 1000000 / t_slot - 1;
    int node_id = 0;


    Serial.println("Starting TDMA scheduler");


    while (true) {
        wait_for_falling();
        node_id = 0;
        uint32_t pps_start = micros();
        
        for (size_t i = 0; i < num_loops; i++) {
            uint32_t slot_start = pps_start + (i * t_slot);
            uint32_t slot_end = slot_start + t_slot - t_margin;
            while (micros() < slot_start) {}
            
            while (micros() < slot_end - 2*t_RSSI_sampling){}
                digitalWrite(PIN_I2C_SCL, HIGH);
                setup_testcarrier(RF24_PA_MAX, network_params.channel);   
                while (micros() < slot_end - (t_RSSI_sampling + t_RSSI_sampling/10) ){ //Slukker lige for Æ jammer i god tid. 
                };
                stop_testcarrier(RF24_PA_MAX);

                //Vent til slut of round.
                while (micros() < slot_end){ };

        }

        delay(10);
    }
}



/*



*/