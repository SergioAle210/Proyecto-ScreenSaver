// Librerías
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// Definiciones
#define SLEEP_ONE_SECOND() sleep(1)

// Enumeración de estados de semáforo
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
    LightState state;    // Estado actual (RED/GREEN/YELLOW)
    int timer;           // Ticks restantes del estado actual
    int duration_red;    // Duración del rojo (en ticks)
    int duration_green;  // Duración del verde (en ticks)
    int duration_yellow; // Duración del amarillo (en ticks)
} TrafficLight;

// Estructura del vehículo
typedef struct
{
    int id;        // Identificador del vehículo
    int direction; // Índice del semáforo que obedece (0..num_lights-1)
    int position;  // Posición discreta (celdas avanzadas)
    int speed;     // Celdas por tick (1 o 2 aquí)
} Vehicle;

// Estructura de la intersección
// Usa punteros para tamaños dinámicos (reservados con malloc).
typedef struct
{
    TrafficLight *lights;
    int num_lights;
    Vehicle *vehicles;
    int num_vehicles;
} Intersection;

// Cambia al siguiente estado y reinicia el temporizador.
// Ciclo: GREEN -> YELLOW -> RED -> GREEN
static void next_state(TrafficLight *L)
{
    if (L->state == GREEN)
    {
        L->state = YELLOW;
        L->timer = L->duration_yellow;
    }
    else if (L->state == YELLOW)
    {
        L->state = RED;
        L->timer = L->duration_red;
    }
    else
    {
        L->state = GREEN;
        L->timer = L->duration_green;
    }
}

// Inicialización de semáforos
// Patrón inicial desfasado: 0:GREEN, 1:RED, 2:YELLOW, 3:GREEN, ...
// Evita que todos empiecen sincronizados.
void init_lights(TrafficLight *lights, int n)
{
    for (int i = 0; i < n; ++i)
    {
        lights[i].id = i;

        if (i % 3 == 0)
        {
            lights[i].state = GREEN;
            lights[i].duration_green = 3;
            lights[i].duration_yellow = 1;
            lights[i].duration_red = 3;
            lights[i].timer = lights[i].duration_green; // timer acorde al estado inicial
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
        vehicles[i].direction = rand() % num_lights; // asegura rango válido
        vehicles[i].position = 0;                    // posición inicial
        vehicles[i].speed = 1 + (rand() % 2);        // 1 o 2
    }
}

// Actualización de semáforos (SECUENCIAL)
// Recorre todos los semáforos, decrementa timer y cambia de estado si vence.
void update_traffic_lights(TrafficLight *lights, int num_lights)
{
    for (int i = 0; i < num_lights; ++i)
    {
        TrafficLight *L = &lights[i];
        L->timer--; // avanza un tick
        if (L->timer <= 0)
        { // al vencer, rota el estado y resetea timer
            next_state(L);
        }
    }
}

// Movimiento de vehículos (SECUENCIAL)
// Lee el estado del semáforo correspondiente y decide si avanza.
// Verde: avanza; Amarillo: ~50% determinista; Rojo: no avanza.
void move_vehicles(Vehicle *vehicles, int num_vehicles, TrafficLight *lights, int num_lights)
{
    for (int i = 0; i < num_vehicles; ++i)
    {
        Vehicle *V = &vehicles[i];
        TrafficLight *L = &lights[V->direction % num_lights]; // semáforo asociado (índice seguro)

        int advance = 0;
        if (L->state == GREEN)
        {
            advance = 1; // verde: siempre avanza
        }
        else if (L->state == YELLOW)
        {
            // amarillo: ~50% “determinista” (sin rand) usando paridad
            advance = ((V->id + V->position) % 2 == 0);
        }
        // rojo: no avanza

        if (advance)
        {
            V->position += V->speed; // mueve en celdas según su velocidad
        }
    }
}

// Salida de estado
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

// Bucle de simulación (SECUENCIAL)
// Orden fijo por tick: actualizar luces -> mover vehículos -> imprimir.
// Si 'sleep_between' es 1, pausa 1s para observar el avance.
void simulate_traffic(int num_iterations, Intersection *inter, int sleep_between)
{
    for (int it = 1; it <= num_iterations; ++it)
    {
        update_traffic_lights(inter->lights, inter->num_lights);
        move_vehicles(inter->vehicles, inter->num_vehicles, inter->lights, inter->num_lights);
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
    int sleep_between = 1; // 1: pausa de 1s entre iteraciones

    srand((unsigned)time(NULL)); // resultados diferentes en cada ejecución

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

    // Intersección como contenedor del estado
    Intersection inter = {lights, numLights, vehicles, numVehicles};

    // Simulación
    simulate_traffic(iterations, &inter, sleep_between);

    // Limpieza
    free(lights);
    free(vehicles);
    return 0;
}