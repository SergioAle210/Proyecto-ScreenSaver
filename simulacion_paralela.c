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
    LightState state;    // Estado actual
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

// Prototipos de funciones
// Función para cambiar al siguiente estado del semáforo según su estado actual
// El Ciclo es GREEN -> YELLOW -> RED -> GREEN
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
        L->timer = L->duration_red; // Reiniciar el temporizador
    }
    // Si está en rojo, cambia a verde
    else
    {
        L->state = GREEN;
        L->timer = L->duration_green; // Reiniciar el temporizador
    }
}

// Función para inicializar los semáforos
void init_lights(TrafficLight *lights, int n)
{
    // Inicializa cada semáforo con un estado y duraciones predefinidas
    for (int i = 0; i < n; ++i)
    {
        // Asigna un ID al semáforo
        lights[i].id = i;

        // Define el estado y duraciones según el ID del semáforo
        // 0 -> GREEN, 1 -> RED, 2 -> YELLOW
        // Esto simula que los semaforos no esten sincronizados
        if (i % 3 == 0)
        {
            lights[i].state = GREEN;
            lights[i].duration_green = 3;
            lights[i].duration_yellow = 1;
            lights[i].duration_red = 3;
            // El temporizador se inicializa con la duración del verde
            lights[i].timer = lights[i].duration_green;
        }
        else if (i % 3 == 1)
        {
            lights[i].state = RED;
            lights[i].duration_green = 3;
            lights[i].duration_yellow = 1;
            lights[i].duration_red = 3;
            // El temporizador se inicializa con la duración del rojo
            lights[i].timer = lights[i].duration_red;
        }
        else
        {
            lights[i].state = YELLOW;
            lights[i].duration_green = 3;
            lights[i].duration_yellow = 1;
            lights[i].duration_red = 3;
            // El temporizador se inicializa con la duración del amarillo
            lights[i].timer = lights[i].duration_yellow;
        }
    }
}

// Función para inicializar los vehículos
void init_vehicles(Vehicle *vehicles, int n, int num_lights)
{
    // Inicializa cada vehículo con un ID, dirección aleatoria, posición inicial y velocidad (1 o 2 celdas por tick)
    for (int i = 0; i < n; ++i)
    {
        vehicles[i].id = i;
        vehicles[i].direction = rand() % num_lights;
        vehicles[i].position = 0;
        vehicles[i].speed = 1 + (rand() % 2);
    }
}

// Función para actualizar los semáforos
void update_traffic_lights_parallel(TrafficLight *lights, int num_lights)
{
    // Distribución uniforme de iteraciones
#pragma omp parallel for schedule(static)
    for (int i = 0; i < num_lights; ++i)
    {
        TrafficLight *L = &lights[i];
        // Decrementa el temporizador del semáforo
        L->timer--;
        // Si el temporizador llega a cero, cambia al siguiente estado
        if (L->timer <= 0)
        {
            next_state(L);
        }
    }
}

// Función para mover los vehículos
// Esta función se paraleliza para que cada vehículo pueda moverse independientemente
void move_vehicles_parallel(Vehicle *vehicles, int num_vehicles,
                            TrafficLight *lights, int num_lights)
{

    // No se bloquea a los hilos esperando iteraciones mas "pesas"
#pragma omp parallel for schedule(dynamic, 4)
    for (int i = 0; i < num_vehicles; ++i)
    {
        // Obtiene el vehículo actual
        // En cada iteración, cada hilo procesa un vehículo
        Vehicle *V = &vehicles[i];

        // Obtiene el semáforo que el vehículo debe obedecer
        TrafficLight *L = &lights[V->direction % num_lights]; // num_lights asegura que no se salga del rango
        int advance = 0;
        // Si el semáforo está en verde, el vehículo avanza
        if (L->state == GREEN)
        {
            advance = 1;
        }
        // Si el semáforo está en amarillo, el vehículo avanza si su ID y posición son pares (El 50% de los vehículos avanzan)
        else if (L->state == YELLOW)
        {
            advance = ((V->id + V->position) % 2 == 0);
        }
        // Si se determina que el vehículo debe avanzar, se incrementa su posición
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
    // Imprime el estado de los vehículos y semáforos en cada iteración
    printf("Iteracion %d\n", iter);
    for (int i = 0; i < num_vehicles; ++i)
    {
        printf("Vehiculo %d - Posicion: %d (dir=%d)\n",
               vehicles[i].id, vehicles[i].position, vehicles[i].direction);
    }
    // Imprime el estado de cada semáforo
    for (int j = 0; j < num_lights; ++j)
    {
        printf("Semaforo %d - Estado: %d (timer=%d)\n",
               lights[j].id, (int)lights[j].state, lights[j].timer);
    }
    puts("");
}

// Función para simular el tráfico dinámico
void simulate_traffic_dynamic(int num_iterations, Intersection *inter, int sleep_between)
{
    omp_set_dynamic(1); // Permite que OpenMP ajuste dinámicamente el número de hilos
    omp_set_nested(1);  // Permite secciones paralelas anidadas

    // Itera sobre el número de iteraciones especificado
    for (int it = 1; it <= num_iterations; ++it)
    {
        // Para cada 10 vehiculos se agrega un hilo, y se asegura que al menos 2 hilos estén disponibles
        int num_threads = inter->num_vehicles / 10 + 2;

        // Asegura que al menos 2 hilos estén disponibles para la paralelización
        if (num_threads < 2)
            num_threads = 2;
        omp_set_num_threads(num_threads);

        // Sección paralela para actualizar semáforos y mover vehículos
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
        }

        // Imprime el estado de la simulación después de cada iteración
        print_state(it, inter->vehicles, inter->num_vehicles, inter->lights, inter->num_lights);

        if (sleep_between)
            SLEEP_ONE_SECOND();
    }
}

int main(int argc, char **argv)
{
    // Configuración de parámetros de la simulación
    // Si no se pasan argumentos, se usan valores por defecto
    int iterations = (argc > 1) ? atoi(argv[1]) : 4;
    int numVehicles = (argc > 2) ? atoi(argv[2]) : 20;
    int numLights = (argc > 3) ? atoi(argv[3]) : 4;
    int sleep_between = 1;

    // Configura la semilla del generador de números aleatorios usando la hora actual
    srand((unsigned)time(NULL));

    // Reserva memoria para los semáforos y vehículos (mallox para tamano dinámico)
    TrafficLight *lights = (TrafficLight *)malloc(sizeof(TrafficLight) * numLights);
    Vehicle *vehicles = (Vehicle *)malloc(sizeof(Vehicle) * numVehicles);

    // Verifica si la memoria se reservó correctamente
    if (!lights || !vehicles)
    {
        fprintf(stderr, "Error: memoria insuficiente.\n");
        free(lights);
        free(vehicles);
        return 1;
    }

    // Establece estado, duracion y temporizador de los semáforos
    init_lights(lights, numLights);
    // Inicializa la dirección, posición y velocidad de los vehículos
    init_vehicles(vehicles, numVehicles, numLights);

    // Crea un interseccion que agrupa semáforos y vehículos y el conteo de cada uno
    Intersection inter = {lights, numLights, vehicles, numVehicles};

    // Inicia la simulación del tráfico
    simulate_traffic_dynamic(iterations, &inter, sleep_between);

    // Libera la memoria reservada para semáforos y vehículos
    free(lights);
    free(vehicles);
    return 0;
}
