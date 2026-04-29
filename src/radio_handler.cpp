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

void setup_testcarrier(rf24_pa_dbm_e level, uint8_t channel) {
    // startConstCarrier() håndterer selv power sekvensen
    radio.startConstCarrier(level, channel);
}


void stop_testcarrier(rf24_pa_dbm_e power_level) {
    radio.stopConstCarrier();   // Sætter PWR_UP = 0, rydder CONT_WAVE
    digitalWrite(PIN_I2C_SCL, LOW);

    delay(5);                  // Giv chippen tid - ESP32 er hurtig
    radio.powerUp();            // Vækk den igen
    re_init_modem(power_level);
}


void re_init_modem(rf24_pa_dbm_e power_level) {

    
    radio.setDataRate(network_params.bitRate);
    radio.setAutoAck(false);
    radio.disableAckPayload();
    radio.setPALevel(power_level, true);
    radio.setChannel(network_params.channel);
    radio.setRetries(0, 0);                          // Du har det i testcarrier men ikke her
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

  radio.powerDown();
  delay(10);                                        // Tpd2stby - vigtigt!
  radio.powerUp();
  delay(5);
  re_init_modem(power_level);
  radio.stopListening();                            // Definér en start-tilstand

}


void modem_tx(){
  radio.stopListening(); //Fortæller svinet den skal gå i tx
  //TODO Sætte GPIO rf switches
}



void modem_rx(){
  radio.startListening();
  //TODO Sætte GPIO rf switches
}