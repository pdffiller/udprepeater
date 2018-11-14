TARGET = udprepeater
LIBS = -lresolv -lkunf -L ./lib
CC = gcc
CFLAGS = -g -Wall

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@

install: $(TARGET)
	cp udprepeater /usr/local/bin/udprepeater
	cp configs/udprepeater.conf.example /etc/udprepeater.conf

clean:
	-rm -f *.o
	-rm -f $(TARGET)
