/**
 *  BLECast Example
 *
 *  This example will listen on the BLE advertising channels for
 *  messages matching the BLECast protocol for the provided key
 *  and print them to the Serial Monitor.
 *
 *  The nRF2401 is a cheap half-duplex radio that can send
 *  and receive packets in the 2400Mhz ISM frequency.
 *
 *  Pins:
 *   _______________________________________________________
 *  |                                                       |
 *  |   |                _______                            |
 *  |   |               ( 16Mhz )                           |
 *  |   |__             (_______)       IRQ  8  7  MISO     |
 *  |    __|  |                                             |
 *  |   |__   |                        MOSI  6  5  SCK      |        
 *  |    __|  |           ____                              |
 *  |   |__   |          |    |         CSN  4  3  CE       |
 *  |    __|  |          |    |                             |
 *  |   |_____|_____     |____|         VIN  2  1  GND      |
 *  |                                                       |
 *  |_______________________________________________________|
 *                                                   nRF2401
 *   GND        Ground
 *   VIN        The Voltage In (3.3V)
 *   CE         Chip Enable (selects RX or TX mode)
 *   CSN        Chip Select Not (SPI)
 *   SCL        Serial Clock (output from master; SPI)
 *   MOSI       Master Output, Slave Input (SPI)
 *   MISO       Master Input, Slave Output (SPI)
 *   IRQ        The Interrupt Request Line
 *
 *  Data Sheet
 *    - https://www.sparkfun.com/datasheets/Components/nRF24L01_prelim_prod_spec_1_2.pdf
 *
 *  Useful Links
 *    - http://www.diyembedded.com/tutorials/nrf24l01_0/nrf24l01_tutorial_0.pdf
 *    - http://arduino-info.wikispaces.com/Nrf24L01-2.4GHz-HowTo
 *    - https://en.wikipedia.org/wiki/Serial_Peripheral_Interface_Bus
 *    - http://dmitry.gr/index.php?r=05.Projects&proj=11.%20Bluetooth%20LE%20fakery
 *
 *  Order Online (includes my affiliate ID)
 *    - nRF2401 (x10): http://amzn.to/2pIAWCN
 *    - Arduino Nano (x5): http://amzn.to/2pWW6cJ
 *
 *  Arduino Uno and Nano:
 *    GND   <=>   GND
 *    VIN   <=>   3.3
 *    CE    <=>   Pin D9
 *    CSN   <=>   Pin D10
 *    SCK   <=>   Pin D13
 *    MOSI  <=>   Pin D11
 *    MISO  <=>   Pin D12
 */


#include "blecast.h"

BLECastMessage message;

uint32_t lastMessageId = BLECAST_INVALID_ID;

void setup() {

    // Set up the Serial Monitor
    Serial.begin(115200);
    while (!Serial) { }

    Serial.println("BLECast ready.");

    // The secret key to listen for
    uint8_t key[] = { 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 2, 2, 3, 3, 4, 4 };

    // Initialize the message data structure
    blecast_init(&message, key, 9, 10);
}

void loop() {
    
    // Poll for a message (returns true if a new message is complete)
    if (blecast_poll(&message)) {
        
        // A different message
        if (message.id != lastMessageId) {

            Serial.print("Got Message: ");

            // Print each characater and a new line
            for (uint8_t i = 0; i < message.size; i++) { Serial.print((char)(message.data[i])); }
            Serial.println("\n");
             
            // Remember this message ID
            lastMessageId = message.id; 
        }
        
        // Reset the message data structure so it is ready for a new message
        blecast_reset(&message);
    }
}
