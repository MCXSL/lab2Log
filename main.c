#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <process.h>
#include <windows.h>

typedef struct {
    int parallel;
    LARGE_INTEGER frequency;
} BenchmarkTimer;

typedef struct {
    clock_t clock_value;
    LARGE_INTEGER counter;
} TimePoint;

static TimePoint read_time(const BenchmarkTimer *timer) {
    TimePoint point = {0};
    if (timer->parallel) {
        if (!QueryPerformanceCounter(&point.counter)) {
            fputs("Не удалось прочитать таймер.\n", stderr);
            ExitProcess(EXIT_FAILURE);
        }
    } else {
        point.clock_value = clock();
    }
    return point;
}

static double seconds(const BenchmarkTimer *timer, TimePoint started, TimePoint finished) {
    if (timer->parallel)
        return (double)(finished.counter.QuadPart - started.counter.QuadPart) /
               (double)timer->frequency.QuadPart;
    return (double)(finished.clock_value - started.clock_value) / CLOCKS_PER_SEC;
}

static void fill_matrix(int *matrix, size_t n) {
    size_t count = n * n;
    for (size_t i = 0; i < count; ++i)
        matrix[i] = rand() % 100 + 1;
}


static void multiply_rows(const int *a, const int *b, int *c, size_t n,
                          size_t first_row, size_t last_row) {
    for (size_t i = first_row; i < last_row; ++i) {
        for (size_t j = 0; j < n; ++j) {
            int64_t sum = 0;
            for (size_t r = 0; r < n; ++r)
                sum += (int64_t)a[i * n + r] * b[r * n + j];
            c[i * n + j] = (int)sum;
        }
    }
}

static void multiply(const int *a, const int *b, int *c, size_t n) {
    multiply_rows(a, b, c, n, 0, n);
}

typedef struct {
    const int *a;
    const int *b;
    int *c;
    size_t n;
    size_t first_row;
    size_t last_row;
} MultiplyTask;

static unsigned __stdcall multiply_worker(void *argument) {
    const MultiplyTask *task = argument;
    multiply_rows(task->a, task->b, task->c, task->n, task->first_row, task->last_row);
    return 0;
}

static size_t multiply_parallel(const int *a, const int *b, int *c, size_t n,
                                size_t requested_threads) {
    size_t thread_count = requested_threads < n ? requested_threads : n;
    if (thread_count <= 1) {
        multiply(a, b, c, n);
        return 1;
    }

    size_t worker_count = thread_count - 1;
    HANDLE *handles = NULL;
    MultiplyTask *tasks = NULL;
    if (worker_count <= SIZE_MAX / sizeof(*handles) &&
        worker_count <= SIZE_MAX / sizeof(*tasks)) {
        handles = malloc(worker_count * sizeof(*handles));
        tasks = malloc(worker_count * sizeof(*tasks));
    }
    if (!handles || !tasks) {
        free(handles); free(tasks);
        fputs("Не удалось выделить память для потоков; используется один поток.\n", stderr);
        multiply(a, b, c, n);
        return 1;
    }

    size_t rows_per_thread = n / thread_count;
    size_t extra_rows = n % thread_count;
    size_t next_row = 0;
    size_t started = 0;
    for (size_t t = 0; t < worker_count; ++t) {
        tasks[t] = (MultiplyTask){a, b, c, n, next_row,
                                 next_row + rows_per_thread + (t < extra_rows)};
        uintptr_t handle = _beginthreadex(NULL, 0, multiply_worker, &tasks[t], 0, NULL);
        if (!handle) {
            fputs("Не удалось создать очередной поток; оставшиеся строки вычисляет основной поток.\n",
                  stderr);
            break;
        }
        handles[started++] = (HANDLE)handle;
        next_row = tasks[t].last_row;
    }

    multiply_rows(a, b, c, n, next_row, n);
    for (size_t t = 0; t < started; ++t) {
        if (WaitForSingleObject(handles[t], INFINITE) != WAIT_OBJECT_0) {
            fprintf(stderr, "Не удалось дождаться потока: код Windows %lu.\n",
                    (unsigned long)GetLastError());
            ExitProcess(EXIT_FAILURE);
        }
        CloseHandle(handles[t]);
    }
    free(tasks); free(handles);
    return started + 1;
}

static void print_usage(const char *program) {
    printf("Использование:\n"
           "  %s              последовательный режим (clock)\n"
           "  %s --parallel   потоки по числу логических процессоров\n"
           "  %s --threads N  задать положительное число потоков\n"
           "Время многопоточного режима измеряется QueryPerformanceCounter.\n",
           program, program, program);
}

static int parse_thread_count(const char *text, size_t *count) {
    if (!text[0]) return 0;
    for (const char *p = text; *p; ++p)
        if (*p < '0' || *p > '9') return 0;
    char *end;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno == ERANGE || *end || value == 0 || value > SIZE_MAX) return 0;
    *count = (size_t)value;
    return 1;
}

int main(int argc, char **argv) {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    size_t n;
    size_t requested_threads = 1;
    BenchmarkTimer timer = {0};
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--parallel") == 0) {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        requested_threads = info.dwNumberOfProcessors ? info.dwNumberOfProcessors : 1;
        timer.parallel = 1;
    } else if (argc == 3 && strcmp(argv[1], "--threads") == 0 &&
               parse_thread_count(argv[2], &requested_threads)) {
        timer.parallel = 1;
    } else if (argc != 1) {
        fputs("Неверные параметры запуска.\n", stderr);
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (timer.parallel && (!QueryPerformanceFrequency(&timer.frequency) ||
                           timer.frequency.QuadPart <= 0)) {
        fputs("Высокоточный таймер недоступен.\n", stderr);
        return EXIT_FAILURE;
    }

    srand((unsigned)time(NULL));
    puts("Умножение квадратных матриц");
    if (timer.parallel)
        printf("Многопоточный режим: до %zu потоков, включая основной.\n", requested_threads);
    puts("Вводите размер матрицы. Для завершения введите 0.");
    if (timer.parallel) {
        puts("\n    N | Общее время, с | Умножение, с | Контрольная сумма | Потоки");
        puts("------+----------------+--------------+------------------+-------");
    } else {
        puts("\n    N | Общее время, с | Умножение, с | Контрольная сумма");
        puts("------+----------------+--------------+------------------");
    }

    for (;;) {
        printf("\nРазмер N: ");
        int input_result = scanf("%zu", &n);
        if (input_result == EOF)
            break;
        if (input_result != 1) {
            puts("Ошибка: введите целое неотрицательное число.");
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) { }
            continue;
        }
        if (n == 0)
            break;
        if (n > SIZE_MAX / n || n * n > SIZE_MAX / sizeof(int)) {
            fprintf(stderr, "Недопустимый размер: %zu\n", n);
            continue;
        }

        size_t bytes = n * n * sizeof(int);
        TimePoint total_start = read_time(&timer);
        int *a = malloc(bytes), *b = malloc(bytes), *c = malloc(bytes);
        if (!a || !b || !c) {
            fprintf(stderr, "Не удалось выделить память для трёх матриц %zux%zu (%.2f ГиБ).\n", n, n,
                    3.0 * (double)bytes / (1024.0 * 1024.0 * 1024.0));
            free(a); free(b); free(c);
            continue;
        }
        fill_matrix(a, n);
        fill_matrix(b, n);
        TimePoint multiplication_start = read_time(&timer);
        size_t used_threads = 1;
        if (timer.parallel)
            used_threads = multiply_parallel(a, b, c, n, requested_threads);
        else
            multiply(a, b, c, n);
        TimePoint multiplication_end = read_time(&timer);
        TimePoint total_end = read_time(&timer);

        /* A checksum makes the result observable and prevents dead-code removal. */
        int64_t checksum = (int64_t)c[0] + c[n * n - 1];
        printf("%5zu | %14.6f | %12.6f | %16lld", n,
               seconds(&timer, total_start, total_end),
               seconds(&timer, multiplication_start, multiplication_end), (long long)checksum);
        if (timer.parallel) printf(" | %zu", used_threads);
        putchar('\n');
        free(a); free(b); free(c);
    }
    puts("Работа программы завершена.");
    return 0;
}
