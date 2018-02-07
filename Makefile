#gcc -o l l.c `pkg-config --cflags --libs glib-2.0`
INCLS=-I./ -I./transports -I./plugins
OBJ=*.o ./plugins/janus-plugin.o 
CFLAGS=-g -O2   -DLOG=3 -DHAVE_LIBCURL -DHAVE_SCTP -DEBUG -D_GNU_SOURCE -std=c99  -DINET -DINET6 -DDEBUG_SCTP -fPIC -Wno-deprecated `pkg-config --cflags openssl nice`
b: b.c
	gcc $(INCLS) $(CFLAGS) -o b b.c   $(OBJ) -pthread -lgobject-2.0 -lusrsctp -lcurl -lsrtp2 -ldl -ljansson `pkg-config --libs glib-2.0 nice openssl`