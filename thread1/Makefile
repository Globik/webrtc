#gcc -o l l.c `pkg-config --cflags --libs glib-2.0` t1.exe
INCLS= -I./plugin
OBJ=plugin.o 
CFLAGS=-g -O2 -D_GNU_SOURCE -std=c99 -Wno-deprecated  -D_GNU_SOURCE `pkg-config --cflags glib-2.0`
p: p
	gcc $(INCLS) $(CFLAGS) -c -fpic ./plugin/plugin.c
b: b
	gcc $(INCLS) $(CFLAGS) -rdynamic -o t1 t1.c   $(OBJ) -pthread -ldl `pkg-config --libs glib-2.0` 