#include <Arduino.h>
#include "pins.h"
#include "main.h"
#include <switches.h>
#include "esp_task_wdt.h"


Network_params network_params;

volatile uint8_t node_counter = 0; 

hw_timer_t * hw_timer = NULL;
SemaphoreHandle_t TDMA_mux;


void Task_TDMA(void *pvParameters);
//void IRAM_ATTR TimerAlarm();
void IRAM_ATTR pps_isr();


QueueHandle_t tx_blockqueue = NULL;
QueueHandle_t rx_blockqueue[MAX_NODES];

const uint32_t t_margin = 1000;

//TIden af et timeslot i mikrosekunder
uint32_t t_slot; 


void TDMA_setup(uint8_t my_id) {

    tx_blockqueue = xQueueCreate(TX_queue_size, sizeof(block_item));

    for (int i = 0; i < network_params.number_of_nodes; i++)
    {
        rx_blockqueue[i] = xQueueCreate(TX_queue_size, sizeof(block_item));
    }

    pinMode(PIN_GPS_PPS, INPUT_PULLUP);

#ifndef DUMMY_RADIO
    setup_RF_switches();
#endif

    t_slot = 250000 / network_params.number_of_nodes;
    network_params.node_id = my_id;
    Serial.print("Tids slots er:");
    Serial.print(t_slot / 1000);
    Serial.println("ms");

    TDMA_mux = xSemaphoreCreateBinary();

    esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(0));

    xTaskCreatePinnedToCore(
        Task_TDMA,
        "TDMA_Logic",
        4096,
        NULL,
        10,
        NULL,
        0
    );

    network_params.ready = true;
    pinMode(PIN_COMPASS_SCL, OUTPUT);
}


void wait_for_falling(){
    while(digitalRead(PIN_GPS_PPS) == LOW){esp_task_wdt_reset(); };
    while(digitalRead(PIN_GPS_PPS) == HIGH){esp_task_wdt_reset(); };
}



void Task_TDMA(void *pvParameters) {
    esp_task_wdt_delete(NULL);
    
    uint32_t num_loops = 1000000 / t_slot - 1;
    int node_id = 0;

    // Vent på at setup er færdig
    while (network_params.ready != true) { delay(10); }

    while (true) {
        wait_for_falling();
        node_id = 0;
        uint32_t pps_start = micros();
        Serial.println("PING");

        for (size_t i = 0; i < num_loops; i++) {
            uint32_t slot_start = pps_start + (i * t_slot);
            uint32_t slot_end = slot_start + t_slot - t_margin;

            // Vent til slot begynder
            while (micros() < slot_start) {}

            if (node_id == network_params.node_id) {
#ifndef DUMMY_RADIO
                set_switches(DIR_TX_OMNI);
#endif
                modem_tx();
                delay(2);
                while (micros() < slot_end) {
                    block_item buf;
                    if (xQueueReceive(tx_blockqueue, &buf, 0) == pdTRUE) {
                        radio.write(buf.block_payload, 32);
                    }
                }
                modem_rx();
            } else {
#ifndef DUMMY_RADIO
                set_switches(DIR_RX_OMNI);
#endif
                digitalWrite(PIN_COMPASS_SCL, HIGH);
                while (micros() < slot_end) {
                    if (radio.available()) {
                        block_item buf;
                        radio.read(buf.block_payload, 32);
                        buf.ID = node_id;
                        xQueueSend(rx_blockqueue[node_id], &buf, 0);
                    }
                }
                digitalWrite(PIN_COMPASS_SCL, LOW);
            }
            node_id = (node_id + 1) % network_params.number_of_nodes;
        }
        delay(10);
    }
}


