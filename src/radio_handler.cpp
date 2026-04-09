#include <arduino.h>
#include <RF24.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <SPI.h>
#include <ESP32-c3_pinout.h>
#include <main.h>

RF24 radio(PIN_CE, PIN_CSN);



bool Assert_setting(bool result, const char* msg){
  Serial.print(msg);
  if(result){
    Serial.println(": OK"); 
  }else{
    Serial.println(": Error");
  }
  return result;
}


void setup_modem(rf24_pa_dbm_e power_level){
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
  SPI.setFrequency(8000000);
  
  Assert_setting(radio.isChipConnected(), "Chip connected");
  Assert_setting(radio.begin(&SPI), "Setting up modem");
  
  radio.disableAckPayload(); //SLår acks fra
  radio.setDataRate(network_params.bitRate); //1 MBITS / S 
  radio.setAutoAck(false); //SLår auto acks fra  i modtager
  radio.setPALevel(power_level, true); //max transmit power 
  radio.setChannel(network_params.channel); //Vi bruger channel 10
  radio.openWritingPipe(0x042);
  radio.openReadingPipe(0, 0x042);

}


void modem_tx(){
  radio.stopListening(); //Fortæller svinet den skal gå i tx
  //TODO Sætte GPIO rf switches
    
}



void modem_rx(){
  radio.startListening();
  //TODO Sætte GPIO rf switches
}