CROSS_COMPILE=
CC=gcc -Wno-stringop-overflow
SRC=main.c dma.c mailbox.c pcm.c pwm.c rpihw.c ws2811.c
LIBS=-pthread
CFLAGS=
LINKFLAGS=-lm -lrt

all:
	$(CROSS_COMPILE)$(CC) $(SRC) $(LIBS) $(CFLAGS) $(LINKFLAGS) -o pwm

clean:
	rm pwm
