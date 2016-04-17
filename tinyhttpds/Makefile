CFLAGS += -Wall -O2 -W -I../lib -DHTTPS_SUPPORT
LDFLAGS += -lssl -lcrypto
#OBJS= sockp.c socket.c tinydns.c
OBJS= main.o ../lib/socket.o
LIBS= -lresolv

all: built-in.o
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o tinyhttpd $(LDFLAGS)
	@if [ ! -f /www/ssl/server.key ];then			\
		sudo mkdir -p /www/;				\
		sudo chmod 1777 /www/;				\
		mkdir -p /www/cgi-bin;				\
		install -m 644 ../www/index.html /www/index.html; \
		install -m 755 ../www/cgi-bin/cgi /www/cgi-bin/cgi; \
		install -m 644 ../www/cgi-bin/index.cgi /www/cgi-bin/index.cgi; \
		mkdir /www/ssl;					\
		install -m 644 ssl/server.cert /www/ssl/;	\
		install -m 644 ssl/server.key /www/ssl/;	\
	fi

built-in.o: $(OBJS)
	$(LD) -r -o $@ $(OBJS)
	
clean:
	rm -f main *.o tinyhttpd  $(OBJS)
