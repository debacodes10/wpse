CC = gcc
CFLAGS = -Wall -Wextra -Isrc -O2
SRCS = src/main.c src/network.c src/protocol.c src/cache.c src/pager.c src/btree.c
OBJS = $(SRCS:.c=.o)
TARGET = engine_server

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)
