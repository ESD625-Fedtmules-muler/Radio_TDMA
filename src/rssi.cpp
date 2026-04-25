#include <RadioLib.h>
#include <pins.h>
#include <network_params.h>

LR1121 LBAA = new Module(
  PIN_RF_LR1121_CSN,     // NSS (CS)
  RADIOLIB_NC,    // BUSY  <-- HER!
  RADIOLIB_NC,           // DIO1 (kan være NC hvis ikke brugt)
  RADIOLIB_NC,           // RESET (eller giv en pin hvis du bruger den)
  SPI
);


void setup_rssi(){
    int16_t result_Sband = LBAA.begin(); //LBAA.begin(864.0,500.0,8,7,RADIOLIB_LR11X0_LORA_SYNC_WORD_PRIVATE,2,8,1.6);
    Serial.print("Starting LR1121: ");
    Serial.println(result_Sband);
    float freq = 2400 + network_params.channel;
    Serial.printf("Setting freq: %f\n", freq);
    result_Sband = LBAA.setFrequency(freq);
    
    LBAA.setSpreadingFactor(5);
    LBAA.setBandwidth(812.5, true);
    LBAA.setCodingRate(4);
    
    LBAA.startReceive();
    LBAA.getRSSI(false);
    LBAA.startReceive();

}

float speedy_rssi() {
  // === Fase 1: Send kommando ===
  digitalWrite(PIN_RF_LR1121_CSN, LOW);
  delayMicroseconds(100);  // tCSS: CS setup time

  SPI.transfer((RADIOLIB_LR11X0_CMD_GET_RSSI_INST >> 8) & 0xFF);
  SPI.transfer( RADIOLIB_LR11X0_CMD_GET_RSSI_INST        & 0xFF);

  delayMicroseconds(100);
  digitalWrite(PIN_RF_LR1121_CSN, HIGH);


  delayMicroseconds(1000);  // fallback hvis ingen BUSY-pin

  // === Fase 2: Læs response ===
  digitalWrite(PIN_RF_LR1121_CSN, LOW);
  delayMicroseconds(100);

  uint8_t stat = SPI.transfer(0x00);  // statsbyte (kan tjekkes for fejl)
  uint8_t rssi_raw = SPI.transfer(0x00);  // selve RSSI-værdien

  digitalWrite(PIN_RF_LR1121_CSN, HIGH);

  // RSSI formel: dBm = -rawvalue / 2
  float rssi = -(float)rssi_raw / 2.0f;

  return rssi +11;
}