#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        write(STDOUT_FILENO, "Usage: ./program <fisier_intrare>\n", 34);
        return 1;
    }

    int file = open(argv[1], O_RDONLY);
    if (file < 0) {
        perror("Error opening file");
        return 2;
    }

    unsigned char bmpHeader[54];
    if (read(file, bmpHeader, 54) != 54) {
        write(STDOUT_FILENO, "Error reading BMP header\n", 25);
        close(file);
        return 3;
    }

    if (bmpHeader[0] != 'B' || bmpHeader[1] != 'M') {
        write(STDOUT_FILENO, "Not a BMP file\n", 15);
        close(file);
        return 4;
    }

    int width = *(int*)&bmpHeader[18];
    int height = *(int*)&bmpHeader[22];
    int size = *(int*)&bmpHeader[2];

    struct stat fileStat;
    if (fstat(file, &fileStat) < 0) {
        perror("Error getting file stats");
        close(file);
        return 5;
    }

    close(file);

    int statsFile = open("statistica.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (statsFile < 0) {
        perror("Error opening stats file");
        return 6;
    }

    char stats[512];

    char user_perms[4] = {
        (fileStat.st_mode & S_IRUSR) ? 'R' : '-',
        (fileStat.st_mode & S_IWUSR) ? 'W' : '-',
        (fileStat.st_mode & S_IXUSR) ? 'X' : '-',
        '\0'
        };

    char group_perms[4] = {
        (fileStat.st_mode & S_IRGRP) ? 'R' : '-',
        (fileStat.st_mode & S_IWGRP) ? 'W' : '-',
        (fileStat.st_mode & S_IXGRP) ? 'X' : '-',
        '\0'
        };

    char other_perms[4] = {
        (fileStat.st_mode & S_IROTH) ? 'R' : '-',
        (fileStat.st_mode & S_IWOTH) ? 'W' : '-',
        (fileStat.st_mode & S_IXOTH) ? 'X' : '-',
        '\0'
        };

    int length = sprintf(stats, 
                         "nume fisier: %s\n"
                         "inaltime: %d\n"
                         "lungime: %d\n"
                         "dimensiune: %d\n"
                         "identificatorul utilizatorului: %d\n"
                         "timpul ultimei modificari: %s"
                         "contorul de legaturi: %ld\n"
                         "drepturi de acces user: %c%c%c\n"
                         "drepturi de acces grup: %c%c%c\n"
                         "drepturi de acces altii: %c%c%c\n",
                         argv[1],
                         height,
                         width,
                         size,
                         fileStat.st_uid,
                         ctime(&fileStat.st_mtime),
                         fileStat.st_nlink,
                         user_perms[0],user_perms[1],user_perms[2],
                         group_perms[0],group_perms[1],group_perms[2],
                         other_perms[0],other_perms[1],other_perms[2]
                         );

    if (write(statsFile, stats, length) != length) {
        write(STDOUT_FILENO, "Error writing to stats file\n", 28);
        close(statsFile);
        return 7;
    }

    close(statsFile);
    return 0;
}
