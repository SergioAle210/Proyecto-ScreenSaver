CC = gcc
CFLAGS = -Wall -O2 -std=c11
LIBS = -lSDL2 -lm

SRC = src/main.c src/sim.c src/cube3d.c
OBJ_SEQ = $(SRC:.c=.o)
OBJ_PAR = $(SRC:.c=.op)

TARGET_SEQ = screensaver_seq
TARGET_PAR = screensaver_par

all: $(TARGET_SEQ) $(TARGET_PAR)

$(TARGET_SEQ): $(OBJ_SEQ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(TARGET_PAR): $(OBJ_PAR)
	$(CC) $(CFLAGS) -fopenmp -o $@ $^ $(LIBS)

src/%.op: src/%.c
	$(CC) $(CFLAGS) -fopenmp -c -o $@ $<

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f src/*.o src/*.op $(TARGET_SEQ) $(TARGET_PAR)
