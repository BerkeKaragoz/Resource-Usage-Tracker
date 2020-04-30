CC = gcc


CFLAGS  =  -g -Wall -Wno-format -pthread
LIBS 	= $(wildcard berkelib/*)
DIR = ./build/
TARGET = $(DIR)resource_usage_tracker
SOURCES = $(wildcard *.c)
HEADERS = $(wildcard *.h)
OBJECTS = $(SOURCES:.c=.o)

DEBUGFLAGS   = -O0 -D _DEBUG
RELEASEFLAGS = -O2 -D NDEBUG 


all: clean dir $(TARGET) run clean

dir:
	mkdir -p $(DIR)

$(TARGET): $(OBJECTS) 
		$(CC) $(OBJECTS) $(CFLAGS) $(LIBS) $(RELEASEFLAGS) -o $(TARGET) 

run:
	$(TARGET)

clean:
		$(RM) $(OBJECTS)