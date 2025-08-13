

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <omp.h>

#define SLEEP_ONE_SECOND() sleep(1)

typedef enum
{
    RED = 0,
    GREEN = 1,
    YELLOW = 2
} LightState;

typedef struct
{
    int id;
    LightState state;
    int timer;
    int duration_red;
    int duration_green;
    int duration_yellow;
} TrafficLight;

typedef struct
{
    int id;
    int direction;
    int position;
    int speed;
} Vehicle;

typedef struct
{
    TrafficLight *lights;
    int num_lights;
    Vehicle *vehicles;
    int num_vehicles;
} Intersection;

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

void init_vehicles(Vehicle *vehicles, int n, int num_lights)
{
    for (int i = 0; i < n; ++i)
    {
        vehicles[i].id = i;
        vehicles[i].direction = rand() % num_lights;
        vehicles[i].position = 0;
        vehicles[i].speed = 1 + (rand() % 2);
    }
}

void update_traffic_lights_parallel(TrafficLight *lights, int num_lights)
{
#pragma omp parallel for schedule(static)
    for (int i = 0; i < num_lights; ++i)
    {
        TrafficLight *L = &lights[i];
        L->timer--;
        if (L->timer <= 0)
        {
            next_state(L);
        }
    }
}

void move_vehicles_parallel(Vehicle *vehicles, int num_vehicles,
                            TrafficLight *lights, int num_lights)
{
#pragma omp parallel for schedule(dynamic, 4)
    for (int i = 0; i < num_vehicles; ++i)
    {
        Vehicle *V = &vehicles[i];
        TrafficLight *L = &lights[V->direction % num_lights];
        int advance = 0;
        if (L->state == GREEN)
        {
            advance = 1;
        }
        else if (L->state == YELLOW)
        {
            advance = ((V->id + V->position) % 2 == 0);
        }
        if (advance)
        {
            V->position += V->speed;
        }
    }
}

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

void simulate_traffic_dynamic(int num_iterations, Intersection *inter, int sleep_between)
{
    omp_set_dynamic(1);
    omp_set_nested(1);

    for (int it = 1; it <= num_iterations; ++it)
    {
        int num_threads = inter->num_vehicles / 10 + 2;
        if (num_threads < 2)
            num_threads = 2;
        omp_set_num_threads(num_threads);

#pragma omp parallel sections
        {
#pragma omp section
            {
                update_traffic_lights_parallel(inter->lights, inter->num_lights);
            }
#pragma omp section
            {
                move_vehicles_parallel(inter->vehicles, inter->num_vehicles,
                                       inter->lights, inter->num_lights);
            }
        }

        print_state(it, inter->vehicles, inter->num_vehicles, inter->lights, inter->num_lights);
        if (sleep_between)
            SLEEP_ONE_SECOND();
    }
}

int main(int argc, char **argv)
{
    int iterations = (argc > 1) ? atoi(argv[1]) : 4;
    int numVehicles = (argc > 2) ? atoi(argv[2]) : 20;
    int numLights = (argc > 3) ? atoi(argv[3]) : 4;
    int sleep_between = 0;

    srand((unsigned)time(NULL));

    TrafficLight *lights = (TrafficLight *)malloc(sizeof(TrafficLight) * numLights);
    Vehicle *vehicles = (Vehicle *)malloc(sizeof(Vehicle) * numVehicles);
    if (!lights || !vehicles)
    {
        fprintf(stderr, "Error: memoria insuficiente.\n");
        free(lights);
        free(vehicles);
        return 1;
    }

    init_lights(lights, numLights);
    init_vehicles(vehicles, numVehicles, numLights);

    Intersection inter = {lights, numLights, vehicles, numVehicles};
    simulate_traffic_dynamic(iterations, &inter, sleep_between);

    free(lights);
    free(vehicles);
    return 0;
}
