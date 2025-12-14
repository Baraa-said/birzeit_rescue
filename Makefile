CC=gcc
CFLAGS=-Wall -g
LDFLAGS=-lm -pthread

OBJS=main.o config.o genetic.o pool.o astar.o

all: rescue_robot

rescue_robot: $(OBJS)
	$(CC) $(OBJS) -o rescue_robot $(LDFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f rescue_robot *.o
