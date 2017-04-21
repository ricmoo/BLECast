#include <SPI.h>
#include <RF24.h>

#define INITIAL_CHANNEL    37

#if ENABLE_DUMP
#include <arduino.h>
#endif

#include "blecast.h"

#include "aes.h"

typedef struct BLECastPayload {
    uint8_t index;
    char data[12];
    BLECastPayload *nextPayload;
} BLECastPayload;

#define BUFFER_SIZE      40

typedef struct RadioInfo {
    RF24 *radio;
    uint8_t channel;
    uint8_t buffer[BUFFER_SIZE];
} RadioInfo;


// http://sunsite.icm.edu.pl/gnupg/rfc2440-6.html

#define CRC24_INIT      0xb704ce
#define CRC24_POLY      0x1864cfb

static uint32_t computeCrc24(uint8_t *data, uint16_t length) {
    uint32_t crc = CRC24_INIT;

    for (uint16_t i = 0; i < length; i++) {
        crc ^= ((uint32_t)data[i]) << 16;

        for (uint8_t j = 0; j < 8; j++) {
            crc <<= 1;
            if (crc & 0x1000000) {
                crc ^= CRC24_POLY;
            }
        }
    }

    return crc & 0xffffff;
}
#define CHANNEL_NEXT       0

static void ble_setChannel(RadioInfo *radioInfo, uint8_t channel) {

    if (channel == CHANNEL_NEXT) {
        channel = radioInfo->channel + 1;
    }

    // Invalid channels become 37
    if (channel < 37 || channel > 39) {
        channel = 37;
    }

    // Advertising frequencies (the nRF2401 adds 2400 to these values)
    static const uint8_t frequencies[] = { 2, 26, 80};

    radioInfo->channel = channel;
    radioInfo->radio->setChannel(frequencies[channel - 37]);
}


static bool blecast_init(BLECastMessage *message) {
    message->data = NULL;
    message->headPayload = NULL;
    message->totalPayloadCount = -1;
    message->discoveredPayloadCount = 0;
    message->size = -1;
    message->id = BLECAST_INVALID_ID;
}


bool blecast_init(BLECastMessage *message, uint8_t key[16], uint8_t pin1, uint8_t pin2) {
    blecast_init(message);

    aes_init();
    message->aesContext = (aes_decrypt_ctx*)(malloc(sizeof(aes_decrypt_ctx)));
    aes_decrypt_key128(key, message->aesContext);

    if (pin1 > 0 && pin2 > 0) {
        RadioInfo *radioInfo = (RadioInfo*)malloc(sizeof(RadioInfo));
        message->radioInfo = radioInfo;

        RF24 *radio = new RF24(pin1, pin2);
        radioInfo->radio = radio;

        radio->begin();
        radio->setAutoAck(false);
        radio->setDataRate(RF24_1MBPS);
        radio->setPALevel(RF24_PA_MAX);
        radio->setRetries(0, 0);

        radio->disableCRC();

        ble_setChannel(radioInfo, INITIAL_CHANNEL);

        radio->setAddressWidth(4);

        // Access Address; see Core Specification B.2.1.2 (reverseBits(0x8E89BED6))
        radio->openReadingPipe(0, 0x6B7D9171);
        //radio->openWritingPipe(0x6B7D9171);

        radio->powerUp();
    }
}

void blecast_free_payloads(BLECastMessage *message) {
    BLECastPayload *current = message->headPayload;
    while (current) {
        BLECastPayload *toFree = current;
        current = current->nextPayload;
        free(toFree);
    }

    message->headPayload = NULL;

    free(message->data);
    message->data = NULL;
}

void blecast_free(BLECastMessage *message) {
    blecast_free_payloads(message);

    free(message->aesContext);

    RadioInfo *radioInfo = (RadioInfo*)(message->radioInfo);
    radioInfo->radio->powerDown();
    delete radioInfo->radio;

    delete message->radioInfo;
}

void blecast_reset(BLECastMessage *message) {
    blecast_free_payloads(message);
    blecast_init(message);
}


#if ENABLE_DUMP

void blecast_dump(BLECastMessage *message) {

    Serial.print("Message count=");
    Serial.print(message->totalPayloadCount);
    Serial.print(", discovered=");
    Serial.print(message->discoveredPayloadCount);
    Serial.print(", size=");
    Serial.print(message->size);
    Serial.print("\n");

    BLECastPayload* current = (BLECastPayload*)(message->headPayload);
    while (current) {
        Serial.print("  Payload ");
        Serial.print(current->index & 0x3f);
        Serial.print(": ");
        for (uint8_t i = 0; i < 12; i++) { Serial.print((uint8_t)(current->data[i]), HEX); Serial.print(" "); }
        Serial.println("");

        current = current->nextPayload;
    }

    Serial.println("\n");

}

#endif


bool blecast_addPayload(BLECastMessage *message, uint8_t *data) {
    // Already done
    if (message->data) { return false; }

    // Decrypt the payload
    aes_ecb_decrypt(data, data, 16, message->aesContext);

    // This is the CRC to match
    uint32_t payloadCrc = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];

    // De-noise the payload data
    for (uint32_t i = 3; i < 16; i++) {
        data[i] ^= (payloadCrc >> (uint32_t)(i - 3));
    }

    // Compute the CRC (while removing the noise applied during shrink-wrapping)
    uint32_t computedPayloadCrc = computeCrc24(&data[3], 13);

    // Check the CRC (either not for us of data transmission error)
    if (payloadCrc != computedPayloadCrc) { return false; }

    // Done with the CRC; strip it
    data += 3;

    // The index byte; [terminal1] [partial1] [index6]
    uint8_t index = data[0];
    data++;

    // If we don't have this payload, add it (linked list)
    BLECastPayload *payload;
    if (message->headPayload == NULL) {
         payload = (BLECastPayload*)malloc(sizeof(BLECastPayload));
         payload->nextPayload = NULL;

         message->headPayload = payload;

    } else {

        // Scan current payloads...
        BLECastPayload *current = message->headPayload;
        while (current) {
            // ..already have this payload, bail
            if (current->index == index) { return false; }
            current = current->nextPayload;
         }

         // Insert the payload at the head of the payloads
         payload = (BLECastPayload*)malloc(sizeof(BLECastPayload));
         payload->nextPayload = message->headPayload;

         message->headPayload = payload;
    }

    payload->index = index;

    memcpy(payload->data, data, 12);

    message->discoveredPayloadCount++;

    // Last payload for this message
    if ((index & 0x80) && message->totalPayloadCount == -1) {
        message->totalPayloadCount = (index & 0x3f) + 1;

        // A partial payload (the length is in the last payload content slot)
        if (index & 0x40) {
            message->size = (message->totalPayloadCount - 1) * 12 + data[11];

        } else {
            message->size = message->totalPayloadCount * 12;
        }
    }

    //blecast_dump(message);

    // This can happen when switching between messages; stray payloads
    // got picked up from a previous message without the terminal.
    if (message->totalPayloadCount != -1 &&  message->totalPayloadCount < message->discoveredPayloadCount) {
        //Serial.println("Stale Payload; flushing");
        blecast_reset(message);
        return false;
    }

    // Message complete!
    if (message->totalPayloadCount == message->discoveredPayloadCount) {

        // The combined message, on the stack
        uint8_t output[message->size];

        BLECastPayload* current = (BLECastPayload*)(message->headPayload);
        while (current) {
            uint8_t length = 12;
            if (current->index &0x40) { length = current->data[11]; }
            memcpy(&output[12 * (current->index & 0x3f)], current->data, length);
            current = current->nextPayload;
        }

        // Remove all the allocated memory (we will do another malloc below; hopefully no fragmentation)
        blecast_free_payloads(message);

        // If the message is more than 1 block, if contains an additional message CRC prefix
        uint8_t *outputPtr = output;
        if (message->size > 12) {
            uint32_t computedMessageCrc = computeCrc24(&output[3], message->size - 3);
            uint32_t messageCrc = ((uint32_t)output[0] << 16) | ((uint32_t)output[1] << 8) | output[2];
            if (computedMessageCrc != messageCrc) {
                //Serial.println("Bad Checksum; flushing");
                blecast_init(message);
                return false;
            }

            message->size -= 3;
            outputPtr += 3;

            message->id = messageCrc;

        } else {
            message->id = payloadCrc;
        }

        message->data = (uint8_t*)malloc(message->size);
        memcpy(message->data, outputPtr, message->size);

        return true;
    }

    return false;
}

bool blecast_poll(BLECastMessage *message) {
    if (message->data) { return false; }

    bool success = false;

    RadioInfo *radioInfo = (RadioInfo*)(message->radioInfo);

    radioInfo->radio->startListening();
    delay(10);

    while (radioInfo->radio->available()) {
        radioInfo->radio->read(radioInfo->buffer, BUFFER_SIZE);

        // Pre-computed whiten mask for each channel (byte[0] and byte[13:13 + 16]
        const uint8_t whitenMask[] = {
            // Channel 37
            0x8d, 0x77, 0xf8, 0xe3, 0x46, 0xe9, 0xab, 0xd0, 0x9e,
            0x53, 0x33, 0xd8, 0xba, 0x98, 0x08, 0x24, 0xcb,

            // Channel 38
            0xd6, 0x4e, 0xcd, 0x60, 0xeb, 0x62, 0x22, 0x90, 0x2c,
            0xef, 0xf0, 0xc7, 0x8d, 0xd2, 0x57, 0xa1, 0x3d,

            // Channel 39
            0x1f, 0x59, 0xde, 0xe1, 0x8f, 0x1b, 0xa5, 0xaf, 0x42,
            0x7b, 0x4e, 0xcd, 0x60, 0xeb, 0x62, 0x22, 0x90
        };

        uint8_t *data = radioInfo->buffer;
        uint8_t *whiten = &whitenMask[(radioInfo->channel - 37) * 17];

        uint8_t b = *data;
        // @TODO: Pre-compute this and just do a comparison instead of the below 0x40
        b = ((b * 0x0802LU & 0x22110LU) | (b * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
        b ^= *(whiten++);

        // PDU type must be ADV_NONCONN_IND (E.7.7.65.13)
        if (b != 0x40) { continue; }

        data += 13;
        for (uint8_t index = 0; index < 16; index++) {
            uint8_t b = *data;

            // Reverse the bits
            // https://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith32Bits
            b = ((b * 0x0802LU & 0x22110LU) | (b * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;

            // De-whiten
            b ^= *(whiten++);

            *(data++) = b;
        }

        bool complete = blecast_addPayload(message, &(radioInfo->buffer)[13]);
        if (complete) { success = true; }
    }

    radioInfo->radio->stopListening();

    ble_setChannel(radioInfo, CHANNEL_NEXT);

    return success;
}
