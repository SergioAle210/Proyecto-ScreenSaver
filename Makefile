# Makefile — Screensaver (particles | cube3d | cloth) con SDL2 y OpenMP

CC       = gcc
CFLAGS   = -Wall -O3 -march=native -ffast-math -fno-math-errno -std=c11
LDFLAGS  =
SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null || sdl2-config --cflags 2>/dev/null)
SDL_LIBS   ?= $(shell pkg-config --libs sdl2 2>/dev/null    || sdl2-config --libs 2>/dev/null || printf -- "-lSDL2")

CPPFLAGS  = $(SDL_CFLAGS)
LIBS      = $(SDL_LIBS) -lm

# ---------------- Fuentes ----------------
# Comunes a ambos binarios (NO incluyen el backend OMP)
COMMON_SRC = src/main.c src/sim.c src/cloth_core.c src/cloth_draw_seq.c

# El binario paralelo añade el backend OMP
PAR_SRC    = $(COMMON_SRC) src/cloth_draw_omp.c

OBJ_SEQ    = $(COMMON_SRC:.c=.o)
OBJ_PAR    = $(PAR_SRC:.c=.op)

TARGET_SEQ = screensaver_seq
TARGET_PAR = screensaver_par

# ---------------- Reglas ----------------
.PHONY: all clean help run-cloth run-cube run-particles

all: $(TARGET_SEQ) $(TARGET_PAR)

$(TARGET_SEQ): $(OBJ_SEQ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(TARGET_PAR): $(OBJ_PAR)
	$(CC) $(CFLAGS) -fopenmp $(LDFLAGS) -o $@ $^ $(LIBS)

# Compilación de objetos
src/%.op: src/%.c
	$(CC) $(CFLAGS) -fopenmp $(CPPFLAGS) -c -o $@ $<

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f src/*.o src/*.op $(TARGET_SEQ) $(TARGET_PAR)

help:
	@echo "Targets:"
	@echo "  all            -> build secuencial y paralelo"
	@echo "  $(TARGET_SEQ)  -> build secuencial (sin OpenMP)"
	@echo "  $(TARGET_PAR)  -> build paralelo (-fopenmp)"
	@echo "  run-cloth      -> ejecuta modo cloth (paralelo)"
	@echo "  run-cube       -> ejecuta modo cube3d"
	@echo "  run-particles  -> ejecuta modo particles"
	@echo ""
	@echo "Variables:"
	@echo "  THREADS=N      -> pasa --threads N al binario paralelo."

# Atajos de ejecución
run-cloth: $(TARGET_PAR)
	./$(TARGET_PAR) 0 --mode cloth --grid 180x100 --amp 0.35 --sigma 0.22 --colorSpeed 0.6 --tilt 25 --fov 1.2 $(if $(THREADS),--threads $(THREADS),)

run-cube: $(TARGET_SEQ)
	./$(TARGET_SEQ) 0 --mode cube3d

run-particles: $(TARGET_SEQ)
	./$(TARGET_SEQ) 20000 --mode particles
