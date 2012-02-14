#ifndef WRITER_H_INCLUDED
#define WRITER_H_INCLUDED

#include "signalLogger.h"

typedef struct SignalFileInfo {
	// folder that filename is sitting in
	char filePath[MAX_FILENAME_LENGTH];

	// fully qualified name of file (including filePath)
    char fileName[MAX_FILENAME_LENGTH];

    // trailing name of file (without filePath)
    char fileNameShort[MAX_FILENAME_LENGTH];

    // index file contains a list of .mat file names
    char indexFileName[MAX_FILENAME_LENGTH];

    // file handle for index file
    FILE* indexFile;

} SignalFileInfo; 

void * signalWriterThread(void * dummy);

#endif

