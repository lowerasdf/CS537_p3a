#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>

#define MAX_BUFFER_SIZE 100000

int n_threads;
int n_files;

int q_head_idx = 0;
int q_tail_idx = 0;
int q_size = 0;

struct buffer {
    char *value;
    int index;
    int size;
};

struct data {
    char *keyword;
    int *count;
};

struct buffer *queue;
struct data *results;

int main(int argc, char *argv[])
{
    if (argc == 1) {
        printf("pzip: file1 [file2 ...]\n");
        exit(1);
    }

    n_threads = get_nprocs();
    n_files = argc - 1;

    queue = malloc(n_threads * sizeof(struct buffer));
    results = malloc(n_files * sizeof(struct data));

    // TODO create pthread for 1 producer and n_threads consumers

    // TODO do mmap in producer, parse in consumer, print output after join
    char curr;
    char prev;
    int count = 1;
    for(int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "r");
        if(f == NULL) {
            continue;
        }

        if (prev == '\0') {
            prev = fgetc(f);
        }

        while ((curr = fgetc(f)) != EOF) {
            if (curr == '\0') {
                continue;
            } else if (curr == prev) {
                count += 1;
            } else {
                fwrite(&count, 4, 1, stdout);
                fwrite(&prev, 1, 1, stdout);
                count = 1;
                prev = curr;
            }
        }

        fclose(f);
    }
    
    if (prev != '\0') {
        fwrite(&count, 4, 1, stdout);
        fwrite(&prev, 1, 1, stdout);
    }

    // TODO free inside

    free(queue);
    free(results);
}