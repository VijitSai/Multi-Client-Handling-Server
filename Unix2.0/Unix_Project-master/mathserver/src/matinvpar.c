#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <pthread.h>
#define MAX_SIZE 4096
typedef double matrix[MAX_SIZE][MAX_SIZE];
int N; /* matrix size */
int maxnum; /* max number of element*/
char* Init; /* matrix init type */
char* result_file; /* result file name */
int PRINT; /* print switch */
matrix A; /* matrix A */
matrix I = {0.0};  /* The A inverse matrix, which will be initialized to the 
identity matrix */
pthread_t div_threads[4];
pthread_t sub_threads[4];
pthread_barrier_t div_start_barrier, div_end_barrier, sub_start_barrier, sub_end_barrier;

double pivalue; // pivot value
/* forward declarations */
void find_inverse(void);
void Init_Matrix(void);
void Print_Matrix(matrix M, char name[]);
void Init_Default(void);
int Read_Options(int, char**);

FILE *fp;
int
main(int argc, char** argv)
{
    int i, timestart, timeend, iter;
    Init_Default(); /* Init default values */
    Read_Options(argc, argv); /* Read arguments */
    fp = fopen(result_file, "w");
    Init_Matrix(); /* Init the matrix */
    find_inverse();
    if (PRINT == 1)
    {
        Print_Matrix(A, "End: Input");
        Print_Matrix(I, "Inversed");
    }
    fclose(fp);
}

void * div_step(void* arg)
{
    int col;
    int* id_ptr = (int*)arg;
    int id = *id_ptr;
    int loc_pivot = 0;
    while(loc_pivot < N)
    {
        pthread_barrier_wait(&div_start_barrier);
        for (col = 0+id; col < N; col+=4)
        {
            A[loc_pivot][col] = A[loc_pivot][col]/pivalue;
            I[loc_pivot][col] = I[loc_pivot][col]/pivalue;
        }
        loc_pivot++;
        pthread_barrier_wait(&div_end_barrier);
    }
    
}

void* sub_step(void* arg)
{

    double multiplier;
    int row,col;
    int* id_ptr = (int*)arg;
    int id = *id_ptr;
    int loc_pivot = 0;
    while(loc_pivot < N)
    {
        pthread_barrier_wait(&sub_start_barrier);
        for (row = 0+id; row < N; row+=4)
        {
            multiplier = A[row][loc_pivot];
            if (row != loc_pivot) // Perform elimination on all except the current pivot row 
            {
                for (col = 0; col < N; col++)
                {
                    A[row][col] = A[row][col] - A[loc_pivot][col] * multiplier; /* Elimination step on A */
                    I[row][col] = I[row][col] - I[loc_pivot][col] * multiplier; /* Elimination step on I */
                }      
                assert(A[row][loc_pivot] == 0.0);
            }
        }
        loc_pivot++;
        pthread_barrier_wait(&sub_end_barrier);
    }

}

void find_inverse()
{
    int row, col, p; // 'p' stands for pivot (numbered from 0 to N-1)

    pthread_barrier_init(&div_start_barrier, NULL, 4+1);
    pthread_barrier_init(&div_end_barrier, NULL, 4+1);
    pthread_barrier_init(&sub_start_barrier, NULL, 4+1);
    pthread_barrier_init(&sub_end_barrier, NULL, 4+1);

    for(int i=0; i<4; i++)
    {
        int* id = (int*)malloc(sizeof(int));
        *id = i;
        pthread_create(&div_threads[i], NULL, div_step, (void*)id);
        pthread_create(&sub_threads[i], NULL, sub_step, (void*)id);
    }

    /* Bringing the matrix A to the identity form */
    for (p = 0; p < N; p++) { /* Outer loop */

        pivalue = A[p][p];
        pthread_barrier_wait(&div_start_barrier);
        // 
        pthread_barrier_wait(&div_end_barrier);
        assert(A[p][p] == 1.0);

        pthread_barrier_wait(&sub_start_barrier);

        pthread_barrier_wait(&sub_end_barrier);
    }

    for (int i=0; i<4; i++)
    {
        pthread_join(div_threads[i], NULL);
        pthread_join(sub_threads[i], NULL);
    }

    pthread_barrier_destroy(&div_start_barrier);
    pthread_barrier_destroy(&div_end_barrier);
    pthread_barrier_destroy(&sub_start_barrier);
    pthread_barrier_destroy(&sub_end_barrier);

}
void
Init_Matrix()
{
    int row, col;
    // Set the diagonal elements of the inverse matrix to 1.0
    // So that you get an identity matrix to begin with
    for (row = 0; row < N; row++) {
        for (col = 0; col < N; col++) {
            if (row == col)
                I[row][col] = 1.0;
        }
    }
    fprintf(fp, "\nsize      = %dx%d ", N, N);
    fprintf(fp, "\nmaxnum    = %d \n", maxnum);
    fprintf(fp, "Init   = %s \n", Init);
    fprintf(fp, "Initializing matrix...");
    if (strcmp(Init, "rand") == 0) {
        for (row = 0; row < N; row++) {
            for (col = 0; col < N; col++) {
                if (row == col) /* diagonal dominance */
                    A[row][col] = (double)(rand() % maxnum) + 5.0;
                else
                    A[row][col] = (double)(rand() % maxnum) + 1.0;
            }
        }
    }
    if (strcmp(Init, "fast") == 0) {
        for (row = 0; row < N; row++) {
            for (col = 0; col < N; col++) {
                if (row == col) /* diagonal dominance */
                    A[row][col] = 5.0;
                else
                    A[row][col] = 2.0;
            }
        }
    }
    fprintf(fp, "done \n\n");
    if (PRINT == 1)
    {
        Print_Matrix(A, "Begin: Input");
        Print_Matrix(I, "Begin: Inverse");
    }
}
void
Print_Matrix(matrix M, char name[])
{
    int row, col;
    fprintf(fp, "%s Matrix:\n", name);
    for (row = 0; row < N; row++) {
        for (col = 0; col < N; col++){
            fprintf(fp, " %f", M[row][col]);
        }
        fprintf(fp, "\n");
    }
    fprintf(fp, "\n\n");
}
void
Init_Default()
{
    N = 5;
    Init = "fast";
    maxnum = 15.0;
    PRINT = 1;
}
int
Read_Options(int argc, char** argv)
{
    char* prog;
    prog = *argv;
    while (++argv, --argc > 0)
        if (**argv == '-')
            switch (*++ * argv) {
            case 'n':
                --argc;
                N = atoi(*++argv);
                break;
            case 'h':
                printf("\nHELP: try matinv -u \n\n");
                exit(0);
                break;
            case 'u':
                printf("\nUsage: matinv [-n problemsize]\n");
                printf("           [-D] show default values \n");
                printf("           [-h] help \n");
                printf("           [-I init_type] fast/rand \n");
                printf("           [-m maxnum] max random no \n");
                printf("           [-P print_switch] 0/1 \n");
                exit(0);
                break;
            case 'D':
                printf("\nDefault:  n         = %d ", N);
                printf("\n          Init      = rand");
                printf("\n          maxnum    = 5 ");
                printf("\n          P         = 0 \n\n");
                exit(0);
                break;
            case 'I':
                --argc;
                Init = *++argv;
                break;
            case 'm':
                --argc;
                maxnum = atoi(*++argv);
                break;
            case 'P':
                --argc;
                PRINT = atoi(*++argv);
                break;
            
            case 'r':
                --argc;
                result_file = *++argv;
                break;
            default:
                printf("%s: ignored option: -%s\n", prog, *argv);
                printf("HELP: try %s -u \n\n", prog);
                break;
            }
}