CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lrt

# Default target
all: diskmanager foosh

# Build diskmanager
diskmanager: diskmanager.o
	$(CC) $(CFLAGS) -o diskmanager diskmanager.o $(LDFLAGS)

diskmanager.o: diskmanager.c vdisk.h
	$(CC) $(CFLAGS) -c diskmanager.c

# Build foosh
foosh: foosh.o
	$(CC) $(CFLAGS) -o foosh foosh.o $(LDFLAGS)

foosh.o: foosh.c diskutils.c vdisk.h
	$(CC) $(CFLAGS) -c foosh.c

# Clean build artifacts
clean:
	rm -f *.o diskmanager foosh

.PHONY: all clean
