SRC := writer.c
TARGET := writer
OBJS := $(SRC:.c=.o)


CROSS_COMPILE ?= none

ifeq ($(CROSS_COMPILE),aarch64-none-linux-gnu-)
	CC=aarch64-none-linux-gnu-gcc
else
	CC=gcc
endif


$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET) *.elf *.map
