#include <Arduino.h>
#include <RF24.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <SPI.h>
#include <ESP32-c3_pinout.h>
#include <main.h>

RF24 radio(PIN_CE, PIN_CSN);


void block_item::print_payload(){
  Serial.printf("Payload (hex):\n");
  for (int i = 0; i < 32; i++) {
      // Print byte i 2-cifret hex
      Serial.printf("%02X ", block_payload[i]);
      // Ny linje hver 8 bytes for readability
      if ((i + 1) % 8 == 0) {
          Serial.printf("\n");
      }
  }
  Serial.printf("CRC: %s\n", crc ? "true" : "false");
}


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
  
  Assert_setting(radio.begin(&SPI), "Setting up modem (trust this one)");
  Assert_setting(radio.isChipConnected(), "Chip connected (dont trust this one too much)");
  
  radio.disableAckPayload(); //SLår acks fra
  radio.setDataRate(network_params.bitRate); //1 MBITS / S 
  radio.setAutoAck(false); //SLår auto acks fra  i modtager
  radio.setPALevel(power_level, true); //max transmit power 
  radio.setChannel(network_params.channel); //Vi bruger channel 10
  radio.openWritingPipe(network_params.pipe_name);
  radio.openReadingPipe(0, network_params.pipe_name);
  pinMode(PIN_SDA, OUTPUT);
  pinMode(PIN_SCL, OUTPUT);
}


void modem_tx(){
  radio.stopListening(); //Fortæller svinet den skal gå i tx
  //TODO Sætte GPIO rf switches
  digitalWrite(PIN_SCL, HIGH);
}



void modem_rx(){
  radio.startListening();
  //TODO Sætte GPIO rf switches
  digitalWrite(PIN_SCL, LOW);
}