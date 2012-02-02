#include<inttypes.h>

#ifndef SIGNAL_H_INCLUDE
#define SIGNAL_H_INCLUDE

/* packet maxima */
#define MAX_PACKET_LENGTH 1500
#define MAX_PACKETS_PER_TICK 20 
#define MAX_DATA_SIZE_PER_TICK (MAX_PACKETS_PER_TICK * MAX_PACKET_LENGTH)

/* Signal maxima */
#define MAX_SIGNAL_NAME 200
#define MAX_SIGNAL_SIZE 10000 
#define MAX_SIGNAL_NDIMS 10


///////////// DATA TYPES DECLARATIONS /////////////

typedef float single_t;
typedef double double_t;

///////////// UINT8_T BUFFER UTILS ///////

// these macros help with the process of pulling bytes off of a uint8_t buffer
// [bufPtr], typecasting it as [type], and storing it in [assignTo]. The bufPtr
// is automatically advanced by the correct number of bytes. assignTo must be of
// type [type], not a pointer.
#define STORE_TYPE(type, bufPtr, assignTo) \
    memcpy(&assignTo, bufPtr, sizeof(type)); bufPtr += sizeof(type) 

#define STORE_INT8(bufPtr,   assignTo) STORE_TYPE(int8_t,   bufPtr, assignTo) 
#define STORE_UINT8(bufPtr,  assignTo) STORE_TYPE(uint8_t,  bufPtr, assignTo) 
#define STORE_INT16(bufPtr,  assignTo) STORE_TYPE(int16_t,  bufPtr, assignTo) 
#define STORE_UINT16(bufPtr, assignTo) STORE_TYPE(uint16_t, bufPtr, assignTo) 
#define STORE_INT32(bufPtr,  assignTo) STORE_TYPE(int32_t,  bufPtr, assignTo) 
#define STORE_UINT32(bufPtr, assignTo) STORE_TYPE(uint32_t, bufPtr, assignTo) 
#define STORE_SINGLE(bufPtr, assignTo) STORE_TYPE(single_t, bufPtr, assignTo) 
#define STORE_DOUBLE(bufPtr, assignTo) STORE_TYPE(double_t, bufPtr, assignTo) 

// these macros help with the process of pulling nElements*sizeof(type) bytes off 
// of a uint8_t buffer [bufPtr], typecasting them as [type], and storing them 
// into a buffer [pAssign]. The bufPtr is automatically advanced by the correct 
// number of bytes. assignTo must be of type [type*], i.e. a pointer.
#define STORE_TYPE_ARRAY(type, bufPtr, pAssign, nElements) \
    memcpy(pAssign, bufPtr, nElements*sizeof(type)); bufPtr += sizeof(type) * nElements

#define STORE_INT8_ARRAY(   bufPtr, pAssign, nElements) STORE_TYPE_ARRAY(int8_t,   bufPtr, pAssign, nElements) 
#define STORE_UINT8_ARRAY(  bufPtr, pAssign, nElements) STORE_TYPE_ARRAY(uint8_t,  bufPtr, pAssign, nElements) 
#define STORE_INT16_ARRAY(  bufPtr, pAssign, nElements) STORE_TYPE_ARRAY(int16_t,  bufPtr, pAssign, nElements) 
#define STORE_UINT16_ARRAY( bufPtr, pAssign, nElements) STORE_TYPE_ARRAY(uint16_t, bufPtr, pAssign, nElements) 
#define STORE_INT32_ARRAY(  bufPtr, pAssign, nElements) STORE_TYPE_ARRAY(int32_t,  bufPtr, pAssign, nElements) 
#define STORE_UINT32_ARRAY( bufPtr, pAssign, nElements) STORE_TYPE_ARRAY(uint32_t, bufPtr, pAssign, nElements) 
#define STORE_SINGLE_ARRAY( bufPtr, pAssign, nElements) STORE_TYPE_ARRAY(single_t, bufPtr, pAssign, nElements) 
#define STORE_DOUBLE_ARRAY( bufPtr, pAssign, nElements) STORE_TYPE_ARRAY(double_t, bufPtr, pAssign, nElements) 

/////////// DATA STRUCTURES //////////////

typedef struct Packet 
{
    uint16_t packetVersion;
    uint32_t timestamp;
    uint16_t numPackets;
    uint16_t idxPacket; // 1-indexed

    uint8_t rawData[MAX_PACKET_LENGTH];
    uint16_t rawLength;
} Packet;

typedef struct PacketSet
{
    uint32_t timestamp;
    uint16_t numPackets;

    bool packetReceived[MAX_PACKETS_PER_TICK];
    Packet* pPackets[MAX_PACKETS_PER_TICK];
} PacketSet;

typedef struct Signal
{
    uint32_t timestamp;
    char name[MAX_SIGNAL_NAME];
    uint8_t dataTypeId;
    uint8_t nDims;
    uint16_t dims[MAX_SIGNAL_NDIMS];
    uint8_t data[MAX_SIGNAL_SIZE];
} Signal;

////// PROTOTYPES ////////

uint8_t getSizeOfDataTypeId(uint8_t);
const char * getDataTypeIdName(uint8_t);

Packet parsePacket(uint8_t*, int);

void printPacket(const Packet*);
void printPacketSet(const PacketSet*);
void printSignal(const Signal*);

#endif // ifndef SIGNAL_H_INCLUDE
