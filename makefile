######################################################
################ MakeFile Demo #######################
######################################################
COMPILER_PATH = /usr
INCLUDE_PATH = /usr/local/ffmpeg/include #ͷ�ļ�·��
LIB_PATH = /usr/local/ffmpeg/lib #���ļ�·��

GCC = $(COMPILER_PATH)/bin/gcc  #c�ļ�������·��
GXX = $(COMPILER_PATH)/bin/g++  #cpp�ļ�������·��

LIB := -lavformat -ldl -lavfilter -lavcodec -lavutil -lavdevice

CLIENT_OBJS = client.o
SERVER_OBJS = server.o

OBJS := $(CLIENT_OBJS) $(SERVER_OBJS)
#
#  all src .o files 
#

# Each subdirectory must supply rules for building sources it contributes
%.o: ./src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	$(GXX) -std=c++0x -I$(INCLUDE_PATH) -O0 -g3 -Wall -c -fmessage-length=0 -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

%.o: ./src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	$(GCC) -std=c99 -I$(INCLUDE_PATH) -O0 -g3 -Wall -c -fmessage-length=0 -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

client: $(CLIENT_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross GCC Linker'
	$(GCC) -std=c99 -L$(LIB_PATH) -O0 -g3 -Wall -fmessage-length=0 -o "$@" "$<" $(LIB)
	@echo 'Finished building target: $@'
	@echo ' '

server: $(SERVER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross GCC Linker'
	$(GCC) -std=c99 -L$(LIB_PATH) -O0 -g3 -Wall -fmessage-length=0 -o "$@" "$<" $(LIB)
	@echo 'Finished building target: $@'
	@echo ' '

all: client server

clean:
	-$(RM) $(OBJS) client server
	-@echo ' '