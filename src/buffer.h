#ifndef BUFFER_H_INCLUDED
#define BUFFER_H_INCLUDED

#include "signal.h"

/* Packet maxima */
#define PACKET_BUFFER_SIZE 200 
#define PACKETSET_BUFFER_SIZE 5
#define SIGNAL_BUFFER_SIZE 500

/////////// DATA STRUCTURES //////////////

typedef struct PacketRingBuffer
{
    Packet buffer[PACKET_BUFFER_SIZE];
    bool occupied[PACKET_BUFFER_SIZE];
    int head;
} PacketRingBuffer;

typedef struct PacketSetRingBuffer {
    PacketSet buffer[PACKETSET_BUFFER_SIZE];
    bool occupied[PACKET_BUFFER_SIZE];
    int head;
} PacketSetRingBuffer;

typedef struct SignalRingBuffer {
    Signal buffer[SIGNAL_BUFFER_SIZE];
    bool occupied[SIGNAL_BUFFER_SIZE];
    int head; // used to store new values by the network thread
    int tail; // used to chase the head in the writer thread
} SignalRingBuffer;

///////////// PROTOTYPES /////////////

void clearBuffers();

Packet* pushPacketAtHead(Packet);
void removePacketFromBuffer(Packet* pp);

PacketSet* pushPacketSetAtHead(PacketSet);
void removePacketSetFromBuffer(PacketSet* ppset);

Signal* pushSignalAtHead(Signal);
int getSignalCountInBuffer();
void removeSignalFromBuffer(Signal* pp);
bool popSignalFromTail(Signal*);

PacketSet* findPacketSetForPacket(Packet*);
PacketSet* createPacketSetForPacket(Packet*);

bool checkReceivedAllPackets(PacketSet*);
void processPacketSet(PacketSet*);
void processData();

void logIncompletePacketSet(const PacketSet*);
void logDroppedSignal(const Signal* ps);

#endif
