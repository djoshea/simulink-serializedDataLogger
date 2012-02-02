CC=g++
CFLAGS=-Wall -std=gnu++0x 
INCLUDE=-I/usr/local/MATLAB/R2011b/extern/include

serializedDataLogger: serializedDataLogger.cc
	$(CC) -o bin/serializedDataLogger serializedDataLogger.cc $(CFLAGS) $(INCLUDE) 
