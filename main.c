#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <windows.h>


static double seconds(clock_t started, clock_t finished) {
    return (double)(finished - started) / CLOCKS_PER_SEC;
}

static void fill_matrix(int *matrix, size_t n) {
    size_t count = n * n;
    for (size_t i = 0; i < count; ++i)
        matrix[i] = rand() % 100 + 1;
}


static void multiply(const int *a, const int *b, int *c, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            int64_t sum = 0;
            for (size_t r = 0; r < n; ++r)
                sum += (int64_t)a[i * n + r] * b[r * n + j];
            c[i * n + j] = (int)sum;
        }
    }
}

int main(void) {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    size_t n;

    srand((unsigned)time(NULL));
    puts("Умножение квадратных матриц");
    puts("Вводите размер матрицы. Для завершения введите 0.");
    puts("\n    N | Общее время, с | Умножение, с | Контрольная сумма");
    puts("------+----------------+--------------+------------------");

    for (;;) {
        printf("\nРазмер N: ");
        if (scanf("%zu", &n) != 1) {
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
        clock_t total_start = clock();
        int *a = malloc(bytes), *b = malloc(bytes), *c = malloc(bytes);
        if (!a || !b || !c) {
            fprintf(stderr, "Не удалось выделить память для трёх матриц %zux%zu (%.2f ГиБ).\n", n, n,
                    3.0 * (double)bytes / (1024.0 * 1024.0 * 1024.0));
            free(a); free(b); free(c);
            continue;
        }
        fill_matrix(a, n);
        fill_matrix(b, n);
        clock_t multiplication_start = clock();
        multiply(a, b, c, n);
        clock_t multiplication_end = clock();
        clock_t total_end = clock();

        /* A checksum makes the result observable and prevents dead-code removal. */
        int64_t checksum = c[0] + c[n * n - 1];
        printf("%5zu | %14.6f | %12.6f | %16lld\n", n, seconds(total_start, total_end),
               seconds(multiplication_start, multiplication_end), (long long)checksum);
        free(a); free(b); free(c);
    }
    puts("Работа программы завершена.");
    return 0;
}