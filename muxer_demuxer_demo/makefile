COMPILER_PATH = /usr
INCLUDE_PATH = /usr/local/ffmpeg/include
LIB_PATH = /usr/local/ffmpeg/lib

GCC = $(COMPILER_PATH)/bin/gcc
GXX = $(COMPILER_PATH)/bin/g++

LIB := -lavformat -ldl -lavfilter -lavcodec -lavutil -lavdevice

MUXER_OBJS = muxer.o
DEMUXER_OBJS = demuxer.o

%.o: %.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	$(GXX) -std=c++0x -I$(INCLUDE_PATH) -O0 -g3 -Wall -c -fmessage-length=0 -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

%.o: %.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	$(GCC) -std=c99 -I$(INCLUDE_PATH) -O0 -g3 -Wall -c -fmessage-length=0 -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

muxer: $(MUXER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross GCC Linker'
	#$(GCC) -L$(LIB_PATH) -o muxer $(MUXER_OBJS) $(LIB)
	$(GCC) -std=c99 -L$(LIB_PATH) -O0 -g3 -Wall -fmessage-length=0 -o "$@" "$<" $(LIB)
	@echo 'Finished building target: $@'
	@echo ' '

demuxer: $(DEMUXER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross GCC Linker'
	#$(GCC) -L$(LIB_PATH) -o demuxer $(DEMUXER_OBJS) $(LIB)
	$(GCC) -std=c99 -L$(LIB_PATH) -O0 -g3 -Wall -fmessage-length=0 -o "$@" "$<" $(LIB)
	@echo 'Finished building target: $@'
	@echo ' '
	
clean:
	-$(RM) $(MUXER_OBJS) $(DEMUXER_OBJS) muxer demuxer
	-@echo ' '