#ifndef _BLECAST_H_
#define _BLECAST_H_

#define ENABLE_DUMP     0

#define BLECAST_INVALID_ID     0x7f000000


typedef struct BLECastMessage {
    // Total payload counts and unique discovered payload counts
    int8_t discoveredPayloadCount;
    int8_t totalPayloadCount;

    // Total nube of bytes of the message
    int16_t size;

    // Once a message is complete, this is populated (heap)
    uint8_t *data;

    // An ID for this message (BLECAST_INVALID_ID until message is valid)
    uint32_t id;

    // The head node in a linked list of payload structs (heap)
    void *headPayload;

    // The AES context initialized with the key
    void *aesContext;

    // The RF24 radio object (@TODO: replace this?)
    void *radioInfo;
} BLECastMessage;

bool blecast_init(BLECastMessage *message, uint8_t key[16], uint8_t pin1, uint8_t pin2);

bool blecast_poll(BLECastMessage *message);

void blecast_reset(BLECastMessage *message);

void blecast_free(BLECastMessage *message);

#if ENABLE_DUMP

void blecast_dump(BLECastMessage *message);

#endif

#endif
