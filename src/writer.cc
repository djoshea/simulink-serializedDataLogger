// MATLAB mat file library

#include <string.h>
#include <stdio.h>
#include <stdlib.h> /* For EXIT_FAILURE, EXIT_SUCCESS */
#include "mat.h"

#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include "signal.h"
#include "buffer.h"
#include "writer.h"
#include "serializedDataLogger.h"

#define MAX_FILENAME_LENGTH 100

/// PRIVATE DECLARATIONS

int64_t timespec_subtract (const struct timespec *x, const struct timespec *y);
void updateSigFile();
void writeSignalBufferToMATFile();
void writeMxArrayToSigFile(mxArray* mx);
mxArray* createMxArrayForSignals(int nSignalsExpected);
void storeSignalInMxArray(mxArray * mxSignals, const Signal* psig, int index);
mxClassID convertDataTypeIdToMxClassId(uint8_t dataTypeId);
	//
// TYPEDEFS

typedef struct MATFileInfo {
    char fileName[MAX_FILENAME_LENGTH];
    MATFile* pmat;
} MATFileInfo; 

MATFileInfo sigFile;

Signal sig;

// return the difference in time x - y in nanoseconds
#define BILLION 1000000000
int64_t timespec_subtract (const struct timespec *x, const struct timespec *y)
{

    int64_t xnsec = x->tv_nsec * BILLION + x->tv_sec;
    int64_t ynsec = y->tv_nsec * BILLION + y->tv_sec;

    return xnsec - ynsec;
} 
    
void updateSigFile() {
    // get the current date/time
    struct timespec ts;
    struct tm * timeinfo;
    clock_gettime(CLOCK_REALTIME, &ts);
    timeinfo = localtime(&ts.tv_sec);

    int msec = (int)floor((double)ts.tv_nsec / 1000000.0);

    char buffer[MAX_FILENAME_LENGTH];
    strftime(buffer, MAX_FILENAME_LENGTH, "/home/shenoylab/code/serializedDataLogger/data/dlsignal.%Y%m%d.%H%M%S", timeinfo);
    snprintf(sigFile.fileName, MAX_FILENAME_LENGTH, "%s.%03d.mat", buffer, msec); 
}

void * signalWriterThread(void * dummy) {

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    //pthread_cleanup_push(closeFiles, NULL);

    while(1) 
    {
        writeSignalBufferToMATFile();
        usleep(1000*1000);
    }

    //pthread_cleanup_pop(0);

    return NULL;
}

void writeSignalBufferToMATFile() 
{
    bool foundSignal;
    int nSignalsExpected, nSignalsWritten;

    nSignalsExpected = getSignalCountInBuffer();

    // we'll store the signal data in an array of signals with fields:
    // timestamp, name, and data
    mxArray* mxSignals = createMxArrayForSignals(nSignalsExpected);

    // loop until all expected signals are pulled from buffer
    nSignalsWritten = 0;
    for(int i = 0; i < nSignalsExpected; i++)
    {
        // get the signal struct from the buffer
        foundSignal = popSignalFromTail(&sig);

        // check that we pulled it off successfully
        if(!foundSignal) {
            printf("Warning: did not find expected signal in buffer!\n");
            break;
        }

        storeSignalInMxArray(mxSignals, &sig, i); 

        nSignalsWritten++;
        //printSignal(&sig);
    }

    if(nSignalsWritten) {
        updateSigFile();
        printf("%4d signals ==> %s\n", nSignalsWritten, sigFile.fileName);
		writeMxArrayToSigFile(mxSignals);
    }

	mxDestroyArray(mxSignals);
}

void writeMxArrayToSigFile(mxArray* mx)
{
	// open the file
	sigFile.pmat = matOpen(sigFile.fileName, "w");

	if(sigFile.pmat == NULL)
		diep("Error opening MAT file");

	// put variable in file
	int error = matPutVariable(sigFile.pmat, "signals", mx);

	if(error)
		diep("Error putting variable in MAT file");

	// close the file
	matClose(sigFile.pmat);
	sigFile.pmat = NULL;
}

mxArray* createMxArrayForSignals(int nSignalsExpected)
{
    // create a matlab struct array of size N x 1 to hold these signals
    int nfields = 3;
    const char *fieldNames[] = {"timestamp", "name", "data"};
    mwSize ndims = 2;
    mwSize dims[2] = {1, 1};
    dims[0] = nSignalsExpected;

    mxArray* mxSignals = mxCreateStructArray(ndims, dims, nfields, fieldNames);

    return mxSignals;
}

void storeSignalInMxArray(mxArray * mxSignals, const Signal* psig, int index)
{
    // fields to hold the struct field values
    mxArray* mxSignal_timestamp;
    mxArray* mxSignal_name;
    mxArray* mxSignal_data;

    // set the .timestamp field in the mxSignals array
    mxSignal_timestamp = mxCreateNumericMatrix(1, 1, mxUINT32_CLASS, mxREAL);
    *(uint32_t*)mxGetData(mxSignal_timestamp) = psig->timestamp;
    mxSetFieldByNumber(mxSignals, index, 0, mxSignal_timestamp);

    // set the .name field in the mxSignals array
    mxSignal_name = mxCreateString(psig->name);
    mxSetFieldByNumber(mxSignals, index, 1, mxSignal_name);

    // create the data field in the mxSignals array
    
    // get the dimensions
    mwSize ndims = (mwSize)psig->nDims;
    mwSize dims[MAX_SIGNAL_NDIMS];
    for(int i = 0; i < (int)ndims; i++) 
    { 
        dims[i] = (mwSize)(psig->dims[i]);
    }

    // get the data type
    mxClassID cid;

    if(psig->dataTypeId == DTID_CHAR) {
        // special case char array: make 2 x N char array for the string
        mxSignal_data = mxCreateCharArray(ndims, dims);

    } else {
        // numeric type
        cid = convertDataTypeIdToMxClassId(psig->dataTypeId);
        mxSignal_data = mxCreateNumericArray(ndims, dims, cid, mxREAL);
    }

    // stores psig->data in mxSignal_data with appropriate casting
    int nBytes = getNumBytesForSignalData(psig);
    memcpy(mxGetData(mxSignal_data), psig->data, nBytes); 

    mxSetFieldByNumber(mxSignals, index, 2, mxSignal_data);
}

mxClassID convertDataTypeIdToMxClassId(uint8_t dataTypeId)
{
    switch (dataTypeId) {
        case DTID_DOUBLE: // double
            return mxDOUBLE_CLASS;
            
        case DTID_SINGLE: // single
            return mxSINGLE_CLASS;

        case DTID_INT8: // int8
            return mxINT8_CLASS;
                
        case DTID_UINT8: // uint8
            return mxUINT8_CLASS;

        case DTID_INT16: // int16
            return mxINT16_CLASS;

        case DTID_UINT16: // uint16
            return mxUINT16_CLASS;

        case DTID_INT32: // int32
            return mxINT32_CLASS;

        case DTID_UINT32: // uint32
            return mxUINT32_CLASS;

        case DTID_CHAR: // char -> uint8 although use mxCreateString instead 
            return mxUINT8_CLASS; 

        default:
            diep("Unknown data type Id");
    }

    // never reach here
    return mxDOUBLE_CLASS;
}

