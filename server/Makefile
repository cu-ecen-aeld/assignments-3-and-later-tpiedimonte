SRC := aesdsocket.c
TARGET := aesdsocket
OBJS := $(SRC:.c=.o)


CROSS_COMPILE ?= none
#CC=aarch64-none-linux-gnu-gcc
CC=gcc

#ifeq ($(CROSS_COMPILE),aarch64-none-linux-gnu-)
#	CC=aarch64-none-linux-gnu-gcc
#else
#	CC=gcc
#endif

LDFLAGS = -pthread -lrt

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET) *.elf *.map
