# Makefile — C (C11) + SDL2 + OpenMP, Linux y Windows (Git Bash/MSYS2)

UNAME_S  := $(shell uname -s 2>/dev/null)
MSYS_SYS := $(MSYSTEM)
PKGCONF  := $(shell command -v pkg-config 2>/dev/null)

CC := gcc
CFLAGS_BASE := -O3 -std=c11 -Wall -Wextra -Wno-unknown-pragmas
LDFLAGS :=

SRC := src/main.c src/sim.c src/tesseract.c
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
# SDL2 por plataforma
# -------------------------
ifeq ($(UNAME_S),Linux)
  ifeq ($(PKGCONF),)
    SDLCFLAGS := -I/usr/include/SDL2 -D_REENTRANT
    SDLLIBS   := -lSDL2
  else
    SDLCFLAGS := $(shell pkg-config --cflags sdl2)
    SDLLIBS   := $(shell pkg-config --libs sdl2)
  endif
  LDLIBS_EXTRA := -lm
else
  # Windows (Git Bash/MSYS2)
  ifdef SDLPREFIX
    SDLCFLAGS := -I$(SDLPREFIX)/include -Dmain=SDL_main
    SDLLIBS   := -L$(SDLPREFIX)/lib -lmingw32 -lSDL2main -lSDL2
    SDL2_DLL  := $(SDLPREFIX)/bin/SDL2.dll
  else
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
  LDLIBS_EXTRA :=
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
	$(CC) $(CFLAGS_BASE) $(SDLCFLAGS) $(OBJS_SEQ) $(SDLLIBS) $(LDLIBS_EXTRA) -o $(SEQ_BIN)
	@if [ -f "$(SDL2_DLL)" ]; then cp -f "$(SDL2_DLL)" "$(BIN_DIR)/" || true; fi

# --- Paralelo (con OpenMP) ---
$(OBJ_DIR_OMP)/%.o: src/%.c
	$(CC) $(CFLAGS_BASE) -fopenmp $(SDLCFLAGS) -c $< -o $@

omp: $(OBJS_OMP)
	$(CC) $(CFLAGS_BASE) -fopenmp $(SDLCFLAGS) $(OBJS_OMP) $(SDLLIBS) $(LDLIBS_EXTRA) -o $(OMP_BIN)
	@if [ -f "$(SDL2_DLL)" ]; then cp -f "$(SDL2_DLL)" "$(BIN_DIR)/" || true; fi

# -------------------------
# Atajos de ejecución
# -------------------------
RUN_N ?= 2000
RUN_W ?= 1280
RUN_H ?= 720
RUN_SEED ?= 42
RUN_FPSCAP ?= 0
RUN_THREADS ?= 8

run_seq: seq
	$(SEQ_BIN) $(RUN_N) $(RUN_W) $(RUN_H) --seed $(RUN_SEED) --fpscap $(RUN_FPSCAP)

run_omp: omp
	$(OMP_BIN) $(RUN_N) $(RUN_W) $(RUN_H) --seed $(RUN_SEED) --fpscap $(RUN_FPSCAP) --threads $(RUN_THREADS)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all prep seq omp run_seq run_omp clean
