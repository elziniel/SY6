CC=gcc
CFLAGS = -Wall -pedantic -std=c99 -D _POSIX_C_SOURCE=200112L
LDFLAGS=
EXEC=mytar
SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

all : $(EXEC)

$(EXEC) : $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o : %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	rm -rf *.o

cleanall:
	rm -rf *.o
	rm -rf $(EXEC)

mrproper : clean
	rm -rf $(EXEC)