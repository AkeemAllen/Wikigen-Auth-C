CC = gcc
CFLAGS = -Wall -Wextra
BUILDDIR = build

SOURCES := $(wildcard *.c)
OBJECTS := $(patsubst %.c,$(BUILDDIR)/%.o,$(SOURCES))
EXECUTABLE := $(BUILDDIR)/wikigen_auth

all: $(BUILDDIR) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ -lcurl -ljwt -g -gdwarf-4


$(BUILDDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to create the build directory
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -f $(BUILDDIR)/*.o
