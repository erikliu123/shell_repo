#include <stdlib.h>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
typedef union
{
  struct
  {
    unsigned char r,g,b,a;//b, g, r, a; /*if stay same, decompress image will have problem*/
  };
  unsigned int u;
} Color32;

typedef union
{
  struct
  {
    unsigned short b : 5;
    unsigned short g : 6;
    unsigned short r : 5;
  };
  unsigned short u;
} Color16;
// int func(void);
typedef struct
{
  Color16 col0;
  Color16 col1;
  union
  {
    unsigned char row[4];
    unsigned int indices;
  };
  // void decompress(Color32 colors[16]) const;
} BlockDXT1;

#define MAX_CUDA_PICTURES 10
struct cudaPreAlloc
{
  struct {
    Color32 *compressResult[MAX_CUDA_PICTURES];//1MB
    BlockDXT1 *inputImage[MAX_CUDA_PICTURES];//8MB
    //BlockDXT1 *inputImageArray;//8MB
    void *devHostDataToDevice;
  } compressTask;

  struct {
    Color32 *decompressResult[MAX_CUDA_PICTURES];//8MB
    BlockDXT1 *inputImage[MAX_CUDA_PICTURES];//1MB
  } decompressTask;

};

/*
    图像压缩/解压缩相关
*/
#if !defined(MAKEFOURCC)
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                \
  ((unsigned int)(ch0) | ((unsigned int)(ch1) << 8) | \
   ((unsigned int)(ch2) << 16) | ((unsigned int)(ch3) << 24))
#endif

typedef unsigned int uint;
typedef unsigned short ushort;

struct DDSPixelFormat {
  uint size;
  uint flags;
  uint fourcc;
  uint bitcount;
  uint rmask;
  uint gmask;
  uint bmask;
  uint amask;
};

struct DDSCaps {
  uint caps1;
  uint caps2;
  uint caps3;
  uint caps4;
};

/// DDS file header.
struct DDSHeader {
  uint fourcc;
  uint size;
  uint flags;
  uint height;
  uint width;
  uint pitch;
  uint depth;
  uint mipmapcount;
  uint reserved[11];
  DDSPixelFormat pf;
  DDSCaps caps;
  uint notused;
};

static const uint FOURCC_DDS = MAKEFOURCC('D', 'D', 'S', ' ');
static const uint FOURCC_DXT1 = MAKEFOURCC('D', 'X', 'T', '1');
static const uint DDSD_WIDTH = 0x00000004U;
static const uint DDSD_HEIGHT = 0x00000002U;
static const uint DDSD_CAPS = 0x00000001U;
static const uint DDSD_PIXELFORMAT = 0x00001000U;
static const uint DDSCAPS_TEXTURE = 0x00001000U;
static const uint DDPF_FOURCC = 0x00000004U;
static const uint DDSD_LINEARSIZE = 0x00080000U;

void decompress_simple(BlockDXT1 *dxt, Color32 *colors, int width)
{
  Color32 palette[4];
  Color16 col0 = dxt->col0;
  Color16 col1 = dxt->col1;
  // Does bit expansion before interpolation.
  palette[0].b = (col0.b << 3) | (col0.b >> 2);
  palette[0].g = (col0.g << 2) | (col0.g >> 4);
  palette[0].r = (col0.r << 3) | (col0.r >> 2);
  palette[0].a = 0xFF;

  palette[1].r = (col1.r << 3) | (col1.r >> 2);
  palette[1].g = (col1.g << 2) | (col1.g >> 4);
  palette[1].b = (col1.b << 3) | (col1.b >> 2);
  palette[1].a = 0xFF;

  if (col0.u > col1.u)
  {
    // Four-color block: derive the other two colors.
    palette[2].r = (2 * palette[0].r + palette[1].r) / 3;
    palette[2].g = (2 * palette[0].g + palette[1].g) / 3;
    palette[2].b = (2 * palette[0].b + palette[1].b) / 3;
    palette[2].a = 0xFF;

    palette[3].r = (2 * palette[1].r + palette[0].r) / 3;
    palette[3].g = (2 * palette[1].g + palette[0].g) / 3;
    palette[3].b = (2 * palette[1].b + palette[0].b) / 3;
    palette[3].a = 0xFF;
  }
  else
  {
    // Three-color block: derive the other color.
    palette[2].r = (palette[0].r + palette[1].r) / 2;
    palette[2].g = (palette[0].g + palette[1].g) / 2;
    palette[2].b = (palette[0].b + palette[1].b) / 2;
    palette[2].a = 0xFF;

    palette[3].r = 0x00;
    palette[3].g = 0x00;
    palette[3].b = 0x00;
    palette[3].a = 0x00;
  }

  for (int i = 0; i < 16; i++)
  {
    colors[i / 4 * width + i % 4] = palette[(dxt->indices >> (2 * i)) & 0x3];
  }
}


int main(int argc, char *argv[])
{
    double iotime = 0.0;
    uint8_t *compressImageBuffer;
    uint8_t *h_result;
    //ippSetNumThreads(8);
    //omp_set_num_threads(1);
    struct timeval tpstart, iostart, ioend, tpend;
    if (argc < 2)
    {
        std::cout << "you should specified a directory" << std::endl;
        return 0;
    }
     DIR *p;
    struct dirent *entry;
    struct stat statbuf;
    char child[512], cur_path[512];
    const char *path = argv[1];
    int blk_size = getpagesize();
    int total = 0;

     if ((p = opendir(argv[1])) == NULL)
    {
        printf("open directory in failure\n");
        return -1;
    }
    gettimeofday(&tpstart,NULL);
    while ((entry = readdir(p)) != NULL)
    {
        int fd, cnt;
        struct stat buf;
        if (!(entry->d_type & DT_DIR) && strstr(entry->d_name, ".dds")!=NULL)//
        {
            sprintf(cur_path, "%s/%s", path, entry->d_name);
        }
        else continue;
        ++total;

        unsigned char *tempbuf;
        
        //扫描某个文件夹下的所有文件
        fd = open(cur_path, O_DIRECT | O_RDONLY);
        fstat(fd, &buf);
        if (fd < 0)
            return -ENOENT;
        
        int readlen = (buf.st_size + blk_size - 1) / blk_size * blk_size;
        //测试执行时间
        posix_memalign((void **)&tempbuf, blk_size, readlen);
        gettimeofday(&iostart,NULL);
        cnt = read(fd, tempbuf, readlen);
        close(fd);
        gettimeofday(&ioend,NULL);
        //sub_req->end_io_time = spdk_get_ticks();

        DDSHeader header;
        memcpy(&header, tempbuf, sizeof(DDSHeader));
        uint w = header.width, h = header.height;
        uint W = w, H = h;
        compressImageBuffer = tempbuf + sizeof(DDSHeader);
        h_result = (uint8_t *)malloc(header.pitch * 8);
        for (uint y = 0; y < h; y += 4)
        {
          for (uint x = 0; x < w; x += 4)
          {
            uint referenceBlockIdx = ((y / 4) * (W / 4) + (x / 4));
            uint resultBeginIdx = y * W + x;
            BlockDXT1 *tmp = ((BlockDXT1 *)compressImageBuffer) + referenceBlockIdx;
            Color32 *tmpColor = ((Color32 *)h_result) + resultBeginIdx;
            decompress_simple(tmp, tmpColor, W);
          }
        }
        free(h_result);
        gettimeofday(&tpend,NULL);
        iotime += 1000000*(ioend.tv_sec-iostart.tv_sec)+ioend.tv_usec-iostart.tv_usec;
    }
    double timeuse=1000000*(tpend.tv_sec-tpstart.tv_sec)+tpend.tv_usec-tpstart.tv_usec;
    printf("total task: %d, IO: %.2f, TOTAL: %.2f\n", total, iotime, timeuse);
    return 0;
   
}