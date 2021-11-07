#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <pthread.h>

#define MAX_BUFFER_SIZE 100000

int n_threads;
int n_files;

int q_head_idx = 0;
int q_tail_idx = 0;
int q_size = 0;
int q_capacity;

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

pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t fill = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void push(struct buffer buff) {
    queue[q_head_idx] = buff;
    q_head_idx = (q_head_idx + 1) % q_capacity;
    q_size++;
}

struct buffer pop() {
    struct buffer buff = queue[q_tail_idx];
	q_tail_idx = (q_tail_idx + 1) % q_capacity;
  	q_size--;
  	return buff;
}

int main(int argc, char *argv[])
{
    if (argc == 1) {
        printf("pzip: file1 [file2 ...]\n");
        exit(1);
    }

    n_threads = get_nprocs();
    n_files = argc - 1;
    q_capacity = n_threads;

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

    // TODO printing thread

    // TODO free inside

    // TODO wait

    free(queue);
    free(results);
}