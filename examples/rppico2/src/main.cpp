#include <Arduino.h>
#include "WinbondW25N3ea.h"
#include "SPI.h"

W25N3EA flash = W25N3EA();

char* myString = "Hello world from the W25N flash chip!";
char buf[512];

#define my_FLASH_CS   5    // Chip Select
#define my_FLASH_SCK  6    // SCK
#define my_FLASH_MOSI 7    // MOSI
#define my_FLASH_MISO 4    // MISO

#define MY_LED_PIN 29 // LED pin for debugging

// Function to zero out a buffer
void zeroBuffer(char* buf, uint32_t buflen);


void setup(){
  pinMode(MY_LED_PIN, OUTPUT);
  Serial.begin(57600);
  while(!Serial);
  //Make sure the buffer is zero'd
  zeroBuffer(buf, sizeof(buf));

  //Initialize the SPI bus in a while loop until it is successful
  while(flash.spiBegin(my_FLASH_CS, my_FLASH_MOSI, my_FLASH_MISO, my_FLASH_SCK)==1){
    Serial.println("SPI initialization failed, retrying...");
    delay(1000);
    digitalWrite(MY_LED_PIN, !digitalRead(MY_LED_PIN)); // Toggle LED to indicate retry
  }
  Serial.println("SPI initialized successfully");

  //Set the LED to indicate we are ready
  digitalWrite(MY_LED_PIN, HIGH);
  Serial.println("Starting flash chip initialization...");

  //Wait for a second before starting the flash chip initialization
  //This is to ensure that the SPI bus is stable
  delay(1000);

  //Initialize the flash chip
  while(flash.begin()==1){
    Serial.println("Flash chip initialization failed, retrying...");
    // If initialization fails, toggle the LED and wait before retrying
    delay(300);
    digitalWrite(MY_LED_PIN, !digitalRead(MY_LED_PIN)); // Toggle LED to indicate retry
  }
  Serial.println("Flash chip initialized successfully");

  //put the string into our flash buffer
  //We cant point to the string directly as it is static
  //and the buffer will be modified with the recieved data
  memcpy(buf, myString, strlen(myString) + 1);
  //erase the flash page we will be writing to
  flash.blockErase(0);

  //Transfer the data we want to program and execute the program command
  flash.loadProgData(0, buf, strlen(myString) + 1);
  flash.ProgramExecute(0);

  //zero the buffer again just so we know that the data we print will be from the flash
  zeroBuffer(buf, sizeof(buf));

  //read the data back and print in it
  flash.pageDataRead(0);
  flash.read(0, buf, sizeof(buf));

  Serial.print("Data read from flash: ");
  Serial.println(buf);

  digitalWrite(MY_LED_PIN, HIGH); // Turn on LED to indicate success
}

void loop(){
}

void zeroBuffer(char* buf, uint32_t buflen){
  for(int i = 0; i < buflen; i++){
    buf[i] = 0;
  }
}
