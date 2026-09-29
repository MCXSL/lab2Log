#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* Compile: gcc -O2 -std=c11 sort_benchmark.c -o sort_benchmark */

static void shell_sort(int *items, size_t count) {
    const size_t gaps[] = {9, 5, 3, 2, 1};
    for (size_t k = 0; k < sizeof(gaps) / sizeof(gaps[0]); ++k) {
        size_t gap = gaps[k];
        for (size_t i = gap; i < count; ++i) {
            int x = items[i];
            size_t j = i;
            while (j >= gap && x < items[j - gap]) {
                items[j] = items[j - gap];
                j -= gap;
            }
            items[j] = x;
        }
    }
}

static void quick_sort(int *items, long left, long right) {
    long i = left, j = right;
    int pivot = items[(left + right) / 2];
    do {
        while (items[i] < pivot && i < right) ++i;
        while (pivot < items[j] && j > left) --j;
        if (i <= j) {
            int t = items[i]; items[i] = items[j]; items[j] = t;
            ++i; --j;
        }
    } while (i <= j);
    if (left < j) quick_sort(items, left, j);
    if (i < right) quick_sort(items, i, right);
}

static int compare_ints(const void *left, const void *right) {
    int a = *(const int *)left, b = *(const int *)right;
    return (a > b) - (a < b);
}

static int sorted(const int *items, size_t count) {
    for (size_t i = 1; i < count; ++i)
        if (items[i - 1] > items[i]) return 0;
    return 1;
}

typedef void (*sort_fn)(int *, size_t);
static void quick_adapter(int *a, size_t n) { if (n) quick_sort(a, 0, (long)n - 1); }
static void qsort_adapter(int *a, size_t n) { qsort(a, n, sizeof(*a), compare_ints); }

static double measure(sort_fn fn, int *data, const int *source, size_t n) {
    memcpy(data, source, n * sizeof(*data));
    clock_t begin = clock();
    fn(data, n);
    clock_t end = clock();
    if (!sorted(data, n)) { fputs("Sort error\n", stderr); exit(EXIT_FAILURE); }
    return (double)(end - begin) / CLOCKS_PER_SEC;
}

int main(void) {
    size_t n;
#ifdef _WIN32
    /* Исходный файл сохранён в UTF-8, поэтому консоли тоже нужен UTF-8. */
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
    puts("Сравнение алгоритмов сортировки");
    printf("Количество элементов массива: ");
    if (scanf("%zu", &n) != 1 || n == 0) {
        fputs("Нужно ввести положительное целое число.\n", stderr);
        return EXIT_FAILURE;
    }
    int *source = malloc(n * sizeof(*source)), *work = malloc(n * sizeof(*work));
    const char *names[] = {"случайный", "возрастающий", "убывающий", "1-я половина возрастает, 2-я убывает"};
    sort_fn algorithms[] = {shell_sort, quick_adapter, qsort_adapter};
    const char *algorithm_names[] = {"shell", "quick", "qsort"};
    if (!source || !work) return fprintf(stderr, "Не удалось выделить память.\n"), EXIT_FAILURE;

    puts("\nКоличество элементов: ");
    printf("%zu\n", n);
    puts("\nТип исходного массива                 Алгоритм       Время, с");
    puts("--------------------------------------------------------------");
    srand((unsigned)time(NULL));
    for (int kind = 0; kind < 4; ++kind) {
        for (size_t i = 0; i < n; ++i) {
            if (kind == 0) source[i] = rand();
            else if (kind == 1) source[i] = (int)i;
            else if (kind == 2) source[i] = (int)(n - i);
            else source[i] = i < n / 2 ? (int)i : (int)(n - i);
        }
        for (size_t a = 0; a < 3; ++a)
            printf("%-38s %-11s %10.6f\n", names[kind], algorithm_names[a],
                   measure(algorithms[a], work, source, n));
    }
    free(source); free(work);
    return 0;
}
