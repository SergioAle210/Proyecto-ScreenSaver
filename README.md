# Screensaver – Cloth (SDL2 + OpenMP)

Screensaver en **C** que renderiza una manta de esferas 3D (*cloth*) con **SDL2**. 
Incluye dos backends de dibujo que comparten la misma lógica de simulación:

- **Secuencial**: muchas llamadas `SDL_RenderCopyF` (una por esfera).
- **Paralelo (OpenMP)**: genera geometría en paralelo y usa **un solo draw call** con `SDL_RenderGeometry` (con *fallback* automático al secuencial si el renderer no soporta geometry).

---

## Requisitos del proyecto (académicos)
- Parametrizable por **N** (cantidad de elementos).  
  - En **cloth**, `N = GX × GY` cuando usas `--grid`.  
  - Si **no** pasas `--grid`, el programa usa el `N` de la CLI para derivar una grilla inicial, que luego ajusta según el aspecto de la pantalla.
- Despliegue de **colores** (HSV → RGB).
- Tamaño mínimo de **canvas** ≥ 640×480 (la app abre y se maximiza a resolución de pantalla).
- **Movimiento** continuo con trigonometría/física (ondas senoidales, rotaciones y proyección 3D).
- Mostrar **FPS** en el título de la ventana.

---

## Dependencias
- **SDL2** (headers y librerías).
- **OpenMP** (solo para el binario paralelo).
- Compilador compatible con **C11** (GCC/Clang).

> macOS (Clang): instala `libomp` (`brew install libomp`). En la mayoría de distros Linux basta con GCC y `libsdl2-dev`/equivalente.

---

## Compilación

```bash
make clean && make
```

Se generan dos binarios:
- **screensaver_seq** → versión *secuencial* (sin OpenMP).
- **screensaver_par** → versión *paralela* (con `-fopenmp`).

---

## Ejecución (ejemplos)

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

> **Sobre `N`:** con `--grid GXxGY`, el número de esferas es **N = GX × GY** (p. ej., `220×136 = 29920`). Si no pasas `--grid`, puedes dar un `N` inicial (ej. `./screensaver_par 1000 --mode cloth`) y el programa intentará derivar una malla rectangular a partir de ese valor.

---

## Argumentos principales

- `N` : número base de elementos (deriva grilla si no usas `--grid`).
- `--mode cloth`
- `--grid GXxGY` : define explícitamente la grilla (y, por ende, **N**).
- `--tilt DEG` : inclinación X en grados.
- `--fov F` : campo de visión (≈ `1.0` a `2.2`).
- `--zcam Z` : posición de cámara (valores negativos acercan).
- `--spanX Sx`, `--spanY Sy` : tamaño “mundo” de la manta.
- `--radius R` : radio base por esfera (px). Si no se indica, se ajusta automáticamente.
- `--amp A`, `--sigma S`, `--speed V`, `--colorSpeed C` : parámetros de animación/color.
- `--panX px`, `--panY px`, `--center 0|1` : paneo/centrado en pantalla.
- `--fpscap X` : limita FPS (0 = sin límite).
- `--novsync` : desactiva VSync.
- `--threads T` : **solo** en el binario paralelo (OpenMP).
- `--nogeom` : **solo** en el binario paralelo; fuerza backend secuencial (útil para diagnóstico).

---

## Estructura del proyecto

```
.
├── Makefile
├── README.md
├── screensaver_par           # binario paralelo (OpenMP)
├── screensaver_seq           # binario secuencial
└── src
    ├── main.c                # CLI, bucle principal, selección de backend
    ├── sim.h                 # tipo DrawItem (definición mínima)
    ├── cloth.h               # API pública: parámetros/estado y firmas
    ├── cloth_core.c          # lógica común: update, proyección, bucket sort
    ├── cloth_draw_seq.c      # backend secuencial (RenderCopyF por esfera)
    └── cloth_draw_omp.c      # backend paralelo (RenderGeometry + batch)
```

---

## Diseño (resumen)
- **PCAM**  
  - **Partición**: la grilla `GX×GY` divide el trabajo por celdas.  
  - **Comunicación**: sin dependencias entre celdas en el *update*; solo **reducciones** globales (zmin/zmax, bounding box).  
  - **Agregación**: *bucket sort* O(N) por profundidad → orden *painter’s*.  
  - **Mapeo**: OpenMP `parallel for` (+ `collapse` y `reduction`) en *update* y construcción de geometría.

- **Paralelo vs Secuencial**
  - **Paralelo**: un único *draw call* con `SDL_RenderGeometry` (si no está soportado, cae a secuencial).  
  - **Secuencial**: recorre el orden de dibujo y hace `RenderCopyF` por esfera.

---

## Notas de rendimiento
- *Bucket sort* lineal con histogramas; reducciones `min/max`.  
- Sprite circular como textura **STATIC** + `SDL_UpdateTexture` (evita pantallas negras con `RenderGeometry` en algunos drivers).  
- `--nogeom` permite comparar rápidamente ambos backends en el binario paralelo.

---

## Créditos
- SDL2 (Simple DirectMedia Layer)  
- OpenMP (Open Multi-Processing)

