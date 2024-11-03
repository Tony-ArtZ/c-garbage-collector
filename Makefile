CC = gcc
CFLAGS = -Wall -g
TARGET = run
SRCS = main.c gc.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET) execute

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

execute: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean execute