#include <Arduino.h>
#include "ESP32-c3_pinout.h"
#include "main.h"

Network_params network_params;
//GPSData currentGPS;
volatile uint8_t node_counter = 0; 

hw_timer_t * hw_timer = NULL; //Hardware timer fra arduino lib
SemaphoreHandle_t TDMA_mux; //Semafor til at starte TDMA


///De funktioner vi forventer i den her fine fil
void Task_TDMA(void *pvParameters);
void IRAM_ATTR TimerAlarm();
void IRAM_ATTR pps_isr();




/// @brief De 2 queues med valide radioblocke vi kan sende.
QueueHandle_t tx_blockqueue = NULL;
QueueHandle_t rx_blockqueue = NULL;

const uint32_t t_margin = 1000; //Margin for dropping out of rx and tx mode in microseconds
uint32_t t_slot;






void TDMA_setup(uint8_t my_id) {

    //De 2 queues vi stabler blocks i.
    tx_blockqueue = xQueueCreate(TX_queue_size, sizeof(block_item));
    rx_blockqueue = xQueueCreate(TX_queue_size, sizeof(block_item));


    pinMode(GPIO_3, OUTPUT);
    //Til GPS.
    pinMode(PIN_PPS, INPUT_PULLUP);
    attachInterrupt(PIN_PPS, pps_isr, FALLING);

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
    20,                // Priority
    NULL,             // Task handle
    0                 // Core ID (0 or 1)
    );
    
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
    AntennaDir antenna_dir;
    network_params.ready = true; //Sets the global flag to true. so we know we can use
    for (;;) {
        // Vent her. bruger 0% CPU indtil timerISR vækker den
        if (xSemaphoreTake(TDMA_mux, portMAX_DELAY) == pdPASS) { //Tager TDMA semafor
            
            Tx_node = node_counter % network_params.number_of_nodes;

            if (Tx_node == network_params.node_id) {
                uint32_t t_start = micros(); //Husker vores starttidspunkt.

                antenna_dir = get_antenna_dir(Tx_node);
                //TODO set_switches(antenna_dir);

/*                 if (currentGPS.hasUpdate == 1) {
                    Serial.println("GPS er opdateret");
                    Serial.print("Lat: ");
                    Serial.print(currentGPS.latitude, 6);
                    Serial.print(" Long:");
                    Serial.println(currentGPS.longitude, 6);
                    currentGPS.hasUpdate = 0;
                    Serial.print("Antenne dir er: ");
                    Serial.println(antenna_dir);
                } */




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
                uint32_t t_start = micros(); //Husker vores starttidspunkt.

                antenna_dir = get_antenna_dir(Tx_node);
                //TODO set_switches(antenna_dir);
                
                Serial.print("Tx node: ");
                Serial.println(Tx_node);
                Serial.print("Antenne DIR: ");
                Serial.println(antenna_dir);



                //TODO Over i Radio RX
                digitalWrite(PIN_SDA, HIGH);
                while ((micros() - t_start) < (t_slot - t_margin)) //Så længde vi måe slås med radioen.
                {
                    if(radio.available()){
                        block_item buf;
                        radio.read(buf.block_payload, 32);
                        radio.getCRCLength();
                        xQueueSend(rx_blockqueue, &buf, 0); //Sends the block through the queueeueue.
                    }
                }
                digitalWrite(PIN_SDA, LOW);

                //TODO Radio til Rx
                //! Du lugter af ost
                //* Og det bare ret vigtigt
            }
        }
    }
};



