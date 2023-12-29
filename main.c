
#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#define KB(x)       (x? (x / 1024) : 0)
#define MB(x)       (x? (x / 1024 / 1024) : 0)
#define MS(x)       (x? (x / 1000) : 0)


#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define ORANGE "\033[0;33m"
#define NC "\033[0m"

#define printf_warn(...) printf("[" RED "WARN" NC "] " __VA_ARGS__)
#define printf_err(...) printf("[" RED "ERR" NC "] " __VA_ARGS__)
#define printf_info(...) printf("[" GREEN "INFO" NC "] " __VA_ARGS__)

unsigned long timestamp()
{
    struct timeval tv = {0};
    gettimeofday(&tv, NULL);
    
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

int disk_info(const char *odev)
{
    char tmp[128];
    
    // Check if odev is a device path
    if (strstr(odev, "/dev/") != odev)
        return -1;

    // Use stat to get information about the file
    struct stat file_info;
    if (stat(odev, &file_info) != 0)
        return -1;
    
    if (!S_ISBLK(file_info.st_mode))
        return -1;
    
    snprintf(tmp, sizeof(tmp), "fdisk -l %s", odev);
    system(tmp);
    return 0;
}

int main(int argc, char *argv[])
{
    if( argc < 3 ) {
        printf("\n  Usage: ddflash <in> <out> [<blocksize in bytes>]\n");
        return 1;
    }
    int BLOCKSIZE = (argc == 4? atoi(argv[3]) : (1 * 1024 * 1024));
    const char *ifile = argv[1];
    const char *ofile = argv[2];
    unsigned long wtime;
    unsigned long totaltime = 0;
    unsigned long readed = 0;
    unsigned long writed = 0;
    off64_t totalsize = 0;
    int ifd = -1, ofd = -1;

    if (disk_info(ofile) == 0)
        printf("\n");

    while(1) {
        printf_warn("Do you accept to write \"" ORANGE "%s" NC "\"? [Y/n]: ", ofile);
        int is = getc(stdin);
        if( is != 'n' && is != 'y' ) {
            printf("Choise y or n\n");
            continue;
        }
        
        if( is == 'n' ) {
            printf_err("Aborted\n");
            return 1;
        }
        
        break;
    }
    
        
    ifd = open(ifile, O_RDONLY);
    if( ifd < 0 ) {
        fprintf(stderr, "ERROR: Can not open input file for read: %s\n", strerror(errno));
        return 1;
    }
    
    ofd = open(ofile, O_WRONLY | O_SYNC);
    if( ofd < 0 ) {
        close(ifd);
        fprintf(stderr, "ERROR: Can not open output file for write: %s\n", strerror(errno));
        return 1;
    }
    
    /* check file size */
    totalsize = lseek64(ifd, 0, SEEK_END);
    lseek(ifd, 0, SEEK_SET);
    
    
    while(1)
    {
        char buf[BLOCKSIZE];
        
        int r = read(ifd, buf, BLOCKSIZE);
        if( r < 1 ) {
            if( r < 0 ) 
                fprintf(stderr, "Can not read: %s\n", strerror(errno));
            break;
        }
        readed += r;
        
        /* save time */
        wtime = timestamp(); 
        
        int w = write(ofd, buf, r);
        if( w < 1 ) {
            if( w < 0 ) 
                fprintf(stderr, "Can not write: %s\n", strerror(errno));
            break;
        }
        writed += w;
        
        /* increment by diff */
        totaltime += (timestamp() - wtime);
        
        printf("\r -> written: %lu MB at %.02f MB/s, %.02f%% ", MB(writed), ((float)KB(w) / (timestamp() - wtime)), 
               totalsize > 0? (float)writed * 100.0 / totalsize : 0);
        fflush(stdout);
    }
    
    printf("\n\n");
    printf_info(" => OK!\n");
    printf("%lu Mb => %lu Mb, %lu s, %.02f MB/s\n", 
           MB(readed), MB(writed), MS(totaltime), (writed? ((float)KB(writed) / (totaltime)) : 0));
    
    close(ifd);
    close(ofd);
    return 0;
}

