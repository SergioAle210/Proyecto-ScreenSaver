// Librerias
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <omp.h>

// Definiciones
#define SLEEP_ONE_SECOND() sleep(1)

// Enumeracion de estados de semáforo
typedef enum
{
    RED = 0,
    GREEN = 1,
    YELLOW = 2
} LightState;

// Estructura del semáforo
typedef struct
{
    int id;              // Identificador del semáforo
    LightState state;    // Estado actual (RED, GREEN, YELLOW)
    int timer;           // Ticks restantes del estado actual
    int duration_red;    // Duración del rojo
    int duration_green;  // Duración del verde
    int duration_yellow; // Duración del amarillo
} TrafficLight;

// Estructura del vehículo
typedef struct
{
    int id;        // Identificador del vehículo
    int direction; // A qué semáforo obedece (0..num_luces-1)
    int position;  // Posición discreta (celdas avanzadas)
    int speed;     // Celdas por tick
} Vehicle;

// Estructura de la intersección
typedef struct
{
    TrafficLight *lights;
    int num_lights; // Número de semáforos
    Vehicle *vehicles;
    int num_vehicles; // Número de vehículos
    // Usa punteros para que sea dinámico
} Intersection;

// Cambia al siguiente estado y reinicia el temporizador.
// Ciclo: GREEN -> YELLOW -> RED -> GREEN
static void next_state(TrafficLight *L)
{
    // Si el semáforo está en verde, cambia a amarillo
    if (L->state == GREEN)
    {
        L->state = YELLOW;
        L->timer = L->duration_yellow; // Reiniciar el temporizador
    }
    // Si está en amarillo, cambia a rojo
    else if (L->state == YELLOW)
    {
        L->state = RED;
        L->timer = L->duration_red;
    }
    // Si está en rojo, cambia a verde
    else
    {
        L->state = GREEN;
        L->timer = L->duration_green;
    }
}

// Función para inicializar los semáforos
// Patrón inicial desfasado: 0:GREEN, 1:RED, 2:YELLOW, 3:GREEN, ...
// Esto evita que todos empiecen sincronizados.
void init_lights(TrafficLight *lights, int n)
{
    for (int i = 0; i < n; ++i)
    {
        // Asigna un ID al semáforo
        lights[i].id = i;

        if (i % 3 == 0)
        {
            lights[i].state = GREEN;
            lights[i].duration_green = 3;
            lights[i].duration_yellow = 1;
            lights[i].duration_red = 3;
            // timer acorde al estado inicial
            lights[i].timer = lights[i].duration_green;
        }
        else if (i % 3 == 1)
        {
            lights[i].state = RED;
            lights[i].duration_green = 3;
            lights[i].duration_yellow = 1;
            lights[i].duration_red = 3;
            lights[i].timer = lights[i].duration_red;
        }
        else
        {
            lights[i].state = YELLOW;
            lights[i].duration_green = 3;
            lights[i].duration_yellow = 1;
            lights[i].duration_red = 3;
            lights[i].timer = lights[i].duration_yellow;
        }
    }
}

// Inicialización de vehículos
// Dirección aleatoria (semáforo que obedecen), posición 0, velocidad 1..2.
void init_vehicles(Vehicle *vehicles, int n, int num_lights)
{

    for (int i = 0; i < n; ++i)
    {
        vehicles[i].id = i;
        vehicles[i].direction = rand() % num_lights;
        vehicles[i].position = 0; // inicial
        vehicles[i].speed = 1 + (rand() % 2);
    }
}

// Función para actualizar los semáforos
// - schedule(static): costo homogéneo por semáforo.
// - Cada iteración escribe sólo en lights[i]
void update_traffic_lights_parallel(TrafficLight *lights, int num_lights)
{
    // Distribución uniforme de iteraciones
#pragma omp parallel for schedule(static)
    for (int i = 0; i < num_lights; ++i)
    {
        TrafficLight *L = &lights[i];
        L->timer--; // Avanza un tick
        // Si el temporizador llega a cero, cambia al siguiente estado
        if (L->timer <= 0)
        {
            next_state(L);
        }
    }
}

// Movimiento de vehículos (PARALELIZADO)
// - schedule(dynamic,4): balancea mejor si el costo por vehículo varía.
// - Cada hilo escribe sólo en vehicles[i]; sólo LEE lights[...]
void move_vehicles_parallel(Vehicle *vehicles, int num_vehicles,
                            TrafficLight *lights, int num_lights)
{

#pragma omp parallel for schedule(dynamic, 4)
    for (int i = 0; i < num_vehicles; ++i)
    {
        Vehicle *V = &vehicles[i]; // cada hilo procesa un vehículo

        TrafficLight *L = &lights[V->direction % num_lights]; // semáforo asociado (índice seguro)
        int advance = 0;
        if (L->state == GREEN)
        {
            advance = 1; // verde: siempre avanza
        }
        // amarillo: ~50% “determinista” (sin rand) para evitar overhead y no compartir estado
        else if (L->state == YELLOW)
        {
            advance = ((V->id + V->position) % 2 == 0);
        }
        // rojo: no avanza
        // Si se determina que el vehículo debe avanzar, se incrementa su posición en base a su velocidad
        if (advance)
        {
            V->position += V->speed;
        }
    }
}

// Función para imprimir el estado de la simulación
void print_state(int iter, const Vehicle *vehicles, int num_vehicles,
                 const TrafficLight *lights, int num_lights)
{
    printf("Iteracion %d\n", iter);
    for (int i = 0; i < num_vehicles; ++i)
    {
        printf("Vehiculo %d - Posicion: %d (dir=%d)\n",
               vehicles[i].id, vehicles[i].position, vehicles[i].direction);
    }
    for (int j = 0; j < num_lights; ++j)
    {
        printf("Semaforo %d - Estado: %d (timer=%d)\n",
               lights[j].id, (int)lights[j].state, lights[j].timer);
    }
    puts("");
}

// Bucle de simulación (PARALELO con tareas + datos)
// - omp_set_dynamic(1): permite al runtime ajustar hilos si conviene.
// - omp_set_nested(1): habilita paralelismo anidado (for dentro de sections).
// - sections: corre “actualizar semáforos” y “mover vehículos” a la vez.
void simulate_traffic_dynamic(int num_iterations, Intersection *inter, int sleep_between)
{
    omp_set_dynamic(1);
    omp_set_nested(1);

    for (int it = 1; it <= num_iterations; ++it)
    {
        // Heurística simple: más vehículos => más hilos.
        int num_threads = inter->num_vehicles / 10 + 2;
        // Asegura que al menos 2 hilos estén disponibles para la paralelización

        if (num_threads < 2)
            num_threads = 2;
        omp_set_num_threads(num_threads);

#pragma omp parallel sections
        {
#pragma omp section
            {
                // Actualiza los semáforos en paralelo
                update_traffic_lights_parallel(inter->lights, inter->num_lights);
            }
#pragma omp section
            {
                // Mueve los vehículos en paralelo
                move_vehicles_parallel(inter->vehicles, inter->num_vehicles,
                                       inter->lights, inter->num_lights);
            }
        } // Se terminan de sincronizar las secciones paralelas

        // Estado tras la iteración
        print_state(it, inter->vehicles, inter->num_vehicles, inter->lights, inter->num_lights);

        if (sleep_between)
            SLEEP_ONE_SECOND();
    }
}

int main(int argc, char **argv)
{
    // Parámetros (valores por defecto si no se pasan)
    int iterations = (argc > 1) ? atoi(argv[1]) : 4;
    int numVehicles = (argc > 2) ? atoi(argv[2]) : 20;
    int numLights = (argc > 3) ? atoi(argv[3]) : 4;
    int sleep_between = 1;

    srand((unsigned)time(NULL)); // Resultados diferentes en cada ejecución

    // Memoria dinámica (tamaños definidos en tiempo de ejecución)
    TrafficLight *lights = (TrafficLight *)malloc(sizeof(TrafficLight) * numLights);
    Vehicle *vehicles = (Vehicle *)malloc(sizeof(Vehicle) * numVehicles);

    if (!lights || !vehicles)
    {
        fprintf(stderr, "Error: memoria insuficiente.\n");
        free(lights);
        free(vehicles);
        return 1;
    }

    // Inicialización de datos
    init_lights(lights, numLights);
    init_vehicles(vehicles, numVehicles, numLights);

    // Crea un interseccion que agrupa semáforos y vehículos y el conteo de cada uno
    Intersection inter = {lights, numLights, vehicles, numVehicles};

    // Inicia la simulación del tráfico
    simulate_traffic_dynamic(iterations, &inter, sleep_between);

    // Libera la memoria reservada
    free(lights);
    free(vehicles);
    return 0;
}
