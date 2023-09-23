CP=cp
CC=gcc
DEST=/usr/local
CFLAGS=-O2 -g -DNDEBUG
CPPFLAGS=-pedantic -Wall -Wextra
yahboom-hat: main.o i2c.o rgb.o ssd1306_i2c.o minIni/minIni.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
install: yahboom-hat
	$(CP) $^ $(DEST)/bin
.PHONY: clean
clean:
	$(RM) yahboom-hat *.o minIni/*.o
