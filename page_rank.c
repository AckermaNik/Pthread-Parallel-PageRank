#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <math.h>

#define DAMPING_FACTOR 0.85
#define ITERATIONS 50
#define RUNS 5


typedef enum {
    false = 0,
    true = 1
} bool;

typedef struct {
    long *in_edges;
    int out_deg; //out_degree
    int in_deg; // degree of incoming edges
} Node;

Node *graph;
pthread_barrier_t barrier;
float *pagerank, *new_pagerank;
int nodes;
pthread_mutex_t lock;
int next_node = -1;  // shared counter

double compute_standard_deviation(double times[], int runs, double mean) {
    double sum = 0.0;
    int i;
    for ( i = 0; i < runs; i++) {
        sum += pow(times[i] - mean, 2);
    }
    return sqrt(sum / runs);
}


int read_input(char* f_name)
{
    FILE *file = fopen(f_name, "r");
    char line[256];
    long src, dst, max_node=-1;
    int i,j;

    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    while (fgets(line, sizeof(line), file)) {
        // Skip comment lines
        if (line[0] == '#') continue;

        // Parse the first two numbers (ignoring anything after them)
        if (sscanf(line, "%ld %ld", &src, &dst) >= 2) {
            if (src > max_node){ 
                max_node = src; 
            }
            if (dst > max_node){ 
                max_node = dst;
            }
        }else if (sscanf(line, "%ld", &src) >= 1){
            if (src > max_node){ 
                max_node = src; 
            }
        }
    }

    nodes=max_node;
   
    pagerank = malloc((max_node + 1)* sizeof(float));
    new_pagerank = malloc((max_node + 1)* sizeof(float));
    graph = malloc((max_node + 2)* sizeof(Node));

    if (!graph) {
        perror("Memory allocation failed for graph");
        exit(EXIT_FAILURE);
    }

    for(i=0;i<=nodes;i++){
        graph[i].in_edges=NULL; // to be sure
        graph[i].out_deg=0;
        graph[i].in_deg=0;
    }

    rewind(file);

    while (fgets(line, sizeof(line), file)) {
            // Skip comment lines
            if (line[0] == '#') continue;

            // Parse the first two numbers (ignoring anything after them)
            if (sscanf(line, "%ld %ld", &src, &dst) >= 2) {
                graph[dst].in_deg++;
            }
        }

        for(i=0;i<=nodes;i++){
            graph[i].in_edges = malloc(graph[i].in_deg  * sizeof(long));

            if (graph[i].in_edges==NULL) {
                perror("Error reallocating memory for in_edges");
                exit(EXIT_FAILURE);
            }
        }

        rewind(file);

        for(i=0;i<=nodes;i++){
            graph[i].in_deg=0;
        }

        while (fgets(line, sizeof(line), file)) {
            // Skip comment lines
            if (line[0] == '#') continue;

            // Parse the first two numbers (ignoring anything after them)
            if (sscanf(line, "%ld %ld", &src, &dst) >= 2) {

                //printf("N: %ld\n",src);
                graph[dst].in_edges[graph[dst].in_deg] = src;
                graph[dst].in_deg++;
                graph[src].out_deg++;
            }
        }

    
    // for(i=0;i<=nodes;i++){
    //     printf("NODE: %d   Neighbor: ",i);
    //     for(j=0;j<graph[i].in_deg;j++){
    //         printf("%ld",graph[i].in_edges[j]);
    //         printf("\t");
    //     }
    //     printf("\n");
    // }

    fclose(file);
    return 0;
}


int write_output(char* f_name){
    int i;
    FILE *file = fopen(f_name, "w");

    if (!file) {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i <= nodes; i++) {      
            fprintf(file, "%d, %.2f\n", i, new_pagerank[i]);
        
        }

    fclose(file);
}


void *compute_in_parallel(void *args)
{
    int i,j,edge,src,iter;
    float contribution;

    for (iter = 0; iter < ITERATIONS; iter++) {

        pthread_barrier_wait(&barrier);

        while (1) {
           
            pthread_mutex_lock(&lock);
                next_node++;
                i = next_node;

                // if all nodes are processed...
                if (i<=nodes){
                    contribution=0;
                    new_pagerank[i] = 0.15;
                }
            pthread_mutex_unlock(&lock);

            if (i>nodes) break;

                for(edge=0; edge < graph[i].in_deg; edge++){
                    //printf("edge:%ld out_deg:%d \n",graph[i].in_edges[edge],graph[graph[i].in_edges[edge]].out_deg);
                    contribution += pagerank[graph[i].in_edges[edge]] / graph[graph[i].in_edges[edge]].out_deg;
                }

            new_pagerank[i] += DAMPING_FACTOR * contribution;
        }
        
        
        pthread_barrier_wait(&barrier);  // Ensure all threads have completed computation

        
        pthread_mutex_lock(&lock);
            if (next_node >= nodes) {
                memcpy(pagerank, new_pagerank, nodes * sizeof(float));
            }
            next_node=-1;
        pthread_mutex_unlock(&lock); 

        pthread_barrier_wait(&barrier);
    }
}

void run_pagerank(int num_threads)
{
    pthread_t threads[num_threads];
    int i;


    pthread_barrier_init(&barrier, NULL, num_threads);

    for ( i = 0; i <= nodes; i++) {
        pagerank[i] = 1.0;
    }

    for (i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, compute_in_parallel, NULL);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_barrier_destroy(&barrier);
}

int main(int argc, char* argv[])
{
    clock_t start, end;
    int i;
    double times[RUNS],elapsed,sum = 0.0,average;  


    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <num_threads>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int num_threads = atoi(argv[2]);
    if (num_threads <= 0) {
        fprintf(stderr,"Invalid number of threads\n");
        return EXIT_FAILURE;
    }

    read_input(argv[1]);

    for ( i = 0; i < RUNS; i++) {

            start = clock();  // Start time
        
            printf("Run:%d  Starting...\n",i);
            run_pagerank(num_threads);
            //printf("after\n");

            end = clock();  // End time

            elapsed = (double)(end - start) / CLOCKS_PER_SEC;
            times[i] = elapsed;
            sum += elapsed;
            printf("Run %d: %f seconds\n", i, elapsed);
    }

    write_output("pagerank.csv");
  
    average=sum/RUNS;
    printf("\nAverage execution time: %f seconds\n", average);
    printf("Standard deviation: %f seconds\n", compute_standard_deviation(times, RUNS, average));

    for (i = 0; i <= nodes; i++) {
        if(graph[i].in_deg!=0){
            free(graph[i].in_edges);
        }
    }

    free(graph);
    free(pagerank);
    free(new_pagerank);

    return 0;
}
