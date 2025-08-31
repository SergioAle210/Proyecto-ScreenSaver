# Makefile — Screensaver (particles | cube3d | cloth) con SDL2 y OpenMP

# Compilador y flags base
CC       = gcc
CFLAGS   = -Wall -O2 -std=c11
LDFLAGS  =
# Autodetección de SDL2 (pkg-config o sdl2-config). Si falla, usa -lSDL2.
SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null || sdl2-config --cflags 2>/dev/null)
SDL_LIBS   ?= $(shell pkg-config --libs sdl2 2>/dev/null    || sdl2-config --libs 2>/dev/null || printf -- "-lSDL2")

CPPFLAGS  = $(SDL_CFLAGS)
LIBS      = $(SDL_LIBS) -lm

# Fuentes
SRC       = src/main.c src/sim.c src/cube3d.c src/cloth.c
OBJ_SEQ   = $(SRC:.c=.o)
OBJ_PAR   = $(SRC:.c=.op)

# Binarios de salida
TARGET_SEQ = screensaver_seq
TARGET_PAR = screensaver_par

# =========================================================
# Reglas principales
# =========================================================
.PHONY: all clean help run-cloth run-cube run-particles

all: $(TARGET_SEQ) $(TARGET_PAR)

$(TARGET_SEQ): $(OBJ_SEQ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(TARGET_PAR): $(OBJ_PAR)
	$(CC) $(CFLAGS) -fopenmp $(LDFLAGS) -o $@ $^ $(LIBS)

# Objetos paralelos (.op) y secuenciales (.o)
src/%.op: src/%.c
	$(CC) $(CFLAGS) -fopenmp $(CPPFLAGS) -c -o $@ $<

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f src/*.o src/*.op $(TARGET_SEQ) $(TARGET_PAR)

help:
	@echo "Targets:"
	@echo "  all            -> build secuencial y paralelo"
	@echo "  $(TARGET_SEQ)  -> build secuencial (pragmas OpenMP ignorados)"
	@echo "  $(TARGET_PAR)  -> build paralelo (-fopenmp)"
	@echo "  run-cloth      -> ejecuta modo manta (cloth) con parámetros recomendados"
	@echo "  run-cube       -> ejecuta modo cube3d"
	@echo "  run-particles  -> ejecuta modo particles"
	@echo "  clean          -> limpia binarios y objetos"
	@echo ""
	@echo "Variables opcionales:"
	@echo "  THREADS=N      -> pasa --threads N al binario paralelo (si compilaste con OpenMP)."

# =========================================================
# Atajos de ejecución (requieren $(TARGET_PAR) o $(TARGET_SEQ))
# Ajusta parámetros a tu gusto; usa THREADS para OpenMP.
# =========================================================

# Demo potente de la manta de esferas (usa binario paralelo por defecto)
run-cloth: $(TARGET_PAR)
	./$(TARGET_PAR) 0 --mode cloth --grid 180x100 --amp 0.35 --sigma 0.22 --colorSpeed 0.6 --tilt 25 --fov 1.2 $(if $(THREADS),--threads $(THREADS),)

run-cube: $(TARGET_SEQ)
	./$(TARGET_SEQ) 0 --mode cube3d

run-particles: $(TARGET_SEQ)
	./$(TARGET_SEQ) 20000 --mode particles
