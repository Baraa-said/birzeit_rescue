#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>

#include "config.h"
#include "pool.h"
#include "genetic.h"
#include "astar.h"

static StartMode ask_start_mode(void){
    printf("Choose robot starting position:\n");
    printf("1) Top surface (z = grid_z-1)\n");
    printf("2) Edges/boundary (any 3D border)\n");
    printf("3) Random anywhere (non-obstacle)\n");
    printf("Enter choice (1-3): ");
    fflush(stdout);
    int c=3;
    if(scanf("%d",&c)!=1) c=3;
    if(c<1||c>3) c=3;
    return (StartMode)c;
}

static double now_sec(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec/1e9;
}

// for A* baseline start (same rule as GA start-mode)
static int is_edge(int x,int y,int z){
    return (x==0 || x==config.grid_x-1 || y==0 || y==config.grid_y-1 || z==0 || z==config.grid_z-1);
}
static Coord pick_start_for_baseline(StartMode m){
    Coord c;
    if(m==START_TOP){
        do{
            c.x=rand()%config.grid_x;
            c.y=rand()%config.grid_y;
            c.z=config.grid_z-1;
        }while(get_cell(c)==OBSTACLE);
        return c;
    }
    if(m==START_EDGES){
        do{
            c.x=rand()%config.grid_x;
            c.y=rand()%config.grid_y;
            c.z=rand()%config.grid_z;
        }while(!is_edge(c.x,c.y,c.z) || get_cell(c)==OBSTACLE);
        return c;
    }
    do{
        c.x=rand()%config.grid_x;
        c.y=rand()%config.grid_y;
        c.z=rand()%config.grid_z;
    }while(get_cell(c)==OBSTACLE);
    return c;
}

int main(int argc,char **argv){
    const char *cfg = (argc>1)?argv[1]:"config.txt";
    read_config(cfg);
    print_config();

    g_start_mode = ask_start_mode();
    printf("✅ Start mode selected: %d\n\n", (int)g_start_mode);

    srand((unsigned int)time(NULL));

    init_shared_memory();
    init_grid();
    init_population(shared->population);

    // ---- A* baseline timing (comparison only) ----
    Coord baseline_start = pick_start_for_baseline(g_start_mode);
    Path astar_path;

    double t0_astar = now_sec();
    astar_build_baseline(&astar_path, baseline_start);
    double t1_astar = now_sec();

    write_astar_file("robot_data_astar.txt", &astar_path);

    printf("=== A* Baseline (comparison only) ===\n");
    printf("A* time: %.6f sec\n", (t1_astar - t0_astar));
    printf("A* fitness: %.2f | length: %d | unique survivors: %d | priority sum: %d | coverage: %d\n\n",
           astar_path.fitness, astar_path.length, astar_path.survivors_reached, astar_path.priority_sum, astar_path.coverage);

    // ---- GA run timing ----
    pid_t pids[config.num_processes];
    create_process_pool(pids);

    int snapshot_interval = 20;
    int next_snapshot = snapshot_interval;
    int snap_count = 0;

    double t0_ga = now_sec();
    time_t start_wall = time(NULL);

    double last_best = -1e18;
    int stagn = 0;
    int last_seen_gen = -1;

    while(1){
        lock_sem();
        int gen = shared->generation;
        double best = shared->best_fitness;
        int stop = shared->stop_flag;
        unlock_sem();

        if(stop) break;
        if(gen >= config.num_generations) break;

        if(gen != last_seen_gen){
            last_seen_gen = gen;

            if(config.stagnation_limit>0){
                if(best <= last_best + 1e-6) stagn++;
                else { stagn=0; last_best=best; }

                if(stagn >= config.stagnation_limit){
                    printf("\n⏹️  Stopping: stagnation reached (%d generations)\n", config.stagnation_limit);
                    lock_sem(); shared->stop_flag=1; unlock_sem();
                    break;
                }
            }

            if(config.time_limit_seconds>0){
                time_t now = time(NULL);
                if((int)difftime(now, start_wall) >= config.time_limit_seconds){
                    printf("\n⏹️  Stopping: time limit reached (%d sec)\n", config.time_limit_seconds);
                    lock_sem(); shared->stop_flag=1; unlock_sem();
                    break;
                }
            }

            if(gen>0 && gen>=next_snapshot){
                snap_count++;
                printf("\n📸 Saving snapshot %d at global generation %d (best=%.2f)\n",
                       snap_count, gen, best);
                lock_sem(); write_data_file(snap_count); unlock_sem();
                next_snapshot += snapshot_interval;
            }

            if(gen%50==0 && gen>0){
                printf("GLOBAL Gen %d | Best Fitness: %.2f\n", gen, best);
            }
        }

        usleep(100000);
    }

    // final snapshot
    lock_sem();
    snap_count++;
    write_data_file(snap_count);
    unlock_sem();

    wait_for_workers(pids);

    double t1_ga = now_sec();

    printf("\n=== Final GA Results ===\n");
    printf("GA time: %.6f sec\n", (t1_ga - t0_ga));
    printf("Best fitness: %.2f\n", shared->best_fitness);
    printf("Best path length: %d\n", shared->best_path.length);
    printf("Unique survivors reached: %d\n", shared->best_path.survivors_reached);
    printf("Priority sum reached: %d\n", shared->best_path.priority_sum);
    printf("Coverage (cells): %d\n", shared->best_path.coverage);

    printf("\n=== Time Comparison ===\n");
    printf("A* time: %.6f sec | GA time: %.6f sec\n", (t1_astar - t0_astar), (t1_ga - t0_ga));

    cleanup_shared_memory();
    return 0;
}
