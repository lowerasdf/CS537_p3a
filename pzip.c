#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
// #include <time.h>

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

pthread_cond_t printer_cv = PTHREAD_COND_INITIALIZER;
pthread_mutex_t printer_lock = PTHREAD_MUTEX_INITIALIZER;

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

        // printf("file_stat.st_size:%ld\n", file_stat.st_size);
        char *map = mmap(NULL, file_stat.st_size, PROT_READ, MAP_SHARED, f, 0);
        if (map == MAP_FAILED) {
            printf("mmap() failed\n");
            close(f);
            exit(1);
        }

        // for(int i = 0; i < file_stat.st_size; i++) {
        //     if (map[i]) {
        //         printf("map: %c\n", map[i]);
        //     }
        // }

        // printf("n_pages:%d\n", n_pages);
        // printf("file_stat.st_size:%ld\n", file_stat.st_size);
        for(int j = 0; j < n_pages; j++) {
            pthread_mutex_lock(&lock);
            while (q_size == q_capacity) {
                pthread_cond_wait(&empty, &lock);
            }

            int buff_size = MAX_BUFFER_SIZE;
            if (j == n_pages - 1) {
                buff_size = remainder_size;
            }
            // printf("last index of map: %c\n", map[buff_size-1]);
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
        // printf("prev:%c, count:%d\n", prev, count);
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
    
    // for (int i = 1; i < buff->size; i++) {
    //     if (result.keyword[i] == '\n') {
    //         printf("%d, NEWLINE\n", i);
    //     } else {
    //         printf("%d, %c\n", i, buff->value[i]);
    //     }
    // }

    // pthread_mutex_lock(&lock);
    // printf("size: %d\n", result->size);
    // for (int i = 0; i < result->size; i++) {
    //     printf("%d%c\n", result->count[i], result->keyword[i]);
    // }
    // pthread_mutex_unlock(&lock);

    return result;
}

// TODO run munmap(*ptr, size) when done
void *consumer(void *arg) {
    struct buffer buff;
    do {
        pthread_mutex_lock(&lock);
        while (q_size == 0) {
            pthread_cond_wait(&fill, &lock);
        }

        buff = pop();
        // printf("consuming: %d, %d, %d\n", buff.size, buff.index, buff.file_no);
        if (buff.size == terminating_buffer.size) {
            pthread_mutex_unlock(&lock);
            return 0;
        }

        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&lock);

        results[buff.file_no][buff.index] = consume(&buff);
        // pthread_cond_signal(&printer_cv);
        // munmap(buff.value, buff.size);
    } while (buff.size != terminating_buffer.size);

    return 0;
}

void *printer(void *args) {
    // for (int i = 0; i < n_files; i++) {
    //     while (results[i] == NULL || (i < n_files - 1 && results[i+1] == NULL)) {
    //         pthread_cond_wait(&printer_cv, &printer_lock);
    //     }

    //     int buff_size = num_buffer_per_file[i];
    //     for (int j = 0; j < buff_size; j++) {
    //         while (results[i][j] == NULL || (j < buff_size - 1 && results[i][j+1] == NULL)) {
    //             pthread_cond_wait(&printer_cv, &printer_lock);
    //         }

    //         int count_size = results[i][j].size;
    //         if (i < n_files - 1 && j == buff_size - 1 && results[i][buff_size - 1].keyword[count_size - 1] == results[i+1][0].keyword[0]) {
    //             results[i+1][0].count[0] += results[i][buff_size - 1].count[count_size - 1];
    //             count_size--;
    //         } else if (j < buff_size - 1 && results[i][j].keyword[count_size - 1] == results[i][j+1].keyword[0]) {
    //             results[i][j+1].count[0] += results[i][j].count[count_size - 1];
    //             count_size--;
    //         }

    //         for(int k = 0; k < count_size; k++) {
                
    //             printf("%d%c\n", results[i][j].count[k], results[i][j].keyword[k]);
    //             // fwrite(&results[i][j].count[k], 4, 1, stdout);
    //             // fwrite(&results[i][j].keyword[k], 1, 1, stdout);
    //         }
    //     }
    // }

    return 0;
}

// print all values in result
// results[0] = file 1
// results[1] = file 2
// results[0][0] = {{count: 3, keyword:z}, {count: 5, keyword:b}, size = 2}
// results[0][1] = {{count: 2, keyword:b}, {count: 3, keyword:c}, size = 2}
// results[1][0] = {{count: 1, keyword:c}, {count: 2, keyword:x}, size = 2}

// 3z7b4c2x

void print() {
    // for(int i = 0; i < n_files; i++) {
    //     printf("num buff per file: %d\n", num_buffer_per_file[i]);
    //     int null_count = 0;
    //     for (int j = 0; j < num_buffer_per_file[i]; j++) {
    //         if (results[i][j]) {
    //             printf("size:%d\n", results[i][j]->size);
    //             int count_size = results[i][j]->size;
    //             for(int k = 0; k < count_size; k++) {
    //                 printf("%d%c\n", results[i][j]->count[k], results[i][j]->keyword[k]);
    //             }
    //         } else {
    //             null_count += 1;
    //         }
    //     }
    //     printf("null_count:%d\n", null_count);
    // }

    // APPROACH #2
    struct data *prev = NULL;
    for (int i = 0; i < n_files; i++) {
        if (results[i] == NULL) {
            continue;
        }
        int buff_size = num_buffer_per_file[i];
        for (int j = 0; j < buff_size; j++) {
            if (results[i][j] == NULL) {
                continue;
            }
            if (prev != NULL && results[i][j]->keyword[0] == prev->keyword[prev->size - 1]) {
                results[i][j]->count[0] += prev->count[prev->size - 1];
                prev->size -= 1;
            }
            prev = results[i][j];
        }
    }

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
            for (int k = 0; k < count_size; k++) {
                fwrite(&results[i][j]->count[k], 4, 1, stdout);
                fwrite(&results[i][j]->keyword[k], 1, 1, stdout);
            }
        }
    }

    // APPROACH #1
    // char prev_char = '\0';
    // int prev_count = -1;
    // for (int i = 0; i < n_files; i++) {
    //     if (results[i] == NULL) {
    //         continue;
    //     }

    //     int buff_size = num_buffer_per_file[i];
    //     for (int j = 0; j < buff_size; j++) {
    //         if (results[i][j] == NULL) {
    //             continue;
    //         }

    //         int count_size = results[i][j]->size;
    //         if (count_size == 1) {
    //             if (prev_count != -1) {
    //                 if (prev_char != results[i][j]->keyword[0]) {
    //                     // flush prev
    //                     // printf("%d%c\n", prev_count, prev_char);
    //                     fwrite(&prev_count, 4, 1, stdout);
    //                     fwrite(&prev_char, 1, 1, stdout);
    //                     prev_char = results[i][j]->keyword[count_size - 1];
    //                     prev_count = results[i][j]->count[count_size - 1];
    //                 } else {
    //                     prev_count += results[i][j]->count[count_size - 1];
    //                 }
    //             } else {
    //                 prev_char = results[i][j]->keyword[count_size - 1];
    //                 prev_count = results[i][j]->count[count_size - 1];
    //             }
    //         } else {
    //             for(int k = 0; k < count_size - 1; k++) {
    //                 if (prev_count != -1 && k == 0) {
    //                     if (prev_char != results[i][j]->keyword[k]) {
    //                         // flush prev
    //                         // printf("%d%c\n", prev_count, prev_char);
    //                         fwrite(&prev_count, 4, 1, stdout);
    //                         fwrite(&prev_char, 1, 1, stdout);

    //                         // print normal
    //                         // printf("%d%c\n", results[i][j]->count[k], results[i][j]->keyword[k]);
    //                         fwrite(&results[i][j]->count[k], 4, 1, stdout);
    //                         fwrite(&results[i][j]->keyword[k], 1, 1, stdout);
    //                     } else {
    //                         // printf("%d%c\n", results[i][j]->count[k] + prev_count, results[i][j]->keyword[k]);
    //                         fwrite(&results[i][j]->count[k], 4, 1, stdout);
    //                         fwrite(&results[i][j]->keyword[k], 1, 1, stdout);
    //                     }
    //                 } else {
    //                     // print normal
    //                     // printf("%d%c\n", results[i][j]->count[k], results[i][j]->keyword[k]);
    //                     fwrite(&results[i][j]->count[k], 4, 1, stdout);
    //                     fwrite(&results[i][j]->keyword[k], 1, 1, stdout);
    //                 }
    //             }
    //             prev_char = results[i][j]->keyword[count_size - 1];
    //             prev_count = results[i][j]->count[count_size - 1];
    //         }
    //     }
    // }
    // if (prev_count != -1) {
    //     // flush prev
    //     // printf("%d%c\n", prev_count, prev_char);
    //     fwrite(&prev_count, 4, 1, stdout);
    //     fwrite(&prev_char, 1, 1, stdout);
    // }
}

// void print() {
//     // TODO store the prev value instead
//     // FILE *f = fopen("out.txt", "w+");
//     // char *temp = malloc(n_files * MAX_BUFFER_SIZE * (sizeof(int)+sizeof(char)));
//     for (int i = 0; i < n_files; i++) {
//         if (results[i] != NULL) {
//             int buff_size = num_buffer_per_file[i];
//             for (int j = 0; j < buff_size; j++) {
//                 int count_size = results[i][j].size;
//                 if (i < n_files - 1 && j == buff_size - 1 && results[i][buff_size - 1].keyword[count_size - 1] == results[i+1][0].keyword[0]) {
//                     results[i+1][0].count[0] += results[i][buff_size - 1].count[count_size - 1];
//                     count_size--;
//                 } else if (j < buff_size - 1 && results[i][j].keyword[count_size - 1] == results[i][j+1].keyword[0]) {
//                     results[i][j+1].count[0] += results[i][j].count[count_size - 1];
//                     count_size--;
//                 }

//                 for(int k = 0; k < count_size; k++) {
//                     // printf("%d%c\n", results[i][j].count[k], results[i][j].keyword[k]);
//                     // fwrite(&results[i][j].count[k], 4, 1, stdout);
//                     // fwrite(&results[i][j].keyword[k], 1, 1, stdout);
//                     // fwrite(&results[i][j].count[k], 4, 1, f);
//                     // fwrite(&results[i][j].keyword[k], 1, 1, f);
//                 }
//             }
//         }
//     }

//     // fclose(f);
// }

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

    // clock_t start = clock();

    pthread_t pid, cid[n_threads];
    // pthread_t pid, cid[n_threads], printid;
	pthread_create(&pid, NULL, producer, argv + 1);

	for (int i = 0; i < n_threads; i++) {
        pthread_create(&cid[i], NULL, consumer, NULL);
    }

    // pthread_create(&printid, NULL, printer, NULL);

    pthread_join(pid, NULL);
    for (int i = 0; i < n_threads; i++) {
        pthread_join(cid[i], NULL);
    }

    // pthread_cond_broadcast(&printer_cv);
    // pthread_join(printid, NULL);

    // clock_t end = clock();
    // float seconds = ((float)(end - start)) / CLOCKS_PER_SEC;
    // printf("thread time:%f s\n", seconds);

    // start = clock();
    print();
    // end = clock();
    // seconds = ((float)(end - start)) / CLOCKS_PER_SEC;
    // printf("print time:%f s\n", seconds);

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
}