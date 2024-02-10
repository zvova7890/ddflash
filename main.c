
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
#define GB(x)       (x? (x / 1024 / 1024 / 1024) : 0)
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

int64_t parse_size(const char *s)
{
    char *end = 0;
    int64_t size = strtoll(s, &end, 10);
    
    switch (*end)
    {
        case 'K':
            return size * 1024;
            
        case 'M':
            return size * 1024 * 1024;
            
        case 'G':
            return size * 1024 * 1024 * 1024;
            
        case 0:
            break;
            
        default:
            printf_err("Invalid size units specified: '%c'\n", *end);
            break;
    }

    return size;
}

int64_t size_to_human_readable(int64_t size, char *units)
{
    if (size >= (1 * 1024 * 1024 * 1024)) {
        if (units) *units = 'G';
        return GB(size);
    }
    
    if (size >= (1 * 1024 * 1024)) {
        if (units) *units = 'M';
        return MB(size);
    }
    
    if (size >= (1 * 1024)) {
        if (units) *units = 'K';
        return KB(size);
    }
    
    if (units) *units = 'B';
    return size;
}


int main(int argc, char *argv[])
{
    if( argc < 3 ) {
        printf("\n  Usage: ddflash <in> <out> [<blocksize, like 1M>]\n");
        return 1;
    }
    int64_t BLOCKSIZE = (argc == 4? parse_size(argv[3]) : (1 * 1024 * 1024));
    const char *ifile = argv[1];
    const char *ofile = argv[2];
    unsigned long wtime = 0;
    unsigned long wendtime = 0;
    unsigned long totaltime = 0;
    unsigned long m_written = 0;
    unsigned long m_writetime = 0;
    unsigned long m_printperiod = 0;
    float         m_writespeed = 0;
    unsigned long readed = 0;
    unsigned long written = 0;
    off64_t totalsize = 0;
    int ifd = -1, ofd = -1;

    if (disk_info(ofile) == 0)
        printf("\n");

    while(1) {
        char units = 0;
        int64_t human_size = size_to_human_readable(BLOCKSIZE, &units);
        printf_info("Block size: %ld%c\n", human_size, units);
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
    
    ofd = open(ofile, O_WRONLY);
    if( ofd < 0 ) {
        close(ifd);
        fprintf(stderr, "ERROR: Can not open output file for write: %s\n", strerror(errno));
        return 1;
    }
    
    /* check file size */
    totalsize = lseek64(ifd, 0, SEEK_END);
    lseek(ifd, 0, SEEK_SET);
    
    char *buf = malloc(BLOCKSIZE);
    while(1)
    {
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
        
        /* flush the data */
        fdatasync(ofd);
        
        written += w;
        m_written += w;
        wendtime = timestamp();
        
        /* increment by diff */
        totaltime += (wendtime - wtime);
        m_writetime += (wendtime - wtime);
        
        if (m_printperiod + 1000 < wendtime) {
            m_printperiod = wendtime;
            
            float speed = ((float)KB(m_written) / (m_writetime));
            if (m_writespeed == 0)
                m_writespeed = speed;
            else
                m_writespeed = ((m_writespeed + speed) / 2);
            
            printf("\r -> written: %lu MB at %.02f MB/s, %.02f%% ", MB(written), m_writespeed,
                totalsize > 0? (float)written * 100.0 / totalsize : 0);
            fflush(stdout);
            
            m_written = 0;
            m_writetime = 0;
        }
    }
    
    printf("\n\n");
    printf_info(" => OK!\n");
    printf("%lu Mb => %lu Mb, %lu s, %.02f MB/s\n", 
           MB(readed), MB(written), MS(totaltime), (written? ((float)KB(written) / (totaltime)) : 0));
    
    free(buf);
    close(ifd);
    close(ofd);
    return 0;
}

