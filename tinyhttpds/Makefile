CFLAGS += -Wall -O2 -W -I../lib -DHTTPS_SUPPORT
LDFLAGS += -lssl
#OBJS= sockp.c socket.c tinydns.c
OBJS= main.o ../lib/socket.o
LIBS= -lresolv

all: built-in.o
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o tinyhttpd $(LDFLAGS)

built-in.o: $(OBJS)
	$(LD) -r -o $@ $(OBJS)
	
clean:
	rm -f main *.o tinyhttpd
