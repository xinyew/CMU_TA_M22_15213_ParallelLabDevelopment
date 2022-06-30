#include <stdio.h>
#include <stdlib.h>
#include <ftw.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <regex.h>


#define MAX_FILE_NUM 4096
#define BUF_SIZE 4096

// GLOBALS
bool recursive = false;
char *pattern = NULL;

char *parse_args(int argc, char *argv[]) {
    char *usage = "Usage:\n"
                  "./grep [-rh]\n"
                  "-E     Enable extended regex\n"
                  "-h     Show help message\n"
                  "-r     Recursively search through directory structure\n";
    int opt;

    while ((opt = getopt(argc, argv, "r")) != -1) {
        switch (opt) {
            case 'r':
                recursive = true;
                break;

            case '?':
                printf("Error parsing command line arguments\n %s", usage);
                exit(1);
        }
    }
    printf("argc: %d, optind: %d\n", argc, optind);

    // If there are not two arguments left (pattern and file name)
    if (argc - optind != 2) {
        fprintf(stderr, "Missing either pattern or file name in parsing command line arguments\n %s", usage);
        exit(1);
    }

    pattern = argv[optind];
    return argv[optind + 1];
}

void grep_file(const char *file_name) {
    FILE *file;
    char buf[BUF_SIZE]; 
    char error_msg[100];   
    int regex_errorno; // value returned from regex library functions, zero indicates success
    regex_t regex; // tmp variable to conduct regex operations

    if (regcomp(&regex, pattern, 0)) {
        fprintf(stderr, "regex compile failed\n");
        exit(1);
    }
    if ((file = fopen(file_name, "r")) == NULL) {
        perror("Error Opening File");        
        return;
    }
        
    while (fgets(buf, BUF_SIZE, file)) {
        regex_errorno = regexec(&regex, buf, 0, NULL, 0);
        if (!regex_errorno) {
            printf("%s\n", buf);
        } else if (regex_errorno == REG_NOMATCH) {
            continue;
        } else {
            regerror(regex_errorno, &regex, error_msg, sizeof(error_msg));
            fprintf(stderr, "match failed: %s\n", error_msg);
            exit(1);
        }
    }
}

int grep_file_wrapper(const char *filename, const struct stat *statptr,
    int fileflags, struct FTW *pfwt)
{
    if (fileflags == FTW_F)
        grep_file(filename);

    return 0; // Tells nftw to continue
}

void grep_dir(char *path) {
    /* Iterates over the directory structure starting at path and
    calls grep_file_wrapper on each file */
    nftw(path, grep_file_wrapper, MAX_FILE_NUM, 0);
}

int main(int argc, char *argv[]) {
    struct stat sb;

    char *file_name = parse_args(argc, argv);

    if (stat(file_name, &sb) == -1) {
        perror("stat");
        exit(1);
    }

    if (recursive && S_ISREG(sb.st_mode)) {
        fprintf(stderr, "%s is not a directory", file_name);
        exit(1);
    }

    if (!recursive && S_ISDIR(sb.st_mode)) {
        fprintf(stderr, "%s is not a file", file_name);
        exit(1);
    }

    if (!recursive)
        grep_file(file_name);
    else
        grep_dir(file_name);
}
