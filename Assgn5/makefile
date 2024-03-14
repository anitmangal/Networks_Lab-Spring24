CC = gcc
CFLAGS = -Wall

LIBNAME = libmsocket.a

LIBS = -L. -lmsocket

all: $(LIBNAME) initmsocket user1 user2

apps: user1 user2

$(LIBNAME): msocket.o
	ar rcs $(LIBNAME) msocket.o

msocket.o: msocket.c msocket.h
	$(CC) $(CFLAGS) -c msocket.c

initmsocket: initmsocket.o $(LIBNAME)
	$(CC) $(CFLAGS) -o initmsocket initmsocket.o $(LIBS)

initmsocket.o: initmsocket.c msocket.h
	$(CC) $(CFLAGS) -c initmsocket.c

user1: user1.o $(LIBNAME)
	$(CC) $(CFLAGS) -o user1 user1.o $(LIBS)

user1.o: user1.c msocket.h
	$(CC) $(CFLAGS) -c user1.c

user2: user2.o $(LIBNAME)
	$(CC) $(CFLAGS) -o user2 user2.o $(LIBS)

user2.o: user2.c msocket.h
	$(CC) $(CFLAGS) -c user2.c

clean:
	rm -f *.o $(LIBNAME) initmsocket user1 user2
