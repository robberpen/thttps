CFLAGS += -Wall -O2 -W -I../lib
#OBJS= sockp.c socket.c tinydns.c
OBJS= main.o ../lib/socket.o
LIBS= -lresolv

all: built-in.o
	$(CC) $(CFLAG) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o tinyhttpd2

built-in.o: $(OBJS)
	$(LD) -r -o $@ $(OBJS)
	
clean:
	rm -f main *.o tinyhttpd2
