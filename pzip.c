#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define MAX_BUFFER_SIZE 1000000

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
    int file_no;
};

struct data {
    char *keyword;
    int *count;
    int size;
};

int *num_buffer_per_file;

struct buffer *queue;
struct data ***results;

struct buffer terminating_buffer = {.size = -1};

pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t fill = PTHREAD_COND_INITIALIZER;
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
            results[i] = NULL;
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
            
            map += buff_size;

            pthread_cond_signal(&fill);
            pthread_mutex_unlock(&lock);
        }

        close(f);
    }

    for (int i = 0; i < n_threads; i++) {
        pthread_mutex_lock(&lock);
        while (q_size == q_capacity) {
            pthread_cond_wait(&empty, &lock);
        } 
        push(&terminating_buffer);
        pthread_cond_signal(&fill);
        pthread_mutex_unlock(&lock);
    }

    return 0;
}

struct data* consume(struct buffer *buff) {
    struct data *result = malloc(sizeof(struct data));
    int size = 0;
    char *keywords = malloc(buff->size * sizeof(char));
    int *counts = malloc(buff->size * sizeof(int));
    
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
            if (prev != '\0') {
                keywords[size] = prev;
                counts[size] = count;
                count = 1;
                size += 1;
            }
            prev = curr;
        }
    }

    if (buff->size > 0 && prev != '\0') {
        keywords[size] = prev;
        counts[size] = count;
        size += 1;
    }

    if (size == 0) {
        return NULL;
    }

    result->keyword = realloc(keywords, size * sizeof(char));
    result->count = realloc(counts, size * sizeof(int));
    result->size = size;

    return result;
}

void *consumer(void *arg) {
    struct buffer buff;
    do {
        pthread_mutex_lock(&lock);
        while (q_size == 0) {
            pthread_cond_wait(&fill, &lock);
        }

        buff = pop();
        if (buff.size == terminating_buffer.size) {
            pthread_mutex_unlock(&lock);
            return 0;
        }

        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&lock);

        results[buff.file_no][buff.index] = consume(&buff);
    } while (buff.size != terminating_buffer.size);

    return 0;
}

void print() {
    char prev_char = '\0';
    int prev_count = -1;
    for (int i = 0; i < n_files; i++) {
        if (results[i] == NULL) {
            continue;
        }

        int buff_size = num_buffer_per_file[i];
        for (int j = 0; j < buff_size; j++) {
            if (results[i][j] == NULL) {
                continue;
            }

            int count_size = results[i][j]->size;
            if (count_size == 1) {
                if (prev_count != -1) {
                    if (prev_char != results[i][j]->keyword[0]) {
                        fwrite(&prev_count, 4, 1, stdout);
                        fwrite(&prev_char, 1, 1, stdout);
                        prev_char = results[i][j]->keyword[count_size - 1];
                        prev_count = results[i][j]->count[count_size - 1];
                    } else {
                        prev_count += results[i][j]->count[count_size - 1];
                    }
                } else {
                    prev_char = results[i][j]->keyword[count_size - 1];
                    prev_count = results[i][j]->count[count_size - 1];
                }
            } else {
                int temp = results[i][j]->count[0];
                if (prev_count != -1) {
                    if (prev_char == results[i][j]->keyword[0]) {
                        temp += prev_count;
                    } else {
                        fwrite(&prev_count, 4, 1, stdout);
                        fwrite(&prev_char, 1, 1, stdout); 
                    }
                }

                fwrite(&temp, 4, 1, stdout);
                fwrite(&results[i][j]->keyword[0], 1, 1, stdout);

                for(int k = 1; k < count_size - 1; k++) {
                    fwrite(&results[i][j]->count[k], 4, 1, stdout);
                    fwrite(&results[i][j]->keyword[k], 1, 1, stdout);
                }
                prev_char = results[i][j]->keyword[count_size - 1];
                prev_count = results[i][j]->count[count_size - 1];
            }
        }
    }
    if (prev_count != -1) {
        fwrite(&prev_count, 4, 1, stdout);
        fwrite(&prev_char, 1, 1, stdout);
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

    pthread_t pid, cid[n_threads];
	pthread_create(&pid, NULL, producer, argv + 1);
	for (int i = 0; i < n_threads; i++) {
        pthread_create(&cid[i], NULL, consumer, NULL);
    }

    pthread_join(pid, NULL);
    for (int i = 0; i < n_threads; i++) {
        pthread_join(cid[i], NULL);
    }

    print();

    free(queue);
    for(int i = 0; i < n_files; i++) {
        if (results[i] != NULL) {
            int buff_size = num_buffer_per_file[i];
            for (int j = 0; j < buff_size; j++) {
                if (results[i][j]) {
                    free(results[i][j]->keyword);
                    free(results[i][j]->count);
                    free(results[i][j]);
                }
            }
            free(results[i]);
        }
    }
    free(results);
    free(num_buffer_per_file);
}