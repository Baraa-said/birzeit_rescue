#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "genetic.h"
#include "pool.h"
#include "config.h"

StartMode g_start_mode = START_RANDOM;

static inline int idx3(int x,int y,int z){
    return z*config.grid_x*config.grid_y + y*config.grid_x + x;
}
static inline int coord_index(Coord c){ return idx3(c.x,c.y,c.z); }

int is_valid(Coord c){
    return c.x>=0 && c.x<config.grid_x &&
           c.y>=0 && c.y<config.grid_y &&
           c.z>=0 && c.z<config.grid_z;
}
int get_cell(Coord c){
    if(!is_valid(c)) return OBSTACLE;
    return shared->grid[coord_index(c)];
}

static int is_edge_cell(Coord c){
    return (c.x==0 || c.x==config.grid_x-1 ||
            c.y==0 || c.y==config.grid_y-1 ||
            c.z==0 || c.z==config.grid_z-1);
}

static Coord pick_start_coord(void){
    Coord c;
    if(g_start_mode==START_TOP){
        do{
            c.x = rand()%config.grid_x;
            c.y = rand()%config.grid_y;
            c.z = config.grid_z-1;
        } while(get_cell(c)==OBSTACLE);
        return c;
    }
    if(g_start_mode==START_EDGES){
        do{
            c.x = rand()%config.grid_x;
            c.y = rand()%config.grid_y;
            c.z = rand()%config.grid_z;
        } while(!is_edge_cell(c) || get_cell(c)==OBSTACLE);
        return c;
    }
    do{
        c.x = rand()%config.grid_x;
        c.y = rand()%config.grid_y;
        c.z = rand()%config.grid_z;
    } while(get_cell(c)==OBSTACLE);
    return c;
}

static void parse_priorities_into_shared(void){
    for(int i=0;i<config.num_survivors;i++) shared->survivor_priority[i]=config.priority_default;
    if(config.survivor_priorities_csv[0]==0) return;

    char buf[512];
    strncpy(buf, config.survivor_priorities_csv, sizeof(buf)-1);
    buf[sizeof(buf)-1]=0;

    int i=0;
    char *tok = strtok(buf, ",");
    while(tok && i<config.num_survivors){
        int p = atoi(tok);
        if(p<1) p=1;
        shared->survivor_priority[i]=p;
        i++;
        tok = strtok(NULL, ",");
    }
}

void init_grid(void){
    int grid_size = config.grid_x*config.grid_y*config.grid_z;
    for(int i=0;i<grid_size;i++) shared->grid[i]=EMPTY;

    // obstacles
    for(int i=0;i<config.num_obstacles;i++){
        Coord c;
        int id;
        do{
            c.x=rand()%config.grid_x;
            c.y=rand()%config.grid_y;
            c.z=rand()%config.grid_z;
            id=coord_index(c);
        }while(shared->grid[id]!=EMPTY);
        shared->obstacles[i]=c;
        shared->grid[id]=OBSTACLE;
    }

    // survivors
    for(int i=0;i<config.num_survivors;i++){
        Coord c;
        int id;
        do{
            c.x=rand()%config.grid_x;
            c.y=rand()%config.grid_y;
            c.z=rand()%config.grid_z;
            id=coord_index(c);
        }while(shared->grid[id]!=EMPTY);
        shared->survivors[i]=c;
        shared->grid[id]=SURVIVOR;
    }

    parse_priorities_into_shared();
}

void generate_random_path(Path *p){
    int max_len = config.max_path_length;
    if(max_len>MAX_PATH_LENGTH) max_len=MAX_PATH_LENGTH;

    p->length=0;
    p->fitness=0;
    p->survivors_reached=0;
    p->priority_sum=0;
    p->coverage=0;

    Coord cur = pick_start_coord();
    p->genes[p->length++] = cur;

    Coord dirs[6]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};

    for(int step=1; step<max_len; step++){
        int attempts=0;
        Coord nxt;
        do{
            int d=rand()%6;
            nxt=(Coord){cur.x+dirs[d].x, cur.y+dirs[d].y, cur.z+dirs[d].z};
            attempts++;
            if(attempts>20){ return; }
        }while(!is_valid(nxt) || get_cell(nxt)==OBSTACLE);

        p->genes[p->length++] = nxt;
        cur=nxt;
    }
}

void calculate_fitness(Path *p){
    int grid_size = config.grid_x*config.grid_y*config.grid_z;
    unsigned char *visited = (unsigned char*)calloc((size_t)grid_size,1);
    unsigned char *hit = (unsigned char*)calloc((size_t)config.num_survivors,1);
    if(!visited || !hit){ fprintf(stderr,"alloc failed\n"); exit(1); }

    int coverage=0;
    int risk=0;
    int survivors_unique=0;
    int priority_sum=0;

    for(int i=0;i<p->length;i++){
        Coord c=p->genes[i];
        if(!is_valid(c) || get_cell(c)==OBSTACLE){
            p->fitness = -1e18;
            free(visited); free(hit);
            return;
        }

        int id=coord_index(c);
        if(!visited[id]){ visited[id]=1; coverage++; }

        for(int s=0;s<config.num_survivors;s++){
            if(!hit[s] &&
               c.x==shared->survivors[s].x &&
               c.y==shared->survivors[s].y &&
               c.z==shared->survivors[s].z){
                hit[s]=1;
                survivors_unique++;
                priority_sum += shared->survivor_priority[s];
                break;
            }
        }

        for(int dx=-1;dx<=1;dx++)
        for(int dy=-1;dy<=1;dy++)
        for(int dz=-1;dz<=1;dz++){
            Coord n={c.x+dx,c.y+dy,c.z+dz};
            if(is_valid(n) && get_cell(n)==OBSTACLE) risk++;
        }
    }

    free(visited); free(hit);

    p->survivors_reached = survivors_unique;
    p->priority_sum      = priority_sum;
    p->coverage          = coverage;

    // compute total priority and missing
    int total_priority = 0;
    for(int s=0;s<config.num_survivors;s++) total_priority += shared->survivor_priority[s];

    int missing_priority = total_priority - priority_sum;
    if(missing_priority < 0) missing_priority = 0;

    double miss_penalty = config.missing_priority_penalty * (double)missing_priority;
    double full_bonus   = (missing_priority == 0) ? config.full_rescue_bonus : 0.0;

    p->fitness = full_bonus
               + config.w1 * (double)priority_sum
               + config.w2 * (double)coverage
               - config.w3 * (double)p->length
               - config.w4 * (double)risk
               - miss_penalty;
}

static int cmp_desc(const void *a,const void *b){
    const Path *pa=(const Path*)a, *pb=(const Path*)b;
    if(pb->fitness>pa->fitness) return 1;
    if(pb->fitness<pa->fitness) return -1;
    return 0;
}

static int tournament_pick(Path *pop,int N){
    int best=rand()%N;
    for(int i=1;i<config.tournament_size;i++){
        int c=rand()%N;
        if(pop[c].fitness>pop[best].fitness) best=c;
    }
    return best;
}

static void crossover(const Path *p1,const Path *p2,Path *child){
    int min_len = (p1->length<p2->length)?p1->length:p2->length;
    if(min_len<=2){ *child=*p1; return; }

    int cp = 1 + rand()%(min_len-1); // ensure >=1 keeps start
    child->length=0;

    child->genes[child->length++] = p1->genes[0]; // force start

    for(int i=1;i<cp && child->length<MAX_PATH_LENGTH;i++)
        child->genes[child->length++] = p1->genes[i];

    for(int i=cp;i<p2->length && child->length<MAX_PATH_LENGTH;i++)
        child->genes[child->length++] = p2->genes[i];
}

static void mutate(Path *p){
    double r=(double)rand()/(double)RAND_MAX;
    if(r>config.mutation_rate) return;
    if(p->length<2) return;

    // IMPORTANT: never mutate gene[0] so start-mode never breaks
    int mp = 1 + rand()%(p->length-1);

    Coord dirs[6]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    int d=rand()%6;

    Coord nc={p->genes[mp].x+dirs[d].x, p->genes[mp].y+dirs[d].y, p->genes[mp].z+dirs[d].z};
    if(is_valid(nc) && get_cell(nc)!=OBSTACLE) p->genes[mp]=nc;
}

void evolve_population_local(Path *pop,int N){
    qsort(pop,(size_t)N,sizeof(Path),cmp_desc);

    int elite=(int)(config.elitism_percent*N);
    if(elite<1) elite=1;

    Path *newp=(Path*)malloc((size_t)N*sizeof(Path));
    if(!newp){ fprintf(stderr,"alloc failed\n"); exit(1); }

    for(int i=0;i<elite;i++) newp[i]=pop[i];

    for(int i=elite;i<N;i++){
        int p1=tournament_pick(pop,N);
        int p2=tournament_pick(pop,N);

        Path child;
        double cr=(double)rand()/(double)RAND_MAX;
        if(cr<=config.crossover_rate) crossover(&pop[p1],&pop[p2],&child);
        else child=pop[p1];

        mutate(&child);
        calculate_fitness(&child);
        newp[i]=child;
    }

    memcpy(pop,newp,(size_t)N*sizeof(Path));
    free(newp);
}

void init_population(Path *population){
    for(int i=0;i<config.population_size;i++){
        generate_random_path(&population[i]);
        calculate_fitness(&population[i]);
    }
    shared->best_fitness = -1e18;
    shared->best_path = population[0];

    for(int i=0;i<config.population_size;i++){
        if(population[i].fitness>shared->best_fitness){
            shared->best_fitness=population[i].fitness;
            shared->best_path=population[i];
        }
    }
}
