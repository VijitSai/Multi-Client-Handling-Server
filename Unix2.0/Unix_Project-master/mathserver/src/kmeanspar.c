#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>

#define DEBUG 0

#define MAX_POINTS 4096
#define MAX_CLUSTERS 32

#define NUM_THREADS 2

typedef struct point
{
    float x;     // The x-coordinate of the point
    float y;     // The y-coordinate of the point
    int cluster; // The cluster that the point belongs to
} point;

int N;                                     // number of entries in the data
int k;                                     // number of centroids
char *file_name;                       // name of the file containing the data
char* result_file;                         // name of the file to write the results to
bool somechange, assign_cont, update_cont; // whether there is any change in the cluster assignment
point data[MAX_POINTS];                    // Data coordinates
point cluster[MAX_CLUSTERS];               // The coordinates of each cluster center (also called centroid)

bool somechanges[NUM_THREADS] = {false};

int count[MAX_CLUSTERS] = {0}; // Array to keep track of the number of points in each cluster
point temp[MAX_CLUSTERS] = {0.0};

pthread_t assign_threads[NUM_THREADS];
pthread_t update_threads[NUM_THREADS];
pthread_barrier_t assign_start_barrier, assign_end_barrier, update_start_barrier, update_end_barrier;
// pthread_barrier_t update_sum_start_barrier, update_sum_end_barrier;
pthread_barrier_t check_condition_barrier, at_start_barrier;
pthread_mutex_t update_temp_lock;

void read_data()
{
    N = 0;
    FILE *fp = fopen(file_name, "r");
    if (fp == NULL)
    {
        perror("Cannot open the file");
        exit(EXIT_FAILURE);
    }

    // Initialize points from the data file
    float temp;
    while (fscanf(fp, "%f %f", &data[N].x, &data[N].y) != EOF)
    {
        data[N].cluster = -1; 
        N++;
    }
    printf("Read the problem data!\n");
    // Initialize centroids randomly
    srand(0); // Setting 0 as the random number generation seed
    for (int i = 0; i < k; i++)
    {
        int r = rand() % N;
        cluster[i].x = data[r].x;
        cluster[i].y = data[r].y;
    }
    fclose(fp);
}

void read_args(int argc, char *argv[])
{
    char* prog;
    prog = *argv;
    while (++argv, --argc > 0)
        if (**argv == '-')
            switch (*++ * argv) {
                case 'k':
                    --argc;
                    k = atoi(*++argv);
                    break;
                case 'f':
                    --argc;
                    file_name = *++argv;
                    break;
                case 'r':
                    --argc;
                    result_file = *++argv;
                    break;
            }
}
int get_closest_centroid(int i, int k)
{
    /* find the nearest centroid */
    int nearest_cluster = -1;
    double xdist, ydist, dist, min_dist;
    min_dist = dist = INT_MAX;
    for (int c = 0; c < k; c++)
    { // For each centroid
        // Calculate the square of the Euclidean distance between that centroid and the point
        xdist = data[i].x - cluster[c].x;
        ydist = data[i].y - cluster[c].y;
        dist = xdist * xdist + ydist * ydist; // The square of Euclidean distance
        // printf("%.2lf \n", dist);
        if (dist <= min_dist)
        {
            min_dist = dist;
            nearest_cluster = c;
        }
    }
    // printf("-----------\n");
    return nearest_cluster;
}

void *assign_clusters_to_points_thread(void *arg)
{
    int *id_ptr = (int *)arg;
    int id = *id_ptr;
    int start = id * (N / NUM_THREADS);
    int end = (id + 1) * (N / NUM_THREADS);
    do
    {
        pthread_barrier_wait(&at_start_barrier);
        if (DEBUG)
            printf(" assign thread %d: start \n", id);
        pthread_barrier_wait(&assign_start_barrier);
        int old_cluster = -1, new_cluster = -1;
        for (int i = start; i < end; i++)
        { // For each data point
            old_cluster = data[i].cluster;
            new_cluster = get_closest_centroid(i, k);
            data[i].cluster = new_cluster; // Assign a cluster to the point i
            if (old_cluster != new_cluster)
            {
                somechanges[id] = true;
            }
        }
        // pthread_barrier_wait(&update_assign_cont_barrier);
        if (DEBUG)
            printf(" assign thread %d: end \n", id);
        pthread_barrier_wait(&assign_end_barrier);

        if (DEBUG)
            printf(" check condi assign thread %d: start \n", id);
        pthread_barrier_wait(&check_condition_barrier);

    } while (somechanges[0] || somechanges[1]);

    if (DEBUG)
        printf(" assign thread %d: exit \n", id);
    return NULL;
}

void *update_cluster_centers_thread(void *arg)
{
    point l_temp[MAX_CLUSTERS] = {0.0};
    int l_count[MAX_CLUSTERS] = {0};

    int *id_ptr = (int *)arg;
    int id = *id_ptr;
    int start = id * (N / NUM_THREADS);
    int end = (id + 1) * (N / NUM_THREADS);
    int c;
    do
    {
        for(int i = 0; i < k; i++){
            l_temp[i].x = 0.0;
            l_temp[i].y = 0.0;
            l_count[i] = 0;
        }
        pthread_barrier_wait(&at_start_barrier);
        if (DEBUG)
            printf(" update thread %d: start \n", id);
        pthread_barrier_wait(&update_start_barrier);
        // pthread_barrier_wait(&update_sum_start_barrier);
        for (int i = start; i < end; i++)
        { // For each data point
            c = data[i].cluster;
            l_count[c]++;
            l_temp[c].x += data[i].x;
            l_temp[c].y += data[i].y;
        }
        pthread_mutex_lock(&update_temp_lock);
        for (int i = 0; i < k; i++)
        {
            count[i] += l_count[i];
            temp[i].x += l_temp[i].x;
            temp[i].y += l_temp[i].y;
        }
        pthread_mutex_unlock(&update_temp_lock);

        if (DEBUG)
            printf(" update thread %d: end \n", id);
        pthread_barrier_wait(&update_end_barrier);

        if (DEBUG)
            printf(" check condi update thread %d: start \n", id);
        pthread_barrier_wait(&check_condition_barrier);

    } while (somechanges[0] || somechanges[1]);

    if (DEBUG)
        printf(" update thread %d: exit \n", id);
    return NULL;
}

int kmeans(int k)
{
    int iter = 0;
    pthread_barrier_init(&assign_start_barrier, NULL, NUM_THREADS + 1);
    pthread_barrier_init(&assign_end_barrier, NULL, NUM_THREADS + 1);
    pthread_barrier_init(&update_start_barrier, NULL, NUM_THREADS + 1);
    pthread_barrier_init(&update_end_barrier, NULL, NUM_THREADS + 1);
    pthread_barrier_init(&check_condition_barrier, NULL, 2 * NUM_THREADS + 1);
    pthread_barrier_init(&at_start_barrier, NULL, 2 * NUM_THREADS + 1);
    pthread_mutex_init(&update_temp_lock, NULL);

    for (int i = 0; i < NUM_THREADS; i++)
    {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&assign_threads[i], NULL, assign_clusters_to_points_thread, (void *)id);
        pthread_create(&update_threads[i], NULL, update_cluster_centers_thread, (void *)id);
    }

    do
    {
        iter++; // Keep track of number of iterations
        if (DEBUG)
            printf("iter %d \n", iter);

        pthread_barrier_wait(&at_start_barrier);
        for (int i = 0; i < NUM_THREADS; i++)
        {
            somechanges[i] = false;
        }

        if (DEBUG)
            printf(" main: assign start \n");
        pthread_barrier_wait(&assign_start_barrier);

        if (DEBUG)
            printf(" main: assign end \n");
        pthread_barrier_wait(&assign_end_barrier);

        // initialize count and temp
        if (DEBUG)
            printf(" main: init count and temp \n");
        for (int i = 0; i < k; i++)
        {
            count[i] = 0;
            temp[i].x = 0.0;
            temp[i].y = 0.0;
        }

        if (DEBUG)
            printf(" main: update start \n");
        pthread_barrier_wait(&update_start_barrier);

        if (DEBUG)
            printf(" main: update end \n");
        pthread_barrier_wait(&update_end_barrier);
        if (DEBUG)
            printf(" main: update cluster start \n");
        for (int i = 0; i < k; i++)
        {
            cluster[i].x = temp[i].x / count[i];
            cluster[i].y = temp[i].y / count[i];
        }
        if (DEBUG)
            printf(" main: update cluster end \n");

        if (DEBUG)
            printf(" main: check condition start \n");
        pthread_barrier_wait(&check_condition_barrier);
        if (DEBUG)
            printf("%d %d \n", somechanges[0], somechanges[1]);

    } while (somechanges[0] || somechanges[1]);

    if (DEBUG)
        printf(" main: exit \n");

    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_join(assign_threads[i], NULL);
        pthread_join(update_threads[i], NULL);
    }
    pthread_barrier_destroy(&assign_start_barrier);
    pthread_barrier_destroy(&assign_end_barrier);
    pthread_barrier_destroy(&update_start_barrier);
    pthread_barrier_destroy(&update_end_barrier);
    pthread_barrier_destroy(&at_start_barrier);
    pthread_barrier_destroy(&check_condition_barrier);

    pthread_mutex_destroy(&update_temp_lock);

    printf("Number of iterations taken = %d\n", iter);
    printf("Computed cluster numbers successfully!\n");
}

void write_results()
{
    FILE *fp = fopen(result_file, "w");
    if (fp == NULL)
    {
        perror("Cannot open the file");
        exit(EXIT_FAILURE);
    }
    else
    {
        for (int i = 0; i < N; i++)
        {
            fprintf(fp, "%.2f %.2f %d\n", data[i].x, data[i].y, data[i].cluster);
        }
    }
    printf("Wrote the results to a file!\n");
}

int main(int argc, char* argv[])
{
    read_args(argc, argv);
    read_data();
    kmeans(k);
    write_results();
}