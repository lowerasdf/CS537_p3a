#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc == 1) {
        printf("pzip: file1 [file2 ...]\n");
        exit(1);
    }

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
}