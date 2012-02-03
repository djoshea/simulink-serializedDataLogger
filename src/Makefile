# Author: Dan O'Shea dan@djoshea.com 2012

# to get the options in this file, run in Matlab:
# mex('-v', '-f', [matlabroot '/bin/matopts.sh'], '-lrt', 'serializedDataLogger.cc', 'writer.cc', 'buffer.cc', 'signal.cc')

# update this for newer matlab versions
MATLAB_ROOT=/usr/local/MATLAB/R2011b

# compiler options
CXX=g++
CXXFLAGS=-Wall
CXXFLAGS_MEX=-I$(MATLAB_ROOT)/extern/include -I$(MATLAB_ROOT)/simulink/include -DMATLAB_MEX_FILE -ansi -D_GNU_SOURCE -I$(MATLAB_ROOT)/extern/include/cpp -I$(MATLAB_ROOT)/extern/include -DGLNXA64 -DGCC  -DMX_COMPAT_32 -O -DNDEBUG  

# linker options
LD=g++
LDFLAGS_MEX=-lrt -Wl,-rpath-link,$(MATLAB_ROOT)/bin/glnxa64 -L$(MATLAB_ROOT)/bin/glnxa64 -lmat -lmx -lm

# where to locate output files
SRC_DIR=.
BUILD_DIR=../build
BIN_DIR=..

# lists of h, cc, and o files without paths
H_NAMES=serializedDataLogger.h buffer.h signal.h writer.h
CC_NAMES=serializedDataLogger.cc buffer.cc signal.cc writer.cc
O_NAMES=serializedDataLogger.o buffer.o signal.o writer.o

# add file paths pointing to appropriate directories
H_FILES=$(patsubst %,$(SRC_DIR)/%,$(H_NAMES))
CC_FILES=$(patsubst %,$(SRC_DIR)/%,$(CC_NAMES))
O_FILES=$(patsubst %,$(BUILD_DIR)/%,$(O_NAMES))

# final output
EXECUTABLE=$(BIN_DIR)/serializedDataLogger

############ TARGETS #####################
all: serializedDataLogger

# compile .o for each .c, depends also on all .h files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc $(H_FILES)
	@echo "==> Compiling $<:"
	@$(CXX) -c -o $@ $< $(CXXFLAGS) $(CXXFLAGS_MEX) 

# link *.o into executable
serializedDataLogger: $(O_FILES)
	@echo "==> Linking $<:"
	@$(LD) -O -o $(EXECUTABLE) $(O_FILES) $(LDFLAGS_MEX)
	@echo "==> Built $(EXECUTABLE) successfully!"

# clean and delete executable
clobber: clean
	rm -f $(EXECUTABLE)

# delete .o files and garbage
clean: 
	rm -f $(O_FILES) *~ core 
