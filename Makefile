# Makefile — C (C11) + SDL2 + OpenMP, Linux y Windows (Git Bash/MSYS2)
# ---------------------------------------------------------------
# Uso:
#   make                # compila seq y omp
#   make seq            # solo secuencial
#   make omp            # solo OpenMP
#   make run_seq        # ejecuta secuencial con args por defecto
#   make run_omp        # ejecuta paralelo con args por defecto
#   SDLPREFIX=... make  # fuerza ruta SDL2 en Windows si no usas MSYS2
#
# En Windows (Git Bash):
#   - Si tienes MSYS2: se detecta MINGW64/UCRT64/CLANG64 y usa /mingw64, /ucrt64 o /clang64.
#   - Si usas SDL2 "standalone" (zip): define SDLPREFIX a la carpeta x86_64-w64-mingw32.

UNAME_S := $(shell uname -s 2>/dev/null)
MSYS_SYS := $(MSYSTEM)

CC := gcc
CFLAGS_BASE := -O3 -std=c11 -Wall -Wextra -Wno-unknown-pragmas
LDFLAGS :=

SRC := src/main.c src/sim.c
OBJ_DIR_SEQ := build/obj_seq
OBJ_DIR_OMP := build/obj_omp
BIN_DIR := build
SEQ_BIN := $(BIN_DIR)/screensaver4d_seq
OMP_BIN := $(BIN_DIR)/screensaver4d_omp

# Extensión .exe en Windows
ifeq ($(findstring MINGW,$(UNAME_S))$(findstring MINGW,$(MSYS_SYS)),MINGW)
	EXEEXT := .exe
else
	EXEEXT :=
endif
SEQ_BIN := $(SEQ_BIN)$(EXEEXT)
OMP_BIN := $(OMP_BIN)$(EXEEXT)

OBJS_SEQ := $(patsubst src/%.c,$(OBJ_DIR_SEQ)/%.o,$(SRC))
OBJS_OMP := $(patsubst src/%.c,$(OBJ_DIR_OMP)/%.o,$(SRC))

# -------------------------
# Detección SDL2 por plataforma
# -------------------------
# Linux: usa pkg-config
ifeq ($(UNAME_S),Linux)
	SDLCFLAGS := $(shell pkg-config --cflags sdl2)
	SDLLIBS   := $(shell pkg-config --libs sdl2)
else
	# Windows (MSYS2/Git Bash)
	# 1) Si el usuario define SDLPREFIX, usamos esa ruta (zip standalone).
	#    Ej: SDLPREFIX=C:/libs/SDL2/x86_64-w64-mingw32
	ifdef SDLPREFIX
		SDLCFLAGS := -I$(SDLPREFIX)/include -Dmain=SDL_main
		SDLLIBS   := -L$(SDLPREFIX)/lib -lmingw32 -lSDL2main -lSDL2
		SDL2_DLL  := $(SDLPREFIX)/bin/SDL2.dll
	else
		# 2) Si no define SDLPREFIX, intentamos MSYS2 según MSYSTEM
		#    MINGW64 -> /mingw64, UCRT64 -> /ucrt64, CLANG64 -> /clang64
		ifneq (,$(findstring UCRT64,$(MSYS_SYS)))
			_SDLPFX := /ucrt64
		else ifneq (,$(findstring CLANG64,$(MSYS_SYS)))
			_SDLPFX := /clang64
		else
			_SDLPFX := /mingw64
		endif
		SDLCFLAGS := -I$(_SDLPFX)/include -Dmain=SDL_main
		SDLLIBS   := -L$(_SDLPFX)/lib -lmingw32 -lSDL2main -lSDL2
		SDL2_DLL  := $(_SDLPFX)/bin/SDL2.dll
	endif
endif

# -------------------------
# Targets
# -------------------------
all: prep seq omp

prep:
	mkdir -p $(OBJ_DIR_SEQ) $(OBJ_DIR_OMP) $(BIN_DIR)

# --- Secuencial (sin OpenMP) ---
$(OBJ_DIR_SEQ)/%.o: src/%.c
	$(CC) $(CFLAGS_BASE) $(SDLCFLAGS) -c $< -o $@

seq: $(OBJS_SEQ)
	$(CC) $(CFLAGS_BASE) $(SDLCFLAGS) $(OBJS_SEQ) $(SDLLIBS) -o $(SEQ_BIN)
	@# Copiar SDL2.dll junto a los binarios si existe (Windows)
	@if [ -f "$(SDL2_DLL)" ]; then cp -f "$(SDL2_DLL)" "$(BIN_DIR)/" || true; fi

# --- Paralelo (con OpenMP) ---
$(OBJ_DIR_OMP)/%.o: src/%.c
	$(CC) $(CFLAGS_BASE) -fopenmp $(SDLCFLAGS) -c $< -o $@

omp: $(OBJS_OMP)
	$(CC) $(CFLAGS_BASE) -fopenmp $(SDLCFLAGS) $(OBJS_OMP) $(SDLLIBS) -o $(OMP_BIN)
	@# Copiar SDL2.dll junto a los binarios si existe (Windows)
	@if [ -f "$(SDL2_DLL)" ]; then cp -f "$(SDL2_DLL)" "$(BIN_DIR)/" || true; fi

# -------------------------
# Atajos de ejecución
# -------------------------
RUN_N      ?= 2000
RUN_W      ?= 1280
RUN_H      ?= 720
RUN_SEED   ?= 42
RUN_FPSCAP ?= 0
RUN_THREADS?= 8

run_seq: seq
	$(SEQ_BIN) $(RUN_N) $(RUN_W) $(RUN_H) --seed $(RUN_SEED) --fpscap $(RUN_FPSCAP)

run_omp: omp
	$(OMP_BIN) $(RUN_N) $(RUN_W) $(RUN_H) --seed $(RUN_SEED) --fpscap $(RUN_FPSCAP) --threads $(RUN_THREADS)

# -------------------------
# Limpieza
# -------------------------
clean:
	rm -rf $(BIN_DIR)

.PHONY: all prep seq omp run_seq run_omp clean
