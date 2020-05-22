CC = gcc


CFLAGS  =  -g -Wall -Wno-format -pthread -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/glib-2.0/gio -I/usr/include/libxml2
LIBS 	= $(wildcard src/berkelib/*) -lglib-2.0 -lxml2
DIR 	= ./build/
ARGS 	=
TARGET 	= $(DIR)resource_usage_tracker
SOURCES = $(wildcard src/*.c)
HEADERS = $(wildcard src/*.h)
OBJECTS = $(SOURCES:.c=.o)

DEBUGFLAGS   = -O0 -D _DEBUG
RELEASEFLAGS = -O2 -D NDEBUG 


all: clean-pre dir $(TARGET) clean-post run 

clean-pre:
	$(RM) $(OBJECTS)

dir:
	mkdir -p $(DIR)

$(TARGET): $(OBJECTS) 
		$(CC) $(OBJECTS) $(CFLAGS) $(LIBS) $(RELEASEFLAGS) -o $(TARGET) 

clean-post:
	$(RM) $(OBJECTS)

run:
	$(TARGET) $(ARGS)