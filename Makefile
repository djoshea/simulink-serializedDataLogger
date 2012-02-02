CC=g++
CFLAGS=-pthread -lrt -Wall -std=gnu++0x 
INCLUDE=-I/usr/local/MATLAB/R2011b/extern/include

H_FILES=serializedDataLogger.h buffer.h signal.h writer.h
CC_FILES=serializedDataLogger.cc buffer.cc signal.cc writer.cc

serializedDataLogger: $(CC_FILES) $(H_FILES) 
	$(CC) -o bin/serializedDataLogger $(CC_FILES) $(CFLAGS) $(INCLUDE) 
