
CC = gcc
CFLAGS = -Wall -O2 -std=c99

SIM80 = sim80
ASM80 = asm80

all: $(SIM80) $(ASM80)

$(SIM80): sim80.c
	$(CC) $(CFLAGS) -o $(SIM80) sim80.c

$(ASM80): asm80.c
	$(CC) $(CFLAGS) -o $(ASM80) asm80.c

install:
	install -m 755 asm80 /usr/local/bin
	install -m 755 sim80 /usr/local/bin

clean:
	rm -f $(SIM80) $(ASM80) *.o