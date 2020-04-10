CC = gcc


CFLAGS  =  -g -Wall -pthread
LIBS 	= 
TARGET = resource_usage_tracker
SOURCES = $(wildcard *.c)
HEADERS = $(wildcard *.h) 
OBJECTS = $(SOURCES:.c=.o)

DEBUGFLAGS   = -O0 -D _DEBUG
RELEASEFLAGS = -O2 -D NDEBUG 


all: $(TARGET)

$(TARGET): $(OBJECTS)
		$(CC) $(OBJECTS) $(CFLAGS) $(LIBS) $(RELEASEFLAGS) -o $(TARGET) 

clean:
		$(RM) $(TARGET) $(OBJECTS)