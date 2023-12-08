#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <stdbool.h>
#include <sys/wait.h> 

enum file_type {
    BMP,
    NORMAL_FILE,
    SYMLINK,
    DIRECTORY,
};

void convert_to_greyscale(int file) {
    lseek(file, 54, SEEK_SET);
    unsigned char pixel[3];
    while (read(file, &pixel, 3) == 3) {
        unsigned char grey = 0.299 * pixel[2] + 0.587 * pixel[1] + 0.114 * pixel[0];
        pixel[0] = pixel[1] = pixel[2] = grey;
        lseek(file, -3, SEEK_CUR);
        write(file, &pixel, 3);
    }
}


int write_info_for_files(int outputFile, const char *filePath, struct stat *fileStat, enum file_type type) {
    int length = 0;
    char stats[1024];
    int written_lines = 0;
    struct stat targetStat;
            if (stat(filePath, &targetStat) < 0) {
                perror("Error getting target file stats");
            }
    char user_perms[4] = {(fileStat->st_mode & S_IRUSR) ? 'R' : '-',
                           (fileStat->st_mode & S_IWUSR) ? 'W' : '-',
                           (fileStat->st_mode & S_IXUSR) ? 'X' : '-',
                           '\0'};
    char group_perms[4] = {(fileStat->st_mode & S_IRGRP) ? 'R' : '-',
                           (fileStat->st_mode & S_IWGRP) ? 'W' : '-',
                           (fileStat->st_mode & S_IXGRP) ? 'X' : '-',
                           '\0'};
    char other_perms[4] = {(fileStat->st_mode & S_IROTH) ? 'R' : '-',
                           (fileStat->st_mode & S_IWOTH) ? 'W' : '-',
                           (fileStat->st_mode & S_IXOTH) ? 'X' : '-',
                           '\0'};

    const char *fileName = strrchr(filePath, '/');
    if (fileName == NULL) {
        fileName = filePath;
    } else {
        fileName++;
    }
    switch (type) {
        case BMP: {
            int width = 0, height = 0, size = 0;
            int file = open(filePath, O_RDONLY);
            if (file < 0) {
                perror("Error opening file");
                return 0;
            }
            unsigned char bmpHeader[54];
            if (read(file, bmpHeader, 54) != 54) {
                write(STDOUT_FILENO, "Error reading BMP header\n", 25);
                close(file);
                 return 0;
            }
            width = *(int*)&bmpHeader[18];
            height = *(int*)&bmpHeader[22];
            size = *(int*)&bmpHeader[2];
            close(file);

            length = sprintf(stats, 
                "nume fisier: %s\n" "inaltime: %d\n" "lungime: %d\n" "dimensiune: %d\n" "identificatorul utilizatorului: %d\n""timpul ultimei modificari: %s" 
                "contorul de legaturi: %ld\n" "drepturi de acces user: %s\n" "drepturi de acces grup: %s\n" "drepturi de acces altii: %s\n\n",
                fileName,height,width,size,fileStat->st_uid,ctime(&fileStat->st_mtime),fileStat->st_nlink,user_perms,group_perms,other_perms
                );
            if (write(outputFile, stats, length) == length) {
                written_lines = 10;
            }
            break;
        }
        case NORMAL_FILE:{
            length = sprintf(stats, 
            "nume fisier: %s\n" "identificatorul utilizatorului: %d\n""timpul ultimei modificari: %s" 
            "contorul de legaturi: %ld\n" "drepturi de acces user: %s\n" "drepturi de acces grup: %s\n" "drepturi de acces altii: %s\n\n",
            fileName,fileStat->st_uid,ctime(&fileStat->st_mtime),fileStat->st_nlink,user_perms,group_perms,other_perms
            );
            if (write(outputFile, stats, length) == length) {
                written_lines = 7;
            }
            break;
        }
        case DIRECTORY:{
            length = sprintf(stats, 
            "nume director: %s\n" "identificatorul utilizatorului: %d\n"
            "drepturi de acces user: %s\n" "drepturi de acces grup: %s\n" "drepturi de acces altii: %s\n\n",
            fileName,fileStat->st_uid,user_perms,group_perms,other_perms
            );
            if (write(outputFile, stats, length) == length) {
                written_lines = 5;
            }
            break;
        }
        case SYMLINK:{
            int symlink_size = fileStat->st_size;
            int target_file_size = targetStat.st_size;
            length = sprintf(stats, 
            "nume legatura: %s\n" "dimensiune legatura: %d\n" "dimensiune fisier: %d\n" "drepturi de acces user: %s\n" "drepturi de acces grup: %s\n" "drepturi de acces altii: %s\n\n",
            fileName,symlink_size,target_file_size,user_perms,group_perms,other_perms
            );
            if (write(outputFile, stats, length) == length) {
                written_lines = 6;
            }
            break;
        }
    }
    return written_lines;
}

void process_directory(const char *inputDir, const char *outputDir, int statsFile) {
    DIR *dir = opendir(inputDir);
    if (dir == NULL) {
        perror("Error opening input directory");
        return;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char fullPath[1024];
        sprintf(fullPath, "%s/%s", inputDir, entry->d_name);
        
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe");
            continue;
        }
        struct stat fileStat;
        if (lstat(fullPath, &fileStat) < 0) {
            perror("Error getting file stats");
            exit(1);
        }

        if (S_ISREG(fileStat.st_mode)) {
            char *ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".bmp") == 0) {
                pid_t pid_greyscale = fork();
                if (pid_greyscale == 0) {
                    int file = open(fullPath, O_RDWR);
                    if (file < 0) {
                        perror("Error opening BMP file for greyscale conversion");
                        exit(1);
                    }
                    convert_to_greyscale(file);
                    exit(0);
                } else if (pid_greyscale > 0) {
                    waitpid(pid_greyscale, NULL, 0);
                } else {
                    perror("Error forking for greyscale conversion");
                }
            }
        }
        
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            close(pipefd[0]);
            close(pipefd[1]);
            continue;
        }

        if (pid == 0) {
            close(pipefd[0]);
            int written_lines = 0;
            char outputPath[1024];
            sprintf(outputPath, "%s/%s_statistica.txt", outputDir, entry->d_name);

            int outputFile = open(outputPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (outputFile < 0) {
                perror("Error opening output file");
                exit(1);
            }
            if (S_ISREG(fileStat.st_mode)) {
                char *ext = strrchr(entry->d_name, '.');
                if (ext && strcmp(ext, ".bmp") == 0) {
                    written_lines = write_info_for_files(outputFile, fullPath, &fileStat, BMP);
                } else {
                    written_lines = write_info_for_files(outputFile, fullPath, &fileStat, NORMAL_FILE);
                }
            } else if (S_ISDIR(fileStat.st_mode)) {
                written_lines = write_info_for_files(outputFile, fullPath, &fileStat, DIRECTORY);
            } else if (S_ISLNK(fileStat.st_mode)){
                struct stat targetStat;
                if (stat(fullPath, &targetStat) < 0) {
                    perror("Error getting target file stats");
                    continue;
                }
                if (S_ISREG(targetStat.st_mode)){
                     written_lines = write_info_for_files(outputFile, fullPath, &fileStat, SYMLINK);
                }
            }
                write(pipefd[1], &written_lines, sizeof(written_lines));
                close(outputFile);
                close(pipefd[1]);
                exit(0);
        } else {
            close(pipefd[1]);
            int status;
            waitpid(pid, &status, 0);
            int readLines;
            char buf[1024];
            read(pipefd[0], &readLines, sizeof(readLines));
            int length = sprintf(buf, "Numarul de linii scrise de procesul %d: %d\n", pid, readLines);
            write(statsFile, buf, length);
            close(pipefd[0]);
        }
    }

    closedir(dir);
}
int main(int argc, char *argv[]) {
    if (argc != 3) {
        write(STDOUT_FILENO, "Usage: ./program <director_intrare> <director_iesire>\n", 55);
        return 1;
    }

    int statsFile = open("statistica.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (statsFile < 0) {
        perror("Error opening stats file");
        return 2;
    }

    process_directory(argv[1], argv[2], statsFile);

    close(statsFile);
    return 0;
}
