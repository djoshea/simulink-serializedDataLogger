/* Serialized Data File Logger
 *
 */
 
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include<unordered_map>
#include<string>
#include<inttypes.h>
#include<pthread.h>
#include<signal.h>

// MATLAB mat file library
#include "mat.h"
#include "matrix.h"

#define DEBUG_ON 1

#define PORT 25000 

/* Packet maxima */
#define MAX_PACKETS_PER_TICK 20 
#define PACKET_BUFFER_SIZE MAX_PACKETS_PER_TICK
#define MAX_PACKET_LENGTH 1500
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

const char* dataTypeIdNames[] = {"double", "single", "int8", "uint8", "int16", "uint16", "int32", "uint32"};

typedef struct Packet 
{
    uint16_t packetVersion;
    uint32_t timestamp;
    uint16_t numPackets;
    uint16_t idxPacket; // 1-indexed

    uint8_t rawData[MAX_PACKET_LENGTH];
    uint16_t rawLength;
} Packet;

typedef struct PacketRingBuffer
{
    Packet buffer[PACKET_BUFFER_SIZE];
    bool occupied[PACKET_BUFFER_SIZE];
    int head;
    int tail;
} PacketRingBuffer;

typedef struct PacketSet
{
    uint32_t timestamp;
    uint16_t numPackets;

    bool packetReceived[MAX_PACKETS_PER_TICK];
    Packet* pPacket[MAX_PACKETS_PER_TICK];
} PacketSet;

typedef struct PacketSetRingBuffer {
    PacketSet buffer[PACKETSET_BUFFER_SIZE];
    bool occupied[PACKET_BUFFER_SIZE];
    int head;
    int tail;
} PacketSetRingBuffer;

typedef struct Signal
{
    uint32_t timestamp;
    char name[MAX_SIGNAL_NAME];
    uint8_t dataTypeId;
    uint8_t nDims;
    uint16_t dims[MAX_SIGNAL_NDIMS];
    uint8_t data[MAX_SIGNAL_SIZE];
} Signal;

typedef struct SignalRingBuffer {
    Signal buffer[SIGNAL_BUFFER_SIZE];
    bool occupied[PACKET_BUFFER_SIZE];
    int head;
    int tail;
} SignalRingBuffer;

//////////////////////////////////////

PacketRingBuffer pbuf;
PacketSetRingBuffer psetbuf;
SignalRingBuffer sbuf;

///////////// PROTOTYPES /////////////

void processPacket(uint8_t* rawPacket, int bytesRead);
void processPacketSet();
void printPacket(Packet p);
bool checkReceivedAllPackets();
void processData();
void resetPacketBuffer();
uint8_t getSizeOfDataTypeId(uint8_t);
const char * getDataTypeIdName(uint8_t dataTypeId);
void printSignal(Signal s);

///////////// GLOBALS /////////////

int sock;
Packet packetBuffer[PACKET_BUFFER_SIZE];
bool packetReceived[PACKET_BUFFER_SIZE];

uint8_t packetDataBuffer[MAX_DATA_SIZE_PER_TICK];
int packetDataBufferBytes;

Signal s;

Signal signalBuffer[SIGNAL_BUFFER_SIZE];
int signalBufferUsed = 0;

#define UNKNOWN_PACKET_COUNT -1
int numPacketsThisTick = UNKNOWN_PACKET_COUNT;

void diep(const char *s)
{
    perror(s);
    exit(1);
}


void finish_main(int sig)
{
    printf("Finishing Main\n");
    // pthread_cancel(fileThread);
    // pthread_join(fileThread, NULL);
    // fclose(fnumFile);
    close(sock);
    exit(-1);
}


int main(int argc, char *argv[])
{
    uint8_t rawPacket[MAX_PACKET_LENGTH];
    int port = PORT;
    bool receivedAll; 

    printf("Starting Data Logger on port %d\n", port);

    // Setup Socket Variables
    struct sockaddr_in si_me, si_other;
    int slen=sizeof(si_other);

    if ((sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
        diep("socket");

    // Setup Local Socket on specified port to accept from any address
    memset((char *) &si_me, 0, sizeof(si_me)); 
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(port);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock,(struct sockaddr*) &si_me, sizeof(si_me))==-1)
        diep("bind");

    //Register INT handler
    signal(SIGINT, finish_main);

    printf("Socket bound and waiting...\n");

    resetPacketBuffer();

    while(1)
    {

        // Read from the socket 
        int bytesRead = recvfrom(sock, rawPacket, MAX_PACKET_LENGTH, 0,
                (struct sockaddr*)&si_other, (socklen_t *)&slen);

#if DEBUG_ON 
        //printf("Bytes Read: %d \n", bytesRead);
#endif

        if(bytesRead == -1)
            diep("recvfrom()");

        // parse rawPacket into a Packet struct
        Packet p;
        p = processPacket(rawPacket, bytesRead);

        // add Packet to the head of the packet buffer
        Packet* pPacket;
        pPacket = pushPacketAtHead(p);

        PacketSet* pPacketSet;
        pPacketSet = findOrCreatePacketSetForPacket(pPacket);

        // have we received the full group of packets yet?
        receivedAll = checkReceivedAllPackets(pPacketSet);
        if (receivedAll) {
            processPacketSet();

            resetPacketBuffer();
        }
    }

    close(sock);

    return 0;

}

Packet parsePacket(uint8_t* rawPacket, int bytesRead)
{
    Packet p;

    if (bytesRead < 8)
        diep("Packet too short!");

    uint8_t* pBuf = rawPacket;

    // store the packet version
    STORE_UINT16(pBuf, p.packetVersion);

    // store the timestamp
    STORE_UINT32(pBuf, p.timestamp);

    // store the number of packets
    STORE_UINT16(pBuf, p.numPackets);

    // store the 1-indexed packet number
    STORE_UINT16(pBuf, p.idxPacket);

    // compute the data length in this packet
    p.rawLength = bytesRead - (pBuf - rawPacket);
    
    // copy the raw data into the data buffer
    STORE_UINT8_ARRAY(pBuf, p.rawData, p.rawLength);

    return p;
}

Packet * pushPacketAtHead(Packet p)
{
    printf("PacketBuffer: Adding P for timestamp %d\n", p.timestamp);
    pbuf.head = (pbuf.head + 1) % PACKET_BUFFER_SIZE;

    if(pbuf.occupied[pbuf.head])
        diep("Packet Buffer Overrun!");

    pbuf.buffer[pbuf.head] = p;
    pbuf.occupied[pbuf.head] = 1;

    // return a pointer to the newly created packet
    return pbuf.buffer + pbuf.head;
}

PacketSet * pushPacketSetAtHead(PacketSet p)
{
    printf("PacketSetBuffer: Adding PS for timestamp %d\n", p.timestamp);
    psetbuf.head = (psetbuf.head + 1) % PACKETSET_BUFFER_SIZE;

    if(psetbuf.occupied[psetbuf.head])
        diep("Packet Set Buffer Overrun!");

    psetbuf.buffer[pbuf.head] = p;
    psetbuf.occupied[pbuf.head] = 1;

    // return a pointer to the newly created packet
    return psetbuf.buffer + psetbuf.head;
}

PacketSet* findOrCreatePacketSetForPacket(const Packet* pPacket)
{
    PacketSet pset;
    uint32_t ts = pPacket->timestamp;

    // search backwards through the PacketSetBuffer for a PacketSet with a matching timestamp
    for(int i = psetbuf.head; i >= psetbuf.tail; i--) 
    {
        if (psetbuf.occupied[i] && psetbuf.buffer[i].timestamp == ts)
            // found it!
            return psetbuf.buffer + i;
    }

    // not found, create one
    memset(&pset, 0, sizeof(PacketSet));

    pset.timestamp = pPacket->timestamp;
    pset.numPackets = pPacket->numPackets;
    pset.packetReceived[pPacket->idxPacket] = 1;
    pset.pPacket[pPacket->idxPacket] = pPacket;

    // add to head of buffer
    return pushPacketSetAtHead(pset); 
}

bool checkReceivedAllPackets()
{
    // check whether we've received all the packets for this tick
    for(int i = 0; i < numPacketsThisTick; i++) {
        if (!packetReceived[i]) {
            return 0;
        }
    }

    return 1;
}

void processPacketSet()
{
    if (numPacketsThisTick == 0)
        return;

    // buffer to store all packet data
    memset(packetDataBuffer, 0, MAX_DATA_SIZE_PER_TICK * sizeof(uint8_t));

    int bufOffset = 0;
    for(int i = 0; i < numPacketsThisTick; i++) {
        // copy over this packet's data into the data buffer
        memcpy(packetDataBuffer + bufOffset, packetBuffer[i].rawData, 
                packetBuffer[i].rawLength * sizeof(uint8_t));
        
        // advance the offset into the data buffer
        bufOffset += packetBuffer[i].rawLength * sizeof(uint8_t);
    }

    packetDataBufferBytes = bufOffset;

    printf("\nPacketSet : [ Timestamp %d, %d bytes, %d packets ]\n", 
            packetBuffer[0].timestamp, packetDataBufferBytes, numPacketsThisTick);

    processData();
}

void processData()
{
    uint8_t* pBuf = packetDataBuffer;

    while(pBuf - packetDataBuffer < packetDataBufferBytes) {
        memset(&s, 0, sizeof(Signal));
        
        // get the number of bytes in the signal name
        uint16_t lenName;
        STORE_UINT16(pBuf, lenName);

        // store the signal name
        STORE_UINT8_ARRAY(pBuf, s.name, lenName);

        // store the data type
        STORE_UINT8(pBuf, s.dataTypeId);
       
        // store the number of dimensions
        STORE_UINT8(pBuf, s.nDims);

        // store the dimensions
        STORE_UINT16_ARRAY(pBuf, s.dims, s.nDims); 
       
        // compute the number of bytes in the data
        uint16_t nElements = 1;
        for(int idim = 0; idim < s.nDims; idim++) {
            nElements *= s.dims[idim];
        }

        uint16_t bytesForData = nElements * getSizeOfDataTypeId(s.dataTypeId);

        STORE_UINT8_ARRAY(pBuf, s.data, bytesForData);

        printSignal(s);
    }

}

void addToSignalBuffer(Signal s)
{
    
}

uint8_t getSizeOfDataTypeId(uint8_t dataTypeId) 
{
    switch (dataTypeId) {
    case 0: // double
        return 8;
    case 1: // single
        return 4;
    case 2: // int8
    case 3: // uint8
        return 1;
    case 4: // int16
    case 5: // uint16
        return 2;
    case 6: // int32
    case 7: // uint32
        return 4;
    default:
        diep("Unknown data type Id");
    }
}

void resetPacketBuffer()
{
    Packet p;

    // clear the packet
    memset(&p, 0, sizeof(Packet));

    for (int i = 0; i < MAX_PACKETS_PER_TICK; i++) {
        packetBuffer[i] = p;
        packetReceived[i] = 0;
    }
}

void printPacket(Packet p)
{
    printf("\tPacket [ Timestamp %d, packet %d of %d, version %d ]\n", 
            p.timestamp, p.idxPacket, p.numPackets, p.packetVersion);
}

const char * getDataTypeIdName(uint8_t dataTypeId)
{
    if(dataTypeId < 0 || dataTypeId > 7)
        diep("Invalid data type Id");

    return dataTypeIdNames[dataTypeId];
}

void printSignal(Signal s)
{
    int nElements = 1;

    printf("\t%10s [", s.name);
    
    if (s.nDims == 1) {
        printf("%4d x %4d", (int)s.dims[0], 1);
        nElements = s.dims[0];
    } else {
        for(int idim = 0; idim < s.nDims; idim++) {
            nElements *= s.dims[idim];
            printf("%4d", (int)s.dims[idim]);
            if(idim < s.nDims - 1)
                printf(" x ");
        }
    }
    printf(" %6s ] : ", getDataTypeIdName(s.dataTypeId));

    printf("%s ", (char*)s.data);
    /*
    for(int i = 0; i < nElements; i++) {
        printf("%d ", (int)s.data[i]);
    }*/

    printf("\n");
}



