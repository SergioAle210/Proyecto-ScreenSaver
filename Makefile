# Makefile — Compila las variantes secuencial y paralela del modo cloth.
# Usa pkg-config o sdl2-config para detectar SDL2. El binario paralelo añade -fopenmp.

CC       = gcc
CFLAGS   = -Wall -Wextra -Wshadow -Wconversion -O3 -march=native -ffast-math -fno-math-errno -std=c11
LDFLAGS  =
SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null || sdl2-config --cflags 2>/dev/null)
SDL_LIBS   ?= $(shell pkg-config --libs sdl2 2>/dev/null    || sdl2-config --libs 2>/dev/null || printf -- "-lSDL2")
CPPFLAGS  = $(SDL_CFLAGS)
LIBS      = $(SDL_LIBS) -lm

# Fuentes compartidas para ambos binarios
COMMON_SRC = src/main.c \
             src/cloth_core.c src/cloth_draw_seq.c

# El binario paralelo agrega el backend OMP
PAR_SRC    = $(COMMON_SRC) src/cloth_draw_omp.c

OBJ_SEQ = $(COMMON_SRC:.c=.o)
OBJ_PAR = $(PAR_SRC:.c=.op)

TARGET_SEQ = screensaver_seq
TARGET_PAR = screensaver_par

.PHONY: all clean help run-cloth

all: $(TARGET_SEQ) $(TARGET_PAR)

$(TARGET_SEQ): $(OBJ_SEQ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(TARGET_PAR): $(OBJ_PAR)
	$(CC) $(CFLAGS) -fopenmp $(LDFLAGS) -o $@ $^ $(LIBS)

# Regla para objetos con OpenMP
src/%.op: src/%.c
	$(CC) $(CFLAGS) -fopenmp $(CPPFLAGS) -c -o $@ $<

# Regla para objetos secuenciales
src/%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f src/*.o src/*.op $(TARGET_SEQ) $(TARGET_PAR)

help:
	@echo "Targets:"
	@echo "  all            -> build secuencial y paralelo (cloth)"
	@echo "  $(TARGET_SEQ)  -> build secuencial"
	@echo "  $(TARGET_PAR)  -> build paralelo (-fopenmp)"
	@echo "  run-cloth      -> ejecutar ejemplo cloth"

run-cloth: $(TARGET_PAR)
	./$(TARGET_PAR) 0 --mode cloth --grid 200x120 --tilt 20 --fov 1.4 --fpscap 0 --novsync
