#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>                                               /* Low level I/O를 위해서 사용 */
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>                                     /* <videodev2.h> 파일을 위해서 사용 */
#include <linux/videodev2.h>                             /* Video4Linux2를 위한 헤더파일 */
#include <linux/fb.h>
#include "bmpHeader.h"
#include <unistd.h>


// udp server....
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>

//motor
#include "motor.h"


#define MEMZERO(x) memset(&(x), 0, sizeof(x))    /* 변수 초기화를 위한 매크로 */

#define VIDEODEV        "/dev/video0"               /* Pi Camera를 위한 디바이스 파일 */
#define FBDEV               "/dev/fb0"                    /* 프레이버퍼를 위한 디바이스 파일 */
#define WIDTH               100        /* 캡쳐받을 영상의 크기 */
#define HEIGHT           75 
#define NUMCOLOR		3

/* Video4Linux에서 사용할 영상 저장을 위한 버퍼 */
struct buffer {
	void * start;
	size_t length;
};

static int fd = -1;                                                  /* Pi Camera의 장치의 파일 디스크립터 */
struct buffer *buffers = NULL;                             /* Pi Camera의MMAP를 위한 변수 */
static int fbfd = -1;                                               /* 프레임버퍼의 파일 디스크립터 */
static short *fbp = NULL;                                     /* 프레임버퍼의 MMAP를 위한 변수 */
static struct fb_var_screeninfo vinfo;                   /* 프레임버퍼의 정보 저장을 위한 구조체 */

//udp_server...
pthread_t tid[2];

unsigned int serv_port;
#define MAXLINE 4097
#define FILENAME "capture.bmp"
#define BLOCK 4096
pthread_mutex_t a_mutex = PTHREAD_MUTEX_INITIALIZER;

static void processImage(const void *p);
static int readFrame();
static void initRead(unsigned int buffer_size);
void saveImage(unsigned char *inimg);
void* thread_serv(void* arg) ;

static void initDevice()
{   
	struct v4l2_capability cap;                                 /* 비디오 장치에 대한 기능을 조사한다. */
	struct v4l2_format fmt;
	unsigned int min;

	if(ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {       /* 장치가 V4L2를 지원하는지 조사 */
		if(errno == EINVAL) {
			perror("/dev/video0 is no V4L2 device");
			exit(EXIT_FAILURE);
		} else {
			perror("VIDIOC_QUERYCAP");
			exit(EXIT_FAILURE);
		}
	}

	/* 장치가 영상 캡쳐 기능이 있는지 조사 */
	if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		perror("/dev/video0 is no video capture device");
		exit(EXIT_FAILURE);
	}

	MEMZERO(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = WIDTH;
	fmt.fmt.pix.height = HEIGHT;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;

	if(ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
		perror("VIDIOC_S_FMT");
		exit(EXIT_FAILURE);
	}

	/* 영상의 최소 크기를 구함 */
	min = fmt.fmt.pix.width * 2;
	if(fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if(fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	/* 영상 읽기를 위한 초기화 */
	initRead(fmt.fmt.pix.sizeimage); 
}

static void initRead(unsigned int buffer_size) 
{
	/* 메모리를 할당한다 */
	buffers = (struct buffer*)calloc(1, sizeof(*buffers));
	if(!buffers) {
		perror("Out of memory");
		exit(EXIT_FAILURE);
	}

	/* 버퍼를 초기화 한다. */
	buffers[0].length = buffer_size;
	buffers[0].start = malloc(buffer_size);
	if(!buffers[0].start) {
		perror("Out of memory");
		exit(EXIT_FAILURE);
	}
} 

#define NO_OF_LOOP    1000

static void mainloop() 
{
	unsigned int count = NO_OF_LOOP;
	//while(count-- > 0) {                                                /* 100회 반복 */
	while(1) {                                                /* 100회 반복 */
		for(;;) {
			fd_set fds;
			struct timeval tv;
			int r;

			/* fd_set 구조체를 초기화하고 비디오 장치를 파일 디스크립터를 설정한다. */
			FD_ZERO(&fds);
			FD_SET(fd, &fds);

			/* 타임아웃(Timeout)을 2초로 설정한다. */
			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select(fd + 1, &fds, NULL, NULL, &tv);      /* 비디오 데이터가 올때까지 대기 */
			switch(r) {
				case -1:                                                     /* select( ) 함수 에러시의 처리 */
					if(errno != EINTR) {
						perror("select( )");
						exit(EXIT_FAILURE);
					}
					break;
				case 0:                                                     /* 타임아웃시의 처리 */
					perror("select timeout");
					exit(EXIT_FAILURE);
					break;
			};

			if(readFrame()) break;                                 /* 현재의 프레임을 읽어서 표시 */
		};
	};
}

static int readFrame( ) 
{

	pthread_mutex_lock(&a_mutex);
	if(read(fd, buffers[0].start, buffers[0].length) < 0) {
		perror("read( )");
		exit(EXIT_FAILURE);
	}

	processImage(buffers[0].start);
	pthread_mutex_unlock(&a_mutex);
	return 1;
}

/* unsigned char의 범위를 넘어가지 않도록 경계 검사를 수행다. */
inline int clip(int value, int min, int max)
{
	return(value > max ? max : value < min ? min : value);
}

/* YUYV를 RGB16으로 변환한다. */
static void processImage(const void *p)
{
	unsigned char* in =(unsigned char*)p;
	int width = WIDTH;
	int height = HEIGHT;

	// image save... var
	unsigned char inimg[3*WIDTH*HEIGHT];
	int count=0;

	unsigned short pixel;
	int istride = WIDTH*2;          /* 이미지의 폭을 넘어가면 다음 라인으로 내려가도록 설정 */
	int x, y, j;
	int y0, u, y1, v, r, g, b;
	long location = 0;
	for(y = 0; y < height; ++y) {
		for(j = 0, x = 0; j < vinfo.xres * 2; j += 4, x += 2) {
			if(j >= width*2) {                 /* 현재의 화면에서 이미지를 넘어서는 빈 공간을 처리 */
				location++; location++; continue;
			}
			/* YUYV 성분을 분리 */
			y0 = in[j];
			u = in[j + 1] - 128;
			y1 = in[j + 2];
			v = in[j + 3] - 128;

			/* YUV를 RGB로 전환 */
			r = clip((298 * y0 + 409 * v + 128) >> 8, 0, 255);
			g = clip((298 * y0 - 100 * u - 208 * v + 128) >> 8, 0, 255);
			b = clip((298 * y0 + 516 * u + 128) >> 8, 0, 255);

			//BMP image save
			inimg[(height-y-1)*width*3+count++] = b;
			inimg[(height-y-1)*width*3+count++] = g;
			inimg[(height-y-1)*width*3+count++] = r;

			pixel =((r>>3)<<11)|((g>>2)<<5)|(b>>3);                 /* 16비트 컬러로 전환 */
			fbp[location++] = pixel;


			/* YUV를 RGB로 전환 */
			r = clip((298 * y1 + 409 * v + 128) >> 8, 0, 255);
			g = clip((298 * y1 - 100 * u - 208 * v + 128) >> 8, 0, 255);
			b = clip((298 * y1 + 516 * u + 128) >> 8, 0, 255);
			//BMP image save
			inimg[(height-y-1)*width*3+count++] = b;
			inimg[(height-y-1)*width*3+count++] = g;
			inimg[(height-y-1)*width*3+count++] = r;
			pixel =((r>>3)<<11)|((g>>2)<<5)|(b>>3);                 /* 16비트 컬러로 전환 */
			fbp[location++] = pixel;
		};
		in += istride;
		count=0;
	};
	saveImage(inimg);
}

static void uninitDevice() 
{
	long screensize = vinfo.xres * vinfo.yres * 2;

	free(buffers[0].start);
	free(buffers);
	munmap(fbp, screensize);
}

int main(int argc, char ** argv)
{
	long screensize = 0;
	if(argc != 2) { 
		printf("usage: %s port\n", argv[0]);
		exit(0);
	}
	serv_port = atoi(argv[1]);

	/* 장치 열기 */
	/* Pi Camera 열기 */
	fd = open(VIDEODEV, O_RDWR| O_NONBLOCK, 0);
	if(fd == -1) {
		perror("open( ) : video devive");
		return EXIT_FAILURE;
	}

	/* 프레임버퍼 열기 */
	fbfd = open(FBDEV, O_RDWR);
	if(fbfd == -1) {
		perror("open( ) : framebuffer device");
		return EXIT_FAILURE;
	}

	/* 프레임버퍼의 정보 가져오기 */
	if(ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
		perror("Error reading variable information.");
		exit(EXIT_FAILURE);
	}

	/* mmap( ) : 프레임버퍼를 위한 메모리 공간 확보 */
	screensize = vinfo.xres * vinfo.yres * 2;
	fbp = (short *)mmap(NULL, screensize, PROT_READ | PROT_WRITE, 
			MAP_SHARED, fbfd, 0);
	if((int)fbp == -1) {
		perror("mmap() : framebuffer device to memory");
		return EXIT_FAILURE;
	}
	memset(fbp, 0, screensize);

	initDevice();                                                        /* 장치 초기화 */ 
	pthread_create(&tid[0], NULL, &thread_serv,NULL);
	pthread_create(&tid[1], NULL, &thread_motor,NULL);
	mainloop();     /* 캡쳐 실행 */
	while(1) {
		pthread_join(tid[0],NULL);
		pthread_join(tid[1],NULL);
		printf("thread joined..\n");
		break;
	}
	uninitDevice();                                                    /* 장치 해제 */

	/* 장치 닫기 */
	close(fbfd);
	close(fd);

	return EXIT_SUCCESS;                                    /* 애플리케이션 종료 */
}


#define widthbytes(bits)   (((bits)+31)/32*4)  // for padding bit

void saveImage(unsigned char *inimg)
{
	RGBQUAD palrgb[256];
	FILE *fp;
	int imgfd;
	BITMAPFILEHEADER bmpHeader;
	BITMAPINFOHEADER bmpInfoHeader;

	MEMZERO(bmpHeader);
	bmpHeader.bfType[0] = 'B';
	bmpHeader.bfType[1] = 'M';
	bmpHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bmpHeader.bfSize += sizeof(RGBQUAD) * 256;
	bmpHeader.bfSize += WIDTH*HEIGHT*NUMCOLOR;
	bmpHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER); // 54;
	bmpHeader.bfOffBits += sizeof(RGBQUAD) * 256; 

	MEMZERO(bmpInfoHeader);
	bmpInfoHeader.biSize = sizeof(BITMAPINFOHEADER); //40;
	bmpInfoHeader.biWidth = WIDTH;
	bmpInfoHeader.biHeight = HEIGHT;
	bmpInfoHeader.biPlanes = 1;
	bmpInfoHeader.biBitCount = NUMCOLOR*8;
	bmpInfoHeader.SizeImage = WIDTH*HEIGHT*bmpInfoHeader.biBitCount/8;
	bmpInfoHeader.biXPelsPerMeter = 0x0B12;
	bmpInfoHeader.biYPelsPerMeter = 0x0B12;

	if((fp = fopen("capture.bmp", "wb")) == NULL) {
		fprintf(stderr, "Error : Failed to open file...\n");
		exit(EXIT_FAILURE);
	}

	/* BMP 파일(BITMAPFILEHEADER) 정보 저장 */ 
	fwrite((void*)&bmpHeader, sizeof(bmpHeader), 1, fp);

	/* BMP 이미지(BITMAPINFOHEADER) 정보 저장 */ 
	fwrite((void*)&bmpInfoHeader, sizeof(bmpInfoHeader), 1, fp);

	/////////////////////////// RGBQUAD
	fwrite(palrgb, sizeof(RGBQUAD), 256, fp);

	/////////////////////////// Image Data
	fwrite(inimg, sizeof(unsigned char), WIDTH*HEIGHT*3, fp);

	fclose(fp);
}
void* thread_serv(void* arg) {
	struct sockaddr_in servaddr, cliaddr;
	int s, nbyte, addrlen = sizeof(struct sockaddr);
	char buf[MAXLINE+1];
	int bmpfd;

	if((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket fail");
		exit(0);
	}

	memset(&cliaddr, 0, addrlen);
	memset(&servaddr, 0, addrlen);
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons((serv_port)); 

	if(bind(s, (struct sockaddr *)&servaddr, addrlen) < 0) {
		perror("bind fail");
		exit(0);
	}


	// infinite loop
	while(1)
	{
		//puts("Server : waiting request.");
		//block .... revfrom...
		nbyte = recvfrom(s, buf, BLOCK, 0, (struct sockaddr *)&cliaddr, &addrlen);
		printf("nbyte:%d\n",nbyte);
		if(nbyte< 0) {
			perror("recvfrom fail");
			exit(1);
		}
		buf[nbyte] = 0; //마지막 값에 0


		//puts("sendto complete");

		// open buf.txt open and read..and resend to client
		/* send !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
		if((bmpfd = open(FILENAME, O_RDONLY))<0 ) {
			printf("Read file error");
			exit(1);
		}
		pthread_mutex_lock(&a_mutex);
		int n_size,i;
		while(0 <(n_size = read(bmpfd, buf, BLOCK))) {
			//메시지 전송
			if(sendto(s, buf, BLOCK, 0,
						(struct sockaddr *)&cliaddr, addrlen) < 0) {
				perror("sendto fail");
				exit(0);
			}
		}
		close(bmpfd);
		pthread_mutex_unlock(&a_mutex);

	}
	close(s);
	return 0;

}
