# Screensaver 4D — C (C11) + SDL2 + OpenMP

Simulador tipo *screensaver* en **C puro** con dos binarios: **secuencial** y **paralelo (OpenMP)**.
- Partículas en **4D** con rotaciones en planos *(x↔w, y↔z)*, rebotes y proyección 4D→3D→2D.
- **FPS** en el título de la ventana.
- Parámetros por CLI: `N`, tamaño de ventana, semilla, limitador de FPS y hilos.

## Compilación

### Linux
```bash
sudo apt-get install -y libsdl2-dev
make            # genera build/screensaver4d_seq y build/screensaver4d_omp
./build/screensaver4d_seq 2000 1280 720 --seed 42 --fpscap 0
./build/screensaver4d_omp 2000 1280 720 --seed 42 --fpscap 0 --threads 12
```

### Windows (MSYS2 MinGW64)
```bash
# Instala toolchain + SDL2:
# pacman -S --needed base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-SDL2
mingw32-make
./build/screensaver4d_seq.exe 2000 1280 720 --seed 42 --fpscap 0
./build/screensaver4d_omp.exe 2000 1280 720 --seed 42 --fpscap 0 --threads 12
```

> Si `--threads` no se especifica, usa `OMP_NUM_THREADS` o el valor por defecto del runtime.

## Uso
```
screensaver4d_[seq|omp] N [width height] [--seed S] [--fpscap X] [--threads T]
```

## Estructura
- `src/main.c` — bucle principal, eventos, FPS, render.
- `src/sim.c/.h` — partículas, física 4D, proyección y buffer de dibujo.
- `Makefile` — targets `seq` y `omp` (con y sin `-fopenmp`).

## Sugerencias de evaluación
- Mide FPS medios con el mismo `N` y resolución en ambos binarios.
- Registra 10+ mediciones para *speedup* y eficiencia.
