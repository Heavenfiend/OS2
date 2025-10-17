#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define pthread_t HANDLE
#define pthread_create(handle, attr, func, arg) \
    (*(handle) = CreateThread(NULL, 0, func, arg, 0, NULL), (*(handle) == NULL ? -1 : 0))
#define pthread_join(handle, status) (WaitForSingleObject(handle, INFINITE) == WAIT_FAILED ? -1 : 0)
#define pthread_exit(val) ExitThread(0)
#define sleep_ms(ms) Sleep(ms)
typedef struct { int id; int* arr; int lo; int n; int dir; int max_threads; } thread_data_t;
DWORD WINAPI merge_sort_thread(LPVOID arg);
DWORD WINAPI bitonic_sort_thread(LPVOID arg);
#else
#include <pthread.h>
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
typedef struct { int id; int* arr; int lo; int n; int dir; int max_threads; } thread_data_t;
void* merge_sort_thread(void* arg);
void* bitonic_sort_thread(void* arg);
#endif

int logical_cores = 0;

void print_array(int* arr, int n) {
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");
}

void swap(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

void bitonic_compare(int* arr, int idx1, int idx2, int dir) {
    if ((arr[idx1] > arr[idx2]) == dir) {
        swap(&arr[idx1], &arr[idx2]);
    }
}
void bitonic_merge(int* arr, int lo, int n, int dir) {
    if (n <= 1) return;
    int m = 1;
    while (m < n) m <<= 1;
    m >>= 1;
    for (int i = lo; i < lo + n - m; i++) {
        bitonic_compare(arr, i, i + m, dir);
    }
    bitonic_merge(arr, lo, m, dir);
    bitonic_merge(arr, lo + m, n - m, dir);
}

void bitonic_sort_seq(int* arr, int lo, int n, int dir) {
    if (n <= 1) return;
    int m = n / 2;
    bitonic_sort_seq(arr, lo, m, 1);
    bitonic_sort_seq(arr, lo + m, n - m, 0);
    bitonic_merge(arr, lo, n, dir);
}

int active_threads = 0;
#ifdef _WIN32
CRITICAL_SECTION thread_count_mutex;
#else
pthread_mutex_t thread_count_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

void* merge_sort_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    bitonic_merge(data->arr, data->lo, data->n, data->dir);
#ifdef _WIN32
    EnterCriticalSection(&thread_count_mutex);
#else
    pthread_mutex_lock(&thread_count_mutex);
#endif
    active_threads--;
#ifdef _WIN32
    LeaveCriticalSection(&thread_count_mutex);
#else
    pthread_mutex_unlock(&thread_count_mutex);
#endif
    return NULL;
}

void* bitonic_sort_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    if (data->n <= 1) {
#ifdef _WIN32
        EnterCriticalSection(&thread_count_mutex);
#else
        pthread_mutex_lock(&thread_count_mutex);
#endif
        active_threads--;
#ifdef _WIN32
        LeaveCriticalSection(&thread_count_mutex);
#else
        pthread_mutex_unlock(&thread_count_mutex);
#endif
        return NULL;
    }

    int m = data->n / 2;
    pthread_t t1, t2;

    thread_data_t d1 = {0, data->arr, data->lo, m, 1, data->max_threads};
    thread_data_t d2 = {0, data->arr, data->lo + m, data->n - m, 0, data->max_threads};

#ifdef _WIN32
    EnterCriticalSection(&thread_count_mutex);
#else
    pthread_mutex_lock(&thread_count_mutex);
#endif
    if (active_threads >= data->max_threads) {
#ifdef _WIN32
        LeaveCriticalSection(&thread_count_mutex);
#else
        pthread_mutex_unlock(&thread_count_mutex);
#endif
        bitonic_sort_seq(data->arr, data->lo, m, 1);
        bitonic_sort_seq(data->arr, data->lo + m, data->n - m, 0);
    } else {
        active_threads += 2;
#ifdef _WIN32
        LeaveCriticalSection(&thread_count_mutex);
#else
        pthread_mutex_unlock(&thread_count_mutex);
#endif
        pthread_create(&t1, NULL, bitonic_sort_thread, &d1);
        pthread_create(&t2, NULL, bitonic_sort_thread, &d2);
        pthread_join(t1, NULL);
        pthread_join(t2, NULL);
    }

    pthread_t t3;
    thread_data_t d3 = {0, data->arr, data->lo, data->n, data->dir, data->max_threads};

#ifdef _WIN32
    EnterCriticalSection(&thread_count_mutex);
#else
    pthread_mutex_lock(&thread_count_mutex);
#endif
    if (active_threads < data->max_threads) {
        active_threads++;
#ifdef _WIN32
        LeaveCriticalSection(&thread_count_mutex);
#else
        pthread_mutex_unlock(&thread_count_mutex);
#endif
        pthread_create(&t3, NULL, merge_sort_thread, &d3);
        pthread_join(t3, NULL);
    } else {
#ifdef _WIN32
        LeaveCriticalSection(&thread_count_mutex);
#else
        pthread_mutex_unlock(&thread_count_mutex);
#endif
        bitonic_merge(data->arr, data->lo, data->n, data->dir);
    }

#ifdef _WIN32
    EnterCriticalSection(&thread_count_mutex);
#else
    pthread_mutex_lock(&thread_count_mutex);
#endif
    active_threads--;
#ifdef _WIN32
    LeaveCriticalSection(&thread_count_mutex);
#else
    pthread_mutex_unlock(&thread_count_mutex);
#endif
    return NULL;
}

void generate_random_array(int* arr, int n) {
    srand(time(NULL));
    for (int i = 0; i < n; i++) {
        arr[i] = rand() % 10000;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s --threads N\n", argv[0]);
        return 1;
    }
    int num_threads = atoi(argv[2]);

    int n = 1000000;
    int* arr = (int*)malloc(n * sizeof(int));
    generate_random_array(arr, n);

    int* seq_arr = (int*)malloc(n * sizeof(int));
    memcpy(seq_arr, arr, n * sizeof(int));

    clock_t start = clock();
    bitonic_sort_seq(seq_arr, 0, n, 1);
    clock_t end = clock();
    double seq_time = ((double)(end - start)) / CLOCKS_PER_SEC * 1000; // в миллисекундах
    printf("Sequential time: %.2f ms\n", seq_time);

    int* par_arr = (int*)malloc(n * sizeof(int));
    memcpy(par_arr, arr, n * sizeof(int));

#ifdef _WIN32
    InitializeCriticalSection(&thread_count_mutex);
#endif

    active_threads = 1;
    pthread_t main_thread;
    thread_data_t data = {0, par_arr, 0, n, 1, num_threads};

    start = clock();
    pthread_create(&main_thread, NULL, bitonic_sort_thread, &data);
    pthread_join(main_thread, NULL);
    end = clock();

    double par_time = ((double)(end - start)) / CLOCKS_PER_SEC * 1000;
    printf("Parallel time with %d threads: %.2f ms\n", num_threads, par_time);

    printf("Speedup: %.2f\n", seq_time / par_time);
    printf("Efficiency: %.2f\n", (seq_time / par_time) / num_threads);

    free(arr);
    free(seq_arr);
    free(par_arr);

#ifdef _WIN32
    DeleteCriticalSection(&thread_count_mutex);
#endif
    return 0;
}
