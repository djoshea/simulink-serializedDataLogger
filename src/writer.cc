// MATLAB mat file library

#include <string.h>
#include <stdio.h>
#include <stdlib.h> /* For EXIT_FAILURE, EXIT_SUCCESS */
#include "mat.h"

#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>

#include "signal.h"
#include "buffer.h"
#include "writer.h"
#include "signalLogger.h"

#define WRITE_INTERVAL_USEC 100*1000 
#define PATH_SEPARATOR "/"

extern char dataRoot[MAX_FILENAME_LENGTH];

/// PRIVATE DECLARATIONS

void signalWriterThreadCleanup(void* dummy);
void updateSignalFileInfo(SignalFileInfo *);
void writeSignalBufferToMATFile();
void writeMxArrayToSigFile(mxArray* mx, const SignalFileInfo *);
mxArray* createMxArrayForSignals(int nSignalsExpected);
void storeSignalInMxArray(mxArray * mxSignals, const Signal* psig, int index);
mxClassID convertDataTypeIdToMxClassId(uint8_t dataTypeId);
void logToSignalIndexFile(const SignalFileInfo* pSigFileInfo, const char* str);

SignalFileInfo sigFileInfo;
Signal sig;

void * signalWriterThread(void * dummy)
{

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    pthread_cleanup_push(signalWriterThreadCleanup, NULL);

    while(1) 
    {
        writeSignalBufferToMATFile();
        usleep(WRITE_INTERVAL_USEC);
    }

    pthread_cleanup_pop(0);

    return NULL;
}

void signalWriterThreadCleanup(void* dummy) {
    printf("SignalWriteThread: Cleaning up\n");
    if(sigFileInfo.indexFile != NULL)
        fclose(sigFileInfo.indexFile);
}

void updateSignalFileInfo(SignalFileInfo* pSignalFile)
{
    // get the current date/time
    struct timespec ts;
    struct tm * timeinfo;
    clock_gettime(CLOCK_REALTIME, &ts);
    timeinfo = localtime(&ts.tv_sec);

	// and msec within the current second (to ensure the names never collide)
    int msec = (int)floor((double)ts.tv_nsec / 1000000.0);

	// append the date as a folder onto dataRoot 
    char parentDirBuffer[MAX_FILENAME_LENGTH];
    strftime(parentDirBuffer, MAX_FILENAME_LENGTH, "%Y%m%d", timeinfo);

    // start with the dataRoot
    char pathBuffer[MAX_FILENAME_LENGTH];
    snprintf(pathBuffer, MAX_FILENAME_LENGTH, 
            "%s/%s", dataRoot, parentDirBuffer);
    
    // check that this data directory exists if it's changed from last time
    if(strncmp(pathBuffer, pSignalFile->filePath, MAX_FILENAME_LENGTH) != 0)
    {
        // path has changed, need to check that this path exists, and/or create it
        if (access( pathBuffer, R_OK | W_OK ) == -1) 
        {
            // this new path doesn't exist or we can't access it --> mkdir it
            int failed = mkdir(pathBuffer, S_IRWXU);
            if(failed)
                diep("Error creating signal data directory");
        }

        // update the filePath so we don't try to create it again
        strncpy(pSignalFile->filePath, pathBuffer, MAX_FILENAME_LENGTH);
        printf("Signal data dir : %s\n", pSignalFile->filePath);

    }

    // now check the index file to make sure it exists
    char indexFileBuffer[MAX_FILENAME_LENGTH];
    snprintf(indexFileBuffer, MAX_FILENAME_LENGTH, 
            "%s/index.txt", pSignalFile->filePath);

    // check whether the index file is the same as last time
    if(strncmp(indexFileBuffer, pSignalFile->indexFileName, MAX_FILENAME_LENGTH) != 0)
    {
        // it's changed from last time
        strncpy(pSignalFile->indexFileName, indexFileBuffer, MAX_FILENAME_LENGTH);
        printf("Signal index file : %s\n", pSignalFile->indexFileName);

        // it's not, reopen it for appending
        pSignalFile->indexFile = fopen(pSignalFile->indexFileName, "a");

        if(pSignalFile->indexFile == NULL)
        {
            diep("Error opening index file.");
        }
    }
    
    // create a unique file name based on date.time.msec
    char fileTimeBuffer[MAX_FILENAME_LENGTH];
    strftime(fileTimeBuffer, MAX_FILENAME_LENGTH, "%Y%m%d.%H%M%S", timeinfo);

    snprintf(pSignalFile->fileNameShort, MAX_FILENAME_LENGTH,
            "signal.%s.%03d.mat", fileTimeBuffer, msec);

    snprintf(pSignalFile->fileName, MAX_FILENAME_LENGTH, 
            "%s/%s", pSignalFile->filePath, pSignalFile->fileNameShort); 
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

    // write them to disk as a mat file!
    if(nSignalsWritten) {
        updateSignalFileInfo(&sigFileInfo);
        printf("%4d signals ==> %s\n", nSignalsWritten, sigFileInfo.fileName);
		writeMxArrayToSigFile(mxSignals, &sigFileInfo);

        logToSignalIndexFile(&sigFileInfo, sigFileInfo.fileNameShort);
    }

	mxDestroyArray(mxSignals);
}

void logToSignalIndexFile(const SignalFileInfo* pSigFileInfo, const char* str) {
    // write the string to the index file
    if(pSigFileInfo->indexFile == NULL)
        diep("Index file not opened\n");

    fprintf(pSigFileInfo->indexFile, "%s\n", str); 
    fflush(pSigFileInfo->indexFile);
}


void writeMxArrayToSigFile(mxArray* mx, const SignalFileInfo *pSigFileInfo)
{
	// open the file
	MATFile* pmat = matOpen(pSigFileInfo->fileName, "w");

	if(pmat == NULL)
		diep("Error opening MAT file");

	// put variable in file
	int error = matPutVariable(pmat, "signals", mx);

	if(error)
		diep("Error putting variable in MAT file");

	// close the file
	matClose(pmat);
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

