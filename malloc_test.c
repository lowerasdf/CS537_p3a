#include <stdio.h>
#include <stdlib.h>

struct data {
    char key;
    int value;
};

int main() {
    int file_size = 10;
    int page_size = 5;

    struct data **result = malloc(file_size * sizeof(struct data*));
    for (int i = 0; i < file_size - 2; i++) {
        result[i] = malloc(page_size * sizeof(struct data));
        for(int j = 0; j < page_size; j++) {
            struct data temp = {'a', 2};
            result[i][j] = temp;
        }
    }

    for (int i = 0; i < file_size; i++) {
        for (int j = 0; j < page_size; j++) {
            printf("%d,%d: %c,%d\n", i, j, result[i][j].key, result[i][j].value);
        }
    }

    for (int i = 0; i < file_size; i++) {
        free(result[i]);
    }
    free(result);
}