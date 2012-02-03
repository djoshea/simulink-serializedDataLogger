#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mat.h"
#include <pthread.h>

#include "signal.h"
#include "buffer.h"
#include "serializedDataLogger.h"

pthread_mutex_t signalBufferMutex = PTHREAD_MUTEX_INITIALIZER;

PacketRingBuffer pbuf;
PacketSetRingBuffer psetbuf;
SignalRingBuffer sbuf;
Signal s;

uint8_t packetDataBuffer[MAX_DATA_SIZE_PER_TICK];
int packetDataBufferBytes;
uint32_t packetDataTimestamp;

void clearBuffers() 
{
    memset(&pbuf, 0, sizeof(PacketRingBuffer));
    memset(&psetbuf, 0, sizeof(PacketSetRingBuffer));
    memset(&sbuf, 0, sizeof(SignalRingBuffer));
}

/////// PACKET BUFFER /////////

Packet * pushPacketAtHead(Packet p)
{
    pbuf.head = (pbuf.head + 1) % PACKET_BUFFER_SIZE;

    if(pbuf.occupied[pbuf.head]) {
        // the existing packet will now be lost
        // find its PacketSet, log an error and complain
        PacketSet* ppset;
        ppset = findPacketSetForPacket(&pbuf.buffer[pbuf.head]);
        if (ppset != NULL) {
            logIncompletePacketSet(ppset);
            removePacketSetFromBuffer(ppset);
        }
    }

    pbuf.buffer[pbuf.head] = p;
    pbuf.occupied[pbuf.head] = 1;

    // return a pointer to the newly created packet
    return pbuf.buffer + pbuf.head;
}

void removePacketFromBuffer(Packet* pp)
{
    int index = pp - pbuf.buffer;

    //printf("\tRemoving Packet %d for ts %d at index %d\n", pp->idxPacket, pp->timestamp, index);

    if(index < 0 || index > PACKET_BUFFER_SIZE)
        diep("Attempt to remove Packet not in PacketRingBuffer");

    // clear this packet and mark as unoccupied in buffer
    memset(pp, 0, sizeof(Packet));
    pbuf.occupied[index] = 0;
}

/////// PACKETSET BUFFER /////////

PacketSet * pushPacketSetAtHead(PacketSet p)
{
    psetbuf.head = (psetbuf.head + 1) % PACKETSET_BUFFER_SIZE;
    if(psetbuf.occupied[psetbuf.head]) {
        logIncompletePacketSet(psetbuf.buffer + psetbuf.head);
        removePacketSetFromBuffer(psetbuf.buffer + psetbuf.head);
    }

    memset(&psetbuf.buffer[psetbuf.head], 0, sizeof(PacketSet));
    psetbuf.buffer[psetbuf.head] = p;
    psetbuf.occupied[psetbuf.head] = 1;

    // return a pointer to the newly created packet
    return psetbuf.buffer + psetbuf.head;
}

void removePacketSetFromBuffer(PacketSet* ppset)
{
    int index = ppset - psetbuf.buffer;

    //printf("Removing PacketSet for ts %d at index %d\n", ppset->timestamp, index);

    if(index < 0 || index > PACKETSET_BUFFER_SIZE)
        diep("Attempt to remove PacketSet not in PacketSetRingBuffer");

    // clear the packets that comprise this packet set
    for(int i = 0; i < ppset->numPackets; i++) 
    {
        if(ppset->packetReceived[i])
            removePacketFromBuffer(ppset->pPackets[i]);
    }

    // clear this packet set and mark as unoccupied in buffer
    memset(ppset, 0, sizeof(PacketSet));
    psetbuf.occupied[index] = 0;
}
  
/////// SIGNAL BUFFER /////////

Signal* pushSignalAtHead(Signal s)
{
    // lock the signal buffer mutex 
    pthread_mutex_lock(&signalBufferMutex);

    if(sbuf.occupied[sbuf.head]) {
        // signal buffer overflow 
        logDroppedSignal(&s);
    }

    sbuf.buffer[sbuf.head] = s;
    sbuf.occupied[sbuf.head] = 1;

    // unlock the signal buffer mutex
    pthread_mutex_unlock(&signalBufferMutex);

    // return this pointer to the new packet
    Signal* newPacket = &sbuf.buffer[sbuf.head];
    
    //printf("Storing Signal at head %d, (tail=%d)\n", sbuf.head, sbuf.tail);

    // advance the head
    sbuf.head = (sbuf.head + 1) % PACKET_BUFFER_SIZE;

    // return a pointer to the newly created signal
    return newPacket; 
}

void removeSignalFromBuffer(Signal* ps)
{
    // lock the signal buffer mutex 
    pthread_mutex_lock(&signalBufferMutex);

    int index = ps - sbuf.buffer;

    if(index < 0 || index > SIGNAL_BUFFER_SIZE)
        diep("Attempt to remove Signal not in SignalRingBuffer");

    // clear this signal and mark as unoccupied in buffer
    memset(ps, 0, sizeof(Signal));
    sbuf.occupied[index] = 0;
    
    // unlock the signal buffer mutex
    pthread_mutex_unlock(&signalBufferMutex);
}

int getSignalCountInBuffer()
{
    int i,c, count, tailToHeadDistance;
	count = 0;

    // lock the signal buffer mutex 
    pthread_mutex_lock(&signalBufferMutex);
    
    tailToHeadDistance = (sbuf.head - sbuf.tail);
    if(tailToHeadDistance < 0)
        tailToHeadDistance += SIGNAL_BUFFER_SIZE;

    for(c = 0; c < tailToHeadDistance; c++)
    {
        // translate into the circular index from the tail towards the head
        i = (sbuf.tail + c) % SIGNAL_BUFFER_SIZE;
        count += sbuf.occupied[i];
    }

    // unlock the signal buffer mutex
    pthread_mutex_unlock(&signalBufferMutex);

    return count;
}

// get first signal from the tail onward, store in ps, returns true if one is found
bool popSignalFromTail(Signal* ps)
{
    int i, c, tailToHeadDistance;

    // lock the signal buffer mutex 
    pthread_mutex_lock(&signalBufferMutex);
    
    // only check from tail to head, don't go past head
    tailToHeadDistance = (sbuf.head - sbuf.tail);
    if(tailToHeadDistance < 0)
        tailToHeadDistance += SIGNAL_BUFFER_SIZE;
    //printf("TailToHeadDistance = %d\n", tailToHeadDistance);

    for(c = 0; c < tailToHeadDistance; c++) {
        // translate into the circular index from the tail towards the head
        i = (sbuf.tail + c) % SIGNAL_BUFFER_SIZE;

        if(sbuf.occupied[i]) {
            // copy the signal to the pointer
            *ps = sbuf.buffer[i];
          
            sbuf.tail = (i+1) % SIGNAL_BUFFER_SIZE;
            // remove from buffer
            memset(sbuf.buffer + i, 0, sizeof(Signal));
            sbuf.occupied[i] = 0;

            // unlock the signal buffer mutex
            pthread_mutex_unlock(&signalBufferMutex);
            return 1;
        }
    }

    // unlock the signal buffer mutex
    pthread_mutex_unlock(&signalBufferMutex);

    // not found
    return 0;
}

PacketSet* findPacketSetForPacket(Packet* pPacket)
{
    uint32_t ts = pPacket->timestamp;
    int i;

    // search backwards through the PacketSetBuffer for a PacketSet with a matching timestamp
    for(int c = 0; c < PACKETSET_BUFFER_SIZE; c++) {
        i = (psetbuf.head - c) % PACKETSET_BUFFER_SIZE;
        if (psetbuf.occupied[i] && psetbuf.buffer[i].timestamp == ts)
            // found it!
            return psetbuf.buffer + i;
    }

    return NULL;
}

PacketSet* createPacketSetForPacket(Packet* pPacket)
{
    // not found, create one
    PacketSet pset;
	memset(&pset, 0, sizeof(PacketSet));
    pset.timestamp = pPacket->timestamp;
    pset.numPackets = pPacket->numPackets;
    pset.packetReceived[pPacket->idxPacket] = 1;
    pset.pPackets[pPacket->idxPacket] = pPacket;

    // add to head of buffer
    return pushPacketSetAtHead(pset); 
}

bool checkReceivedAllPackets(PacketSet* pPacketSet)
{
    // check whether we've received all the packets for this tick
    for(int i = 0; i < pPacketSet->numPackets; i++) {
        if (!pPacketSet->packetReceived[i]) {
            return 0;
        }
    }

    return 1;
}

void processPacketSet(PacketSet* pPacketSet)
{
    Packet* pPacket;

    //printf("\nProcessing Packet Set for timestamp %d:\n", pPacketSet->timestamp);
    //printPacketSet(pPacketSet);

    // clear buffer to store all packet data
    memset(packetDataBuffer, 0, MAX_DATA_SIZE_PER_TICK * sizeof(uint8_t));

    // loop over the packets, copying each into the data buffer
    int bufOffset = 0;
    for(int i = 0; i < pPacketSet->numPackets; i++) {
        // copy over this packet's data into the data buffer
        pPacket = pPacketSet->pPackets[i];
        memcpy(packetDataBuffer + bufOffset, pPacket->rawData, 
                pPacket->rawLength * sizeof(uint8_t));
        
        // advance the offset into the data buffer
        bufOffset += pPacket->rawLength * sizeof(uint8_t);
    }

    // store the number of bytes stored
    packetDataBufferBytes = bufOffset;

    // store the current timestamp
    packetDataTimestamp = pPacketSet->timestamp;

    processData();
}

void printPacketSet(const PacketSet* ppset)
{
    // compute the total bytes across all packets
    int totalBytes = 0;
    for(int i = 0; i < ppset->numPackets; i++)
    {
        if(ppset->packetReceived[i])
            totalBytes += ppset->pPackets[i]->rawLength;    
    }

    printf("PacketSet : [ Timestamp %d, %d bytes received, %d packets (", 
            ppset->timestamp, totalBytes, ppset->numPackets);
    for(int i = 0; i < ppset->numPackets; i++)
    {
        if (ppset->packetReceived[i])
            printf("r");
        else
            printf("_");
    }
    printf(") ]\n");
}

void processData()
{
    uint8_t* pBuf = packetDataBuffer;

    //printf("Processing %d bytes of data\n", packetDataBufferBytes);

    while(pBuf - packetDataBuffer < packetDataBufferBytes) {
        memset(&s, 0, sizeof(Signal));
       
        // set the timestamp for the signal
        s.timestamp = packetDataTimestamp;

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

        // read the data as uint8, we'll typecast later
        uint16_t bytesForData = nElements * getSizeOfDataTypeId(s.dataTypeId);
        STORE_UINT8_ARRAY(pBuf, s.data, bytesForData);

        // add to the signal ring buffer to queue it up for the writer thread
//        Signal* ps;
        pushSignalAtHead(s);
        //printSignal(ps);
    }

}

void logIncompletePacketSet(const PacketSet* ppset)
{
    // this packet set was overwritten in the buffer before all packets received
    fprintf(stderr, "\nWARNING: Incomplete PacketSet for timestamp %d\n\n", ppset->timestamp);
}

void logDroppedSignal(const Signal* ps)
{
    // this packet set was overwritten in the buffer before all packets received
    fprintf(stderr, "\n\n********\nWARNING: Signal buffer overflow: dropping signal\n******\n\n\n");
}
