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

#include <unordered_map>
#include <string>
#include <inttypes.h>
#include<pthread.h>
#include <signal.h>

// local includes
#include "signal.h"
#include "buffer.h"
#include "writer.h"

#define DEBUG_ON 1

#define PORT 25000 
#define UNKNOWN_PACKET_COUNT -1

///////////// GLOBALS /////////////
int sock;
pthread_t writerThread;

void diep(const char *s)
{
    perror(s);
    exit(1);
}

void finish_main(int sig)
{
    printf("Finishing Main\n");
    pthread_cancel(writerThread);
    pthread_join(writerThread, NULL);
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

    clearBuffers();

    // Setup signal buffer mutex so that multiple locking is okay

    // Start File Writer Thread
    int rc = pthread_create(&writerThread, NULL, signalWriterThread, NULL); 
    if (rc) {
        printf("ERROR!  Return code from pthread_create() is %d\n", rc);
        exit(-1);
    }

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
        p = parsePacket(rawPacket, bytesRead);

        // add Packet to the head of the packet buffer
        Packet* pPacket;
        pPacket = pushPacketAtHead(p);
        //printPacket(pPacket);

        PacketSet* pPacketSet;
        pPacketSet = findPacketSetForPacket(pPacket);

        if(pPacketSet == NULL)
        {
            pPacketSet = createPacketSetForPacket(pPacket);
        } else {
            // store a pointer to this packet inside the packet set
            pPacketSet->pPackets[pPacket->idxPacket] = pPacket;
            pPacketSet->packetReceived[pPacket->idxPacket] = 1;
        }

        // have we received the full group of packets yet?
        receivedAll = checkReceivedAllPackets(pPacketSet);
        if (receivedAll) {

            // look at the multi-packet data in this set
            // turn it into signals on the SignalBuffer, and remove the 
            // packet set and packets from their buffers
            processPacketSet(pPacketSet);

            removePacketSetFromBuffer(pPacketSet);

        }
    }

    pthread_cancel(writerThread);
    pthread_join(writerThread, NULL);
    close(sock);

    return 0;

}




