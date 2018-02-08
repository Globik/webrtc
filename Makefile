#gcc -o l l.c `pkg-config --cflags --libs glib-2.0`
INCLS=-I/home/globik/janus-gateway -I/home/globik/janus-gateway/transports -I/home/globik/janus-gateway/plugins
OBJ=/home/globik/janus-gateway/*.o /home/globik/janus-gateway/plugins/janus-plugin.o 
DEFS=-DINET -DINET6 -DDEBUG_SCTP -DHAVE_LIBCURL -DHAVE_SCTP -DEBUG -D_GNU_SOURCE
CFLAGS=-g -O2 -std=c99  $(DEFS) -fPIC -Wno-deprecated `pkg-config --cflags openssl nice`
SUKA=-pthread -lgobject-2.0 -lusrsctp -lcurl -lsrtp2 -ldl -ljansson
b: b.c
	gcc $(INCLS) $(CFLAGS) -o s s.c   $(OBJ) $(SUKA) `pkg-config --libs glib-2.0 nice openssl` 

s: s.c
	gcc $(INCLS) $(CFLAGS) -o s s.c   $(OBJ) $(SUKA) `pkg-config --libs glib-2.0 nice openssl` 