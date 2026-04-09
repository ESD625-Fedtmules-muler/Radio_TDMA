#include <Arduino.h>
#include "ESP32-c3_pinout.h"
#include "main.h"

Network_params network_params;

volatile uint8_t node_counter = 0; 

hw_timer_t * hw_timer = NULL; //Hardware timer fra arduino lib
SemaphoreHandle_t TDMA_mux; //Semafor til at starte TDMA

void Task_TDMA(void *pvParameters);
void IRAM_ATTR TimerAlarm();
void IRAM_ATTR pps_isr();

void TDMA_setup(uint8_t my_id) {

    pinMode(GPIO_3, OUTPUT);

    //Til GPS.
    pinMode(PIN_PPS, INPUT_PULLUP);
    attachInterrupt(PIN_PPS, pps_isr, FALLING);

    //Timer setup
    uint32_t TimeSlot = 250000 / network_params.number_of_nodes;
    network_params.node_id = my_id;
    Serial.print("Tids slots er:");
    Serial.print(TimeSlot / 1000);
    Serial.println("ms");
    TDMA_mux = xSemaphoreCreateBinary();
    hw_timer = timerBegin(0, 80, true);  //80Mhz div med 80 = 1Mhz = 1us
    timerAttachInterrupt(hw_timer, &TimerAlarm, true);
    timerAlarmWrite(hw_timer, TimeSlot, true); //used to set counter value of the timer.
    //Freertos tasks.
    xTaskCreate(Task_TDMA, "TDMA_Logic", 4096, NULL, 5, NULL);
};

// Interrupt til når der kommer pps
void IRAM_ATTR pps_isr() {
    //Nulstil timeren så den starter forfra ved hvert PPS
    timerRestart(hw_timer);
    timerAlarmEnable(hw_timer);
    node_counter = 0;

    //Serial.println("GPS PPS");

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
        // Vent her. bruger 0% CPU indtil timerISR vækker den
        if (xSemaphoreTake(TDMA_mux, portMAX_DELAY) == pdPASS) { //Tager TDMA semafor
            //Serial.println("Jeg gør noget nu.");         
            //Serial.println("Funktion til at læse radio data her.");
            //Serial.print("Node_counter: "); Serial.println(node_counter);
            
            Tx_node = node_counter % network_params.number_of_nodes;
 
            if (Tx_node == network_params.node_id) {
                modem_tx();
                digitalWrite(GPIO_3, HIGH);
                //TODO Over i Radio TX
            } else {
                modem_rx();
                digitalWrite(GPIO_3, LOW);
                //TODO Radio til Rx
                //! Du lugter af ost
                //* Og det bare ret vigtigt
            }
        }
    }
};