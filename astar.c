#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "astar.h"
#include "pool.h"
#include "genetic.h"
#include "config.h"

static inline int idx3(int x,int y,int z){
    return z*config.grid_x*config.grid_y + y*config.grid_x + x;
}
static inline int manhattan(Coord a, Coord b){
    return abs(a.x-b.x)+abs(a.y-b.y)+abs(a.z-b.z);
}

static int astar_one(Coord start, Coord goal, Coord *out, int *out_len, int max_len){
    int N = config.grid_x*config.grid_y*config.grid_z;

    int *g = (int*)malloc((size_t)N*sizeof(int));
    int *f = (int*)malloc((size_t)N*sizeof(int));
    int *came = (int*)malloc((size_t)N*sizeof(int));
    unsigned char *open = (unsigned char*)calloc((size_t)N,1);
    unsigned char *closed = (unsigned char*)calloc((size_t)N,1);
    if(!g||!f||!came||!open||!closed){ fprintf(stderr,"alloc failed\n"); exit(1); }

    for(int i=0;i<N;i++){ g[i]=INT_MAX/4; f[i]=INT_MAX/4; came[i]=-1; }

    int s = idx3(start.x,start.y,start.z);
    int t = idx3(goal.x,goal.y,goal.z);

    g[s]=0;
    f[s]=manhattan(start,goal);
    open[s]=1;

    Coord dirs[6]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};

    while(1){
        int cur=-1;
        int bestf=INT_MAX;
        for(int i=0;i<N;i++){
            if(open[i] && f[i]<bestf){ bestf=f[i]; cur=i; }
        }
        if(cur==-1) break;

        if(cur==t){
            int count=0;
            int x=cur;
            while(x!=-1 && count<max_len){
                int z = x/(config.grid_x*config.grid_y);
                int rem = x%(config.grid_x*config.grid_y);
                int y = rem/config.grid_x;
                int xx = rem%config.grid_x;
                out[count++] = (Coord){xx,y,z};
                x = came[x];
            }
            for(int i=0;i<count/2;i++){
                Coord tmp=out[i];
                out[i]=out[count-1-i];
                out[count-1-i]=tmp;
            }
            *out_len=count;

            free(g);free(f);free(came);free(open);free(closed);
            return 1;
        }

        open[cur]=0;
        closed[cur]=1;

        int cz = cur/(config.grid_x*config.grid_y);
        int rem = cur%(config.grid_x*config.grid_y);
        int cy = rem/config.grid_x;
        int cx = rem%config.grid_x;

        for(int d=0;d<6;d++){
            Coord nb={cx+dirs[d].x, cy+dirs[d].y, cz+dirs[d].z};
            if(!is_valid(nb)) continue;
            if(get_cell(nb)==OBSTACLE) continue;

            int ni = idx3(nb.x,nb.y,nb.z);
            if(closed[ni]) continue;

            int tentative = g[cur]+1;
            if(!open[ni] || tentative < g[ni]){
                came[ni]=cur;
                g[ni]=tentative;
                f[ni]=tentative + manhattan(nb,goal);
                open[ni]=1;
            }
        }
    }

    free(g);free(f);free(came);free(open);free(closed);
    return 0;
}

static void sort_survivors_by_priority(int *order){
    for(int i=0;i<config.num_survivors;i++) order[i]=i;

    for(int i=0;i<config.num_survivors;i++){
        for(int j=0;j<config.num_survivors-1;j++){
            int a=order[j], b=order[j+1];
            if(shared->survivor_priority[b] > shared->survivor_priority[a]){
                int tmp=order[j]; order[j]=order[j+1]; order[j+1]=tmp;
            }
        }
    }
}

int astar_build_baseline(Path *out_path, Coord start){
    out_path->length=0;
    out_path->fitness=0;
    out_path->survivors_reached=0;
    out_path->priority_sum=0;
    out_path->coverage=0;

    if(config.num_survivors==0){
        out_path->genes[out_path->length++]=start;
        calculate_fitness(out_path);
        return 1;
    }

    int *order = (int*)malloc((size_t)config.num_survivors*sizeof(int));
    if(!order) exit(1);
    sort_survivors_by_priority(order);

    Coord current=start;
    out_path->genes[out_path->length++]=current;

    Coord *tmp = (Coord*)malloc((size_t)MAX_PATH_LENGTH*sizeof(Coord));
    if(!tmp) exit(1);

    for(int k=0;k<config.num_survivors;k++){
        Coord goal = shared->survivors[ order[k] ];
        int seg_len=0;

        if(!astar_one(current, goal, tmp, &seg_len, MAX_PATH_LENGTH)) {
            continue; // unreachable -> skip
        }

        for(int i=1;i<seg_len && out_path->length<MAX_PATH_LENGTH;i++){
            out_path->genes[out_path->length++] = tmp[i];
        }

        current = goal;
    }

    free(order);
    free(tmp);

    calculate_fitness(out_path);
    return (out_path->length>0);
}
