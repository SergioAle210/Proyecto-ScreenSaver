# Screensaver (SDL2 + OpenMP)

Screensaver en C que renderiza tres modos: particles, cube3d y cloth. El modo cloth tiene dos backends de dibujo: secuencial y paralelo (OpenMP + SDL_RenderGeometry), compartiendo la misma lógica de simulación.

## Requisitos

- Parametrizable por N (cantidad de elementos).
  
- Colores pseudoaleatorios.
  
- Tamaño mínimo 640×480.
  
- Movimiento y uso de física/trigonometría.
  
- Mostrar FPS en ejecución.
  

## Dependencias

- SDL2 (headers y libs).
  
- OpenMP (para binario paralelo).
  
- Compilador C11.
  

### Compilación

```bash
make clean && make
```

Genera:

- **screensaver_seq** → versión secuencial.
  
- **screensaver_par** → versión paralela (-fopenmp).
  

### Ejecución (ejemplos)

**Paralelo (recomendado, más FPS):**

```bash
./screensaver_par 0 --mode cloth --grid 220x136 --fov 1.6 --zcam -1.0 --amp 0.35 --sigma 0.22 --colorSpeed 0.6 --tilt 16 --threads 8 --fpscap 0 --novsync
```

**Secuencial:**

```bash
./screensaver_seq 0 --mode cloth --grid 160x100 --fov 1.6 --zcam -1.0 --amp 0.35 --sigma 0.22 --colorSpeed 0.6 --tilt 16 --fpscap 0 --novsync
```

**Diagnóstico (forzar fallback secuencial desde el binario paralelo):**

```bash
./screensaver_par 0 --mode cloth --grid 200x120 --nogeom --fpscap 0 --novsync
```

### Argumentos principales

- **N**: número base de elementos (usado por particles; en cloth se deriva la grilla si no indicas --grid).
  
- --mode particles|cube3d|cloth
  
- --grid GXxGY (cloth)
  
- --tilt, --fov, --zcam, --spanX, --spanY, --radius, --amp, --sigma, --speed, --colorSpeed, --panX, --panY, --center
  
- --threads T (solo paralelo)
  
- --fpscap X (0 = sin límite)
  
- --novsync
  
- --nogeom (opcional; fuerza backend secuencial desde el binario paralelo)
  

**Estructura del proyecto**

.

├── Makefile

├── README.md

├── screensaver_par           # binario paralelo (OpenMP)

├── screensaver_seq           # binario secuencial

└── src

    ├── main.c                # CLI, bucle principal, selección de modo/backend

    ├── sim.c / sim.h         # utilidades comunes (partículas, tipos DrawItem)

    ├── cloth.h               # API pública de la manta (parámetros y estado)

    ├── cloth_core.c          # lógica común: update, proyección, bucketing

    ├── cloth_draw_seq.c      # backend secuencial (RenderCopyF por esfera)

    ├── cloth_draw_omp.c      # backend paralelo (RenderGeometry + batch)

### Diseño

- **PCAM (Partición–Comunicación–Agregación–Mapeo):**
  
  - **Partición**: grilla GX×GY (datos independientes).
    
  - **Comunicación**: no hay dependencia entre celdas en el update (solo reducciones).
    
  - **Agregación**: reducciones para zmin/zmax y bounding box; bucketing O(N).
    
  - **Mapeo**: OpenMP for/collapse + reducciones → core con buen balance.
    
- **Paralelo vs Secuencial:**
  
  - **Paralelo**: update + ordenado por profundidad + batch draw (un solo draw call).
    
  - **Secuencial**: mismo ordenado, pero muchas llamadas a RenderCopyF.
    

**Notas de rendimiento**

- Histogramas por bin sin atomics (acumulación local por hilo + reducción).
  
- Bounding box con reducciones min/max (sin critical).
  
- Textura del sprite STATIC + SDL_UpdateTexture (evita pantallas negras con RenderGeometry).
  
- Bandera --nogeom para diagnóstico rápido.