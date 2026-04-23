#include <Arduino.h>
#include "ESP32-c3_pinout.h"
#include "main.h"
#include <switches.h>

Network_params network_params;

volatile uint8_t node_counter = 0; 

hw_timer_t * hw_timer = NULL; //Hardware timer fra arduino lib
SemaphoreHandle_t TDMA_mux; //Semafor til at starte TDMA


///De funktioner vi forventer i den her fine fil
void Task_TDMA(void *pvParameters);
void IRAM_ATTR TimerAlarm();
void IRAM_ATTR pps_isr();




/// @brief De 2 queues med valide radioblocke vi kan sende.
QueueHandle_t tx_blockqueue = NULL;
QueueHandle_t rx_blockqueue[MAX_NODES];

const uint32_t t_margin = 1000; //Margin for dropping out of rx and tx mode in microseconds
uint32_t t_slot;




void TDMA_setup(uint8_t my_id) {

    //De 2 queues vi stabler blocks i.
    tx_blockqueue = xQueueCreate(TX_queue_size, sizeof(block_item));

    //Array af queues.
    for (int i = 0; i < network_params.number_of_nodes; i++)
    {
        rx_blockqueue[i] = xQueueCreate(TX_queue_size, sizeof(block_item));
    }

    //Til GPS.
    pinMode(PIN_GPS_PPS, INPUT_PULLUP);
    attachInterrupt(PIN_GPS_PPS, pps_isr, FALLING);


#ifndef DUMMY_RADIO
    setup_RF_switches();
#endif


    //Timer setup
    t_slot = 250000 / network_params.number_of_nodes;
    network_params.node_id = my_id;
    Serial.print("Tids slots er:");
    Serial.print(t_slot / 1000);
    Serial.println("ms");
    TDMA_mux = xSemaphoreCreateBinary();
    hw_timer = timerBegin(0, 80, true);  //80Mhz div med 80 = 1Mhz = 1us
    timerAttachInterrupt(hw_timer, &TimerAlarm, true);
    timerAlarmWrite(hw_timer, t_slot, true); //used to set counter value of the timer.
    //Freertos tasks.
    xTaskCreatePinnedToCore(
    Task_TDMA,        // Task function
    "TDMA_Logic",     // Name
    4096,             // Stack size
    NULL,             // Parameters
    5,                // Priority
    NULL,             // Task handle
    0                 // Core ID (0 or 1)
    );
    network_params.ready = true; //Sets the global flag to true. so we know we can use
    //pinMode(PIN_SDA, OUTPUT);
    //pinMode(PIN_SCL, OUTPUT);
};

// Interrupt til når der kommer pps
void IRAM_ATTR pps_isr() {
    //Nulstil timeren så den starter forfra ved hvert PPS
    timerRestart(hw_timer);
    timerAlarmEnable(hw_timer);
    node_counter = 0;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;  
    xSemaphoreGiveFromISR(TDMA_mux, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) { //Skulle gerne sørge for at hoppe ned i "TDMA task" når den ISR er aktiveret.
        portYIELD_FROM_ISR();
    }
}

// Hardware timer. 
void IRAM_ATTR TimerAlarm() {
    if (node_counter >= network_params.number_of_nodes-1) {
        node_counter = 0;
    }else {node_counter ++;}

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;  
    xSemaphoreGiveFromISR(TDMA_mux, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) { //Skulle gerne sørge for at hoppe ned i "TDMA task" når den ISR er aktiveret.
        portYIELD_FROM_ISR();
    }
}







void Task_TDMA(void *pvParameters) {
    static uint8_t Tx_node = 0;
    for (;;) {
        // Vent her indtil timerISR vækker den
        if (xSemaphoreTake(TDMA_mux, portMAX_DELAY) == pdPASS) {
            
            Tx_node = node_counter % network_params.number_of_nodes;
            if (Tx_node == network_params.node_id) {
                uint32_t t_start = micros(); //Husker vores starttidspunkt.
                
#ifndef DUMMY_RADIO
                set_switches(DIR_TX_OMNI);
#endif
                modem_tx();
                while ((micros() - t_start) < (t_slot - t_margin)) //Så længde vi måe slås med radioen. Sikrer os i bund og grund en bagkant.
                {
                    block_item buf;
                    if(xQueueReceive(tx_blockqueue, &buf, 0) == pdTRUE){ //There is something we need to transmit
                        radio.write(buf.block_payload, 32);
                    }
                }
                modem_rx();
            }
            else {
#ifndef DUMMY_RADIO
                set_switches(DIR_RX_7);
#endif
                uint32_t t_start = micros(); //Husker vores starttidspunkt.
                while ((micros() - t_start) < (t_slot - t_margin)) //Så længde vi måe slås med radioen.
                {
                    if(radio.available()){
                        block_item buf;
                        radio.read(buf.block_payload, 32);
                        buf.ID = Tx_node;
                        Serial.println(".");
                        //buf.print_payload();
                        xQueueSend(rx_blockqueue[Tx_node], &buf, 0); //Sends the block through the queueeueue.
                    }
                }
            }
        }
    }
};

