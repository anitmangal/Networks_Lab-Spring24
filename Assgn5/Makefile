CC = gcc
CFLAGS = -Wall -Werror

LIBNAME = libmsocket.a

all: $(LIBNAME)

$(LIBNAME): msocket.o
	ar rcs $(LIBNAME) msocket.o

msocket.o: msocket.c msocket.h
	$(CC) $(CFLAGS) -c msocket.c

clean:
	rm -f *.o $(LIBNAME)
