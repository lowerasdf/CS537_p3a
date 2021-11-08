#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define MAX_BUFFER_SIZE 10

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
    int size;
};

int *num_buffer_per_file;

struct buffer *queue;
struct data **results;

pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t fill = PTHREAD_COND_INITIALIZER;
pthread_cond_t printer_cv = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void push(struct buffer *buff) {
    queue[q_head_idx] = *buff;
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
        struct stat file_stat;
        if (f < 0 || fstat(f, &file_stat) || file_stat.st_size == 0) {
            continue;
        }

        int n_pages = 1 + ((file_stat.st_size - 1) / MAX_BUFFER_SIZE);
        int remainder_size = file_stat.st_size % MAX_BUFFER_SIZE;
        if (remainder_size == 0) {
            remainder_size = MAX_BUFFER_SIZE;
        }
        
        results[i] = malloc(n_pages * sizeof(struct data));
        num_buffer_per_file[i] = n_pages;

        char *map = mmap(NULL, file_stat.st_size, PROT_READ, MAP_SHARED, f, 0);
        if (map == MAP_FAILED) {
            printf("mmap() failed\n");
            close(f);
            exit(1);
        }

        for(int j = 0; j < n_pages; j++) {
            pthread_mutex_lock(&lock);
            while (q_size == q_capacity) {
                pthread_cond_wait(&empty, &lock);
            }

            int buff_size = MAX_BUFFER_SIZE;
            if (j == n_pages - 1) {
                buff_size = remainder_size;
            }

            push(&(struct buffer){.value=map, .index=j, .size=buff_size, .file_no=i});
            pthread_cond_signal(&fill);
            pthread_mutex_unlock(&lock);
        }

        close(f);
    }

    pthread_mutex_lock(&lock);
    is_production_done = 1;
    pthread_mutex_unlock(&lock);

    return 0;
}

struct data consume(struct buffer *buff) {
    struct data result;
    int size = 0;
    char *keywords = malloc(buff->size * sizeof(char));
    char *counts = malloc(buff->size * sizeof(int));
    
    char prev = buff->value[0];
    char curr = buff->value[0];
    int count = 1;
    for (int i = 1; i < buff->size; i++) {
        curr = buff->value[i];
        if (curr == '\0') {
            continue;
        } else if (curr == prev) {
            count += 1;
        } else {
            keywords[size] = prev;
            counts[size] = count;
            count = 1;
            prev = curr;
            size += 1;
        }
    }
    
    if (buff->size > 0) {
        keywords[size] = prev;
        counts[size] = count;
        size += 1;
    }

    printf("size: %d\n", size);
    for (int i = 0; i < size; i++) {
        printf("%d%c\n", counts[i], keywords[i]);
    }

    result.keyword = realloc(keywords, size);
    result.count = realloc(counts, size);
    result.size = size;
    return result;
}

// TODO run munmap(*ptr, size) when done
void *consumer(void *arg) {
    while (is_production_done == 0 || q_size != 0) {
        pthread_mutex_lock(&lock);
        while (q_size == 0) {
            pthread_cond_wait(&fill, &lock);
        }

        if (is_production_done) {
            pthread_mutex_unlock(&lock);
            return 0;
        }

        struct buffer buff = pop();
        // printf("consuming: %d, %d, %d\n", buff.size, buff.index, buff.file_no);

        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&lock);

        results[buff.file_no][buff.index] = consume(&buff);
        munmap(buff.value, buff.size);
    }

    return 0;
}

void *printer(void *args) {
    // int ticket = 0;


    return 0;
}

void print() {
    for (int i = 0; i < n_files; i++) {
        if (results[i] != NULL) {
            int buff_size = num_buffer_per_file[i];
            for (int j = 0; j < buff_size; j++) {
                int count_size = results[i][j].size;
                if (i < n_files - 1 && j == buff_size - 1 && results[i][buff_size - 1].keyword[count_size - 1] == results[i+1][0].keyword[0]) {
                    results[i+1][0].count[0] += results[i][buff_size - 1].count[count_size - 1];
                    count_size--;
                }

                for(int k = 0; k < count_size; k++) {
                    printf("%d%c\n", results[i][j].count[k], results[i][j].keyword[k]);
                    // fwrite(&results[i][j].count[k], 4, 1, stdout);
                    // fwrite(&results[i][j].keyword[k], 1, 1, stdout);
                }
            }
        }
    }
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
    results = malloc(n_files * sizeof(struct data *));
    num_buffer_per_file = malloc(n_files * sizeof(int));

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

    // for (int i = 0; i < 4; i++) {
    //     struct buffer test = pop();
    //     printf("buffer at i=%d, index=%d, size=%d, file_no=%d\n", i, test.index, test.size, test.file_no);
    // }

    // for (int i = 0; i < 2; i++) {
    //     for (int j = 0; j < 1; j++) {
    //         char *keywords = malloc(3 * sizeof(char));
    //         keywords[0] = 'a';
    //         keywords[1] = 'b';
    //         keywords[2] = 'a';

    //         int *counts = malloc(3 * sizeof(int));
    //         counts[0] = 2;
    //         counts[1] = 3;
    //         counts[2] = 4;

    //         results[i][j].keyword = keywords;
    //         results[i][j].count = counts;
    //         results[i][j].size = 3;
    //     }
    // }
    print();

    free(queue);
    for(int i = 0; i < n_files; i++) {
        if (results[i] != NULL) {
            free(results[i]);
        }
    }
    free(results);
    free(num_buffer_per_file);

    // TODO printing thread

    // TODO free inside

    // TODO wait
}