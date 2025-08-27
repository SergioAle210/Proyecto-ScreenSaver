# Screensaver 4D — C (C11) + SDL2 + OpenMP

Simulador tipo *screensaver* en **C puro** con dos binarios: **secuencial** y **paralelo (OpenMP)**.

- Partículas en **4D** con rotaciones en planos *(x↔w, y↔z)*, rebotes y proyección 4D→3D→2D.
- **FPS** en el título de la ventana.
- Parámetros por CLI: `N`, tamaño de ventana, semilla, limitador de FPS y hilos.

## Compilación

### Linux

```bash
sudo apt-get install -y libsdl2-dev
make clean
make            # genera build/screensaver4d_seq y build/screensaver4d_omp
```

VERSIÓN SECUENCIAL (`screensaver_seq`)

```
🌈 Modo partículas sin límite de FPS (semilla por defecto):
./screensaver_seq 1000 --mode particles

🎨 Modo partículas con semilla personalizada y 30 FPS:
./screensaver_seq 800 --mode particles --seed 123 --fpscap 30

🧊 Modo cubo 3D sin límite de FPS:
./screensaver_seq 1 --mode cube3d
```

VERSIÓN PARALELA

```
🌈 Modo partículas usando 4 hilos y 60 FPS:
./screensaver_par 2000 --mode particles --threads 4 --fpscap 60

🎨 Modo partículas sin límite de FPS, semilla fija:
./screensaver_par 1500 --mode particles --seed 42 --threads 2

🧊 Modo cubo 3D (no requiere threads, pero lo puedes dejar):
./screensaver_par 1 --mode cube3d --threads 8
```

### Windows (MSYS2 MinGW64) (CAMBIAR)

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
./screensaver_[seq|par] N [--mode particles|cube3d] [--seed S] [--fpscap X] [--threads T]
```

## Estructura

- `src/main.c` — bucle principal, eventos, FPS, render.
- `src/sim.c/.h` — partículas, física 4D, proyección y buffer de dibujo.
- `Makefile` — targets `seq` y `omp` (con y sin `-fopenmp`).

## Sugerencias de evaluación

- Mide FPS medios con el mismo `N` y resolución en ambos binarios.
- Registra 10+ mediciones para *speedup* y eficiencia.
