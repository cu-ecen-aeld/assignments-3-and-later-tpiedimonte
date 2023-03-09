TARGET = aesdsocket
SRC := aesdsocket.c
OBJS := $(SRC:.c=.o) 

ifeq ($(CC),)
	CC = $(CROSS_COMPILE)gcc
endif
ifeq ($(CFLAGS),)
	CFLAGS += -g -Wall -Wextra
endif
ifeq ($(LDFLAGS),)
	LDFLAGS = -pthread -lrt
endif

.PHONY : all, native, arm64, clean

all : $(TARGET)
	@echo "RUNNING ALL"

$(TARGET) : $(OBJS)
	@echo "BUILDING $(TARGET)..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(OBJS) $(LDFLAGS)
	@echo "BUILDING $(TARGET) DONE"

native:
	@echo "RUNNING NATIVE $(TARGET) BUILD..."
	gcc $(CFLAGS) $(INCLUDES) $(SRC) -o $(TARGET) $(LDFLAGS)
	@echo "BUILDING NATIVE $(TARGET) DONE"

arm64:
	@echo "RUNNING ARM64 $(TARGET) BUILD..."
	aarch64-none-linux-gnu-gcc $(CFLAGS) $(INCLUDES) $(SRC) -o $(TARGET) $(LDFLAGS)
	@echo "BUILDING ARM64 $(TARGET) DONE"

clean:
	@echo "CLEANING..."
	-rm -f *.o $(TARGET) *.elf *.map
	@echo "CLEANING DONE"
