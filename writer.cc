#include<pthread.h>
#include <unistd.h>
#include <time.h>

// MATLAB mat file library
#include "mat.h"
#include "matrix.h"

#include "signal.h"
#include "buffer.h"
#include "writer.h"

#define MAX_FILENAME_LENGTH 100

typedef struct MATFileInfo {
    char fileName[MAX_FILENAME_LENGTH];
    time_t tStartLog;
    MATFile* pmat;
} MATFileInfo; 

MATFileInfo sigFile;

Signal sig;

void updateSigFile() {
    // get the current date/time
    struct timespec ts;
    struct tm * timeinfo;
    clock_gettime(CLOCK_REALTIME, &ts);
    timeinfo = localtime(&ts.tv_sec);
    
    strftime(sigFile.fileName, MAX_FILENAME_LENGTH, "dlsignal.%Y%m%d.%H%M%S.mat", timeinfo);
    printf("File: %s\n", sigFile.fileName);
    
}

void * signalWriterThread(void * dummy) {

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    //pthread_cleanup_push(closeFiles, NULL);

    while(1) 
    {
        bool foundSignal = 1;

        while (foundSignal) {
            foundSignal = popSignalFromTail(&sig);
            if(foundSignal) {
                printSignal(&sig);
            }
        }

        usleep(1000);
    }

    //pthread_cleanup_pop(0);

    return NULL;
}
