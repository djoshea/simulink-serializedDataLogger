#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mat.h"

#include "signal.h"
#include "signalLogger.h"

// these match the data type ids defined in signal.h
// note that "char" is actually uint8, but determines how we store it downstream
const char* dataTypeIdNames[] = {"double", "single", "int8", "uint8", 
                                 "int16", "uint16", "int32", "uint32", 
                                 "char"};

const char * getDataTypeIdName(uint8_t dataTypeId)
{
    if(dataTypeId < 0 || dataTypeId > 7)
        diep("Invalid data type Id");

    return dataTypeIdNames[dataTypeId];
}

uint8_t getSizeOfDataTypeId(uint8_t dataTypeId) 
{
    switch (dataTypeId) {
        case DTID_DOUBLE: // double
            return 8;
            
        case DTID_SINGLE: // single
        case DTID_INT32: // int32
        case DTID_UINT32: // uint32
            return 4;

        case DTID_INT8: // int8
        case DTID_UINT8: // uint8
            return 1;

        case DTID_INT16: // int16
        case DTID_UINT16: // uint16
            return 2;

        default:
            diep("Unknown data type Id");
    }

    return 0;
}

int getNumBytesForSignalData(const Signal* psig)
{
    int nBytes = getSizeOfDataTypeId(psig->dataTypeId);
    for(int idim = 0; idim < psig->nDims; idim++) 
        nBytes *= psig->dims[idim];

    return nBytes;
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

    // convert this to 0-indexed
    p.idxPacket--;

    // compute the data length in this packet
    p.rawLength = bytesRead - (pBuf - rawPacket);
    
    // copy the raw data into the data buffer
    STORE_UINT8_ARRAY(pBuf, p.rawData, p.rawLength);

    return p;
}

void printPacket(const Packet* pp)
{
    printf("\tPacket [ Timestamp %d, packet %d of %d, version %d ]\n", 
            pp->timestamp, pp->idxPacket, pp->numPackets, pp->packetVersion);
}

void printSignal(const Signal* psig)
{
    int nElements = 1;

    printf("%d : %10s [", psig->timestamp, psig->name);
    
    if (psig->nDims == 1) {
        printf("%4d x %4d", (int)psig->dims[0], 1);
        nElements = psig->dims[0];
    } else {
        for(int idim = 0; idim < psig->nDims; idim++) {
            nElements *= psig->dims[idim];
            printf("%4d", (int)psig->dims[idim]);
            if(idim < psig->nDims - 1)
                printf(" x ");
        }
    }
    printf(" %6s ] : ", getDataTypeIdName(psig->dataTypeId));

    printf("%s ", (char*)psig->data);
    /*
    for(int i = 0; i < nElements; i++) {
        printf("%d ", (int)psig->data[i]);
    }*/

    printf("\n");
}
