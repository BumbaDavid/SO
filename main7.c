#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <stdbool.h>

enum file_type {
    BMP,
    NORMAL_FILE,
    SYMLINK,
    DIRECTORY,
};

void write_info_for_files(int statsFile, const char *filePath, struct stat *fileStat, enum file_type type) {
    int length = 0;
    char stats[1024];
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
                return;
            }
            unsigned char bmpHeader[54];
            if (read(file, bmpHeader, 54) != 54) {
                write(STDOUT_FILENO, "Error reading BMP header\n", 25);
                close(file);
                 return;
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
            break;
        }
        case NORMAL_FILE:{
            length = sprintf(stats, 
            "nume fisier: %s\n" "identificatorul utilizatorului: %d\n""timpul ultimei modificari: %s" 
            "contorul de legaturi: %ld\n" "drepturi de acces user: %s\n" "drepturi de acces grup: %s\n" "drepturi de acces altii: %s\n\n",
            fileName,fileStat->st_uid,ctime(&fileStat->st_mtime),fileStat->st_nlink,user_perms,group_perms,other_perms
            );
            break;
        }
        case DIRECTORY:{
            length = sprintf(stats, 
            "nume director: %s\n" "identificatorul utilizatorului: %d\n"
            "drepturi de acces user: %s\n" "drepturi de acces grup: %s\n" "drepturi de acces altii: %s\n\n",
            fileName,fileStat->st_uid,user_perms,group_perms,other_perms
            );
            break;
        }
        case SYMLINK:{
            int symlink_size = fileStat->st_size;
            int target_file_size = targetStat.st_size;
            length = sprintf(stats, 
            "nume legatura: %s\n" "dimensiune legatura: %d\n" "dimensiune fisier: %d\n" "drepturi de acces user: %s\n" "drepturi de acces grup: %s\n" "drepturi de acces altii: %s\n\n",
            fileName,symlink_size,target_file_size,user_perms,group_perms,other_perms
            );
            break;
        }
    }
    if (write(statsFile, stats, length) != length) {
        write(STDOUT_FILENO, "Error writing to stats file\n", 28);
    }
}

void process_directory(const char *dirPath, int statsFile) {
    DIR *dir = opendir(dirPath);
    if (dir == NULL) {
        perror("Error opening directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char fullPath[1024];
        sprintf(fullPath, "%s/%s", dirPath, entry->d_name);
        
        struct stat fileStat;
        if (lstat(fullPath, &fileStat) < 0) {
            perror("Error getting file stats");
            continue;
        }

        if (S_ISREG(fileStat.st_mode)) {
            char *ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".bmp") == 0) {
                write_info_for_files(statsFile, fullPath, &fileStat, BMP);
            } else {
                write_info_for_files(statsFile, fullPath, &fileStat, NORMAL_FILE);
            }
        } else if (S_ISDIR(fileStat.st_mode)) {
            write_info_for_files(statsFile, fullPath, &fileStat, DIRECTORY);
        } else if (S_ISLNK(fileStat.st_mode)){
            struct stat targetStat;
            if (stat(fullPath, &targetStat) < 0) {
                perror("Error getting target file stats");
                continue;
            }
            if (S_ISREG(targetStat.st_mode)){
                 write_info_for_files(statsFile, fullPath, &fileStat, SYMLINK);
            }
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        write(STDOUT_FILENO, "Usage: ./program <director_intrare>\n", 37);
        return 1;
    }

    int statsFile = open("statistica.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (statsFile < 0) {
        perror("Error opening stats file");
        return 2;
    }

    process_directory(argv[1], statsFile);

    close(statsFile);
    return 0;
}
