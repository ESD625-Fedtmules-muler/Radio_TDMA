#include <main.h>
#include <ESP32-c3_pinout.h>
#include <Arduino.h>
#include <switches.h>

#ifndef DUMMY_RADIO 
void setup_RF_switches(){
    pinMode(PIN_SR_CLK, OUTPUT);
    pinMode(PIN_SR_DAT, OUTPUT);
    pinMode(PIN_SR_STR, OUTPUT);

    digitalWrite(PIN_SR_STR, LOW);
    shiftOut(PIN_SR_DAT, PIN_SR_CLK, MSBFIRST, 0x00); //Sætter alt til 0
    digitalWrite(PIN_SR_STR, HIGH);
}

void set_switches(AntennaDir antenna_dir) {
    static const byte antennaValues[] = {
    B00000110,    //ERROR,
    B00001001,    //DIR_TX_OMNI
    B00000101,    //DIR_RX_OMNI
    B00000010,    //DIR_RX_0
    B01000010,    //DIR_RX_1
    B00100010,    //DIR_RX_2
    B01100010,    //DIR_RX_3
    B00010010,    //DIR_RX_4
    B01010010,    //DIR_RX_5
    B00110010,    //DIR_RX_6
    B01110010    //DIR_RX_7
    };

    byte value = antennaValues[antenna_dir];
    digitalWrite(PIN_SR_STR, LOW);
    shiftOut(PIN_SR_DAT, PIN_SR_CLK, MSBFIRST, value);
    digitalWrite(PIN_SR_STR, HIGH); 
}

#endif