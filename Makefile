CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lm -pthread
TARGET = rescue_robot

SOURCES = main.c config.c genetic.c pool.c
HEADERS = types.h config.h genetic.h pool.h
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) -c $< -o $@ $(CFLAGS)

debug: CFLAGS += -DDEBUG
debug: $(TARGET)

clean:
	rm -f $(TARGET) $(OBJECTS) *.o

run: $(TARGET)
	./$(TARGET) config.txt

run-default: $(TARGET)
	./$(TARGET)

.PHONY: all clean run run-default debug
