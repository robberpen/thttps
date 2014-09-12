CFLAGS += -Wall -O2 -W -I../lib
#OBJS= sockp.c socket.c tinydns.c
OBJS= main.o ../lib/socket.o
LIBS= -lresolv

all: built-in.o
	$(CC) $(CFLAG) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o tinyhttpd2
	@if [ ! -x /www/cgi-bin/cgi ];then	\
		sudo mkdir -p /www/;		\
		sudo chmod 1777 /www/;		\
		mkdir /www/cgi-bin;		\
		install -m 644 ../www/index.html /www/index.html; \
		install -m 755 ../www/cgi-bin/cgi /www/cgi-bin/cgi; \
		install -m 644 ../www/cgi-bin/index.cgi /www/cgi-bin/index.cgi; \
	fi

built-in.o: $(OBJS)
	$(LD) -r -o $@ $(OBJS)
	
clean:
	rm -f main *.o tinyhttpd2
