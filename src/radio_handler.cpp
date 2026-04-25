#include <Arduino.h>
#include <RF24.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <SPI.h>
#include <pins.h>
#include <main.h>

RF24 radio(PIN_RF_CE, PIN_RF_CSN);





bool Assert_setting(bool result, const char* msg){
  Serial.print(msg);
  if(result){
    Serial.println(": OK"); 
  }else{
    Serial.println(": Error");
  }
  return result;
}

void setup_testcarrier(rf24_pa_dbm_e level, uint8_t channel){
  radio.powerDown();
  delayMicroseconds(200);
  radio.powerUp();
  delayMicroseconds(200);
  radio.stopListening();
  radio.setPALevel(level, false);
  radio.setDataRate(RF24_1MBPS);
  radio.setAutoAck(false);
  radio.setRetries(0,0);
  radio.startConstCarrier(level, channel);
}


void re_init_modem(rf24_pa_dbm_e power_level){
  radio.powerDown();
  delayMicroseconds(200);
  
  radio.powerUp();
  delayMicroseconds(200);
  
  
  radio.disableAckPayload(); //SLår acks fra
  radio.setDataRate(network_params.bitRate); //1 MBITS / S 
  radio.setAutoAck(false); //SLår auto acks fra  i modtager
  radio.setPALevel(power_level, true); //max transmit power 
  radio.setChannel(network_params.channel); //Vi bruger channel 10
  //radio.disableCRC();
  radio.openWritingPipe(network_params.pipe_name);
  radio.openReadingPipe(0, network_params.pipe_name);
}

void setup_modem(rf24_pa_dbm_e power_level){

  
  Assert_setting(radio.begin(&SPI), "Setting up modem (trust this one)");
  Assert_setting(radio.isChipConnected(), "Chip connected (dont trust this one too much)");
  
  if(radio.isPVariant()){
    Serial.println("CHIP is NRF24L01+");
  } else{
    Serial.println("CHIP is NRF24L01");
  }
  re_init_modem(power_level);

}


void modem_tx(){
  radio.stopListening(); //Fortæller svinet den skal gå i tx
  //TODO Sætte GPIO rf switches
}



void modem_rx(){
  radio.startListening();
  //TODO Sætte GPIO rf switches
}