CC = g++

CFLAGS=-c -Wall
SRCDIR=./src/
BINDIR=./bin/

SOURCES=$(wildcard $(SRCDIR)*.cpp)
OBJECTS=$(SOURCES:$(SRCDIR)%.cpp=$(BINDIR)%.o)
EXECUTABLE=$(BINDIR)program
BUILDMESSAGE = @echo "\nCleaned and compiled successfully\n"
RUNMESSAGE = @echo "\nNow running filesystem. Make sure to use "format" command to properly initialize the FAT filesystem\n"

run: $(EXECUTABLE)
	$(RUNMESSAGE)
	$(EXECUTABLE)

all: clean $(EXECUTABLE)
	$(BUILDMESSAGE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

$(BINDIR)%.o: $(SRCDIR)%.cpp
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS)
	rm $(EXECUTABLE)

.PHONY: clean