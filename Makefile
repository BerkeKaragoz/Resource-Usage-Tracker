CC = gcc


CFLAGS  =  -g -Wall -pthread
LIBS 	= 
DIR = ./build/
TARGET = $(DIR)resource_usage_tracker
SOURCES = $(wildcard *.c)
HEADERS = $(wildcard *.h) 
OBJECTS = $(SOURCES:.c=.o)

DEBUGFLAGS   = -O0 -D _DEBUG
RELEASEFLAGS = -O2 -D NDEBUG 


all: dir $(TARGET) clean

dir:
	mkdir -p $(DIR)

$(TARGET): $(OBJECTS)
		$(CC) $(OBJECTS) $(CFLAGS) $(LIBS) $(RELEASEFLAGS) -o $(TARGET) 

clean:
		$(RM) $(OBJECTS)