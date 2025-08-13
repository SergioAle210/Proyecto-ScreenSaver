# Makefile (Linux-only)
CC=gcc
CFLAGS=-O2 -Wall -Wextra -pedantic
OMPFLAGS=-fopenmp

all: simulacion_secuencial simulacion_paralela

simulacion_secuencial: simulacion_secuencial.c
	$(CC) $(CFLAGS) $< -o $@

simulacion_paralela: simulacion_paralela.c
	$(CC) $(CFLAGS) $(OMPFLAGS) $< -o $@

clean:
	rm -f simulacion_secuencial simulacion_paralela
