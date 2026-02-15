CC = gcc
CFLAGS = -Wall -Wextra 
BUILDDIR = build
LIBFLAGS = -lcurl -ljwt
DEBUGFLAGS = -g -gdwarf-4

SOURCES := $(wildcard *.c)
OBJECTS := $(patsubst %.c,$(BUILDDIR)/%.o,$(SOURCES))
EXECUTABLE := $(BUILDDIR)/wikigen_auth

all: $(BUILDDIR) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LIBFLAGS) $(DEBUGFLAGS)

$(BUILDDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to create the build directory
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)
