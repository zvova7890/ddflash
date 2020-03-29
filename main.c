
#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fs.h>


#define KB(x)       (x? (x / 1024) : 0)
#define MB(x)       (x? (x / 1024 / 1024) : 0)
#define MS(x)       (x? (x / 1000) : 0)



uint64_t timestamp()
{
	struct timeval tv = {0};
	gettimeofday(&tv, NULL);
	
	return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}


uint64_t getSizeByDescriptor(int fd)
{
	uint64_t size = 0;
	
	/* try first as block device */
	ioctl(fd, BLKGETSIZE64, &size);
    
    /* then as file */
    if( size < 1 ) {
		size = lseek64(fd, 0, SEEK_END);
		lseek(fd, 0, SEEK_SET);
	}
	
	return size;
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
	uint64_t rwtime, rwtimeend;
	uint64_t totaltime = 0;
	uint64_t readed = 0;
	uint64_t writed = 0;
	uint64_t totalsize = 0;
	float    currentSpeed = -1;
	uint64_t lastPrintTime = 0;
	int      ifd = -1, ofd = -1;
	char *   buf = NULL;

	
	while(1) {
		printf(" Attention! Did you accept to write \"%s\"? [y/n]: ", ofile);
		int is = getc(stdin);
		if( is != 'n' && is != 'y' ) {
			printf("Choise y or n\n");
			continue;
		}
		
		if( is == 'n' ) {
			printf("Aborted\n");
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
	totalsize = getSizeByDescriptor(ifd);
	printf("\n");
	
	printf(" Input size: %lu MB\n", MB(totalsize));
	printf(" Buffer size: %d MB\n", MB(BLOCKSIZE));
	
	printf("\n");
	buf = malloc(BLOCKSIZE);
	
	while(1)
	{
		/* save time */
		rwtime = timestamp(); 
		
		int r = read(ifd, buf, BLOCKSIZE);
		if( r < 1 ) {
			if( r < 0 ) 
				fprintf(stderr, "Can not read: %s\n", strerror(errno));
			break;
		}
		readed += r;
		
		
		int64_t w = write(ofd, buf, r);
		if( w < 1 ) {
			if( w < 0 ) 
				fprintf(stderr, "Can not write: %s\n", strerror(errno));
			break;
		}
		writed += w;
		
		rwtimeend = timestamp(); 
		
		/* increment by diff */
		totaltime += (rwtimeend - rwtime);
		
		if( currentSpeed == -1 )
			currentSpeed = ((float)KB(w) / (rwtimeend - rwtime));
		else
			currentSpeed = (currentSpeed + ((float)KB(w) / (rwtimeend - rwtime))) / 2;
		
		if( rwtimeend - lastPrintTime > 500 ) {
			printf("\r =>: %lu MB at %.02f MB/s, %.02f%%      ", MB(writed), currentSpeed, 
				totalsize > 0? (float)writed * 100.0 / totalsize : 0);
			fflush(stdout);
			
			lastPrintTime = rwtimeend;
		}
	}
	
	
	printf("\n\n ========= OK ========== \n%lu Mb => %lu Mb, %lu s, %.02f MB/s\n", 
		MB(readed), MB(writed), MS(totaltime), (writed? ((float)KB(writed) / (totaltime)) : 0));
	
	if( buf )
		free(buf);
	
	close(ifd);
	close(ofd);
	return 0;
}

