#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_BUFFER_SIZE 100000

int n_threads;
int n_files;

int q_head_idx = 0;
int q_tail_idx = 0;
int q_size = 0;
int q_capacity;

int is_production_done = 0;

struct buffer {
    char *value;
    int index;
    int size;
    int file_no;
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

void *producer(void *arg) {
    char **files = (char **) arg;

    for(int i = 0; i < n_files; i++) {
        int f = open(files[i], O_RDONLY);
        if (f < 0) {
            continue;
        }

        struct stat file_stat;
        if (fstat(f, &file_stat) || file_stat.st_size == 0) {
            continue;
        }

        printf("file size: %ld\n", file_stat.st_size);
    }

    pthread_mutex_lock(&lock);
    is_production_done = 1;
    pthread_mutex_unlock(&lock);
}

void *consumer(void *arg) {
    // TODO consume
    // TODO also wake printer when done

    
}

void *printer(void *args) {
    // TODO 1 thread, print when results[ticket] is not null, otherwise sleep
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
    results = malloc(n_files * sizeof(struct *data));

    pthread_t pid, cid[n_threads], printid;
	pthread_create(&pid, NULL, producer, argv + 1);

	for (int i = 0; i < n_threads; i++) {
        pthread_create(&cid[i], NULL, consumer, NULL);
    }

    pthread_create(&printid, NULL, printer, NULL);

    pthread_join(pid, NULL);
    for (int i = 0; i < n_threads; i++) {
        pthread_join(cid[i], NULL);
    }
    pthread_join(printid, NULL);

    // TODO do mmap in producer, parse in consumer, print output after join
    // char curr;
    // char prev;
    // int count = 1;
    // for(int i = 1; i < argc; i++) {
    //     FILE *f = fopen(argv[i], "r");
    //     if(f == NULL) {
    //         continue;
    //     }

    //     if (prev == '\0') {
    //         prev = fgetc(f);
    //     }

    //     while ((curr = fgetc(f)) != EOF) {
    //         if (curr == '\0') {
    //             continue;
    //         } else if (curr == prev) {
    //             count += 1;
    //         } else {
    //             fwrite(&count, 4, 1, stdout);
    //             fwrite(&prev, 1, 1, stdout);
    //             count = 1;
    //             prev = curr;
    //         }
    //     }

    //     fclose(f);
    // }
    
    // if (prev != '\0') {
    //     fwrite(&count, 4, 1, stdout);
    //     fwrite(&prev, 1, 1, stdout);
    // }

    // TODO printing thread

    // TODO free inside

    // TODO wait

    free(queue);
    free(results);
}