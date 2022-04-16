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
#include "dxt.h"

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


void writeFile(const char* file_name, int w, int h, int compressedSize, void *h_result);

inline bool __loadPPM_from_mem(const char *file_data, unsigned char **data, unsigned int *w,
                      unsigned int *h, unsigned int *channels) {

  int offset = 0;
  // check header
  char header[0x40];
  memcpy(header, file_data + offset, 3);
  offset += 3;
  if (strncmp(header, "P5", 2) == 0) {
    *channels = 1;
  } else if (strncmp(header, "P6", 2) == 0) {
    *channels = 3;
  } else {
    std::cerr << "__LoadPPM() : File is not a PPM or PGM image" << std::endl;
    *channels = 0;
    return false;
  }

  // parse header, read maxval, width and height
  unsigned int width = 0;
  unsigned int height = 0;
  unsigned int maxval = 0;
  unsigned int i = 0;
  int n;
  while (i < 3) {
    int k = 0;
    while(!isspace(*(file_data + offset)))
    {
      header[k++] = *(file_data + offset);
      ++offset;
    }
    ++offset;
    header[k] = '\0';
    if (header[0] == '#') {
      continue;
    }

    if (i == 0) {
      i += sscanf(header, "%u %u %u", &width, &height, &maxval);
    } else if (i == 1) {
      i += sscanf(header, "%u %u", &height, &maxval);
    } else if (i == 2) {
      i += sscanf(header, "%u", &maxval);
    }
  }

  // check if given handle for the data is initialized
  if (NULL != *data) {
    if (*w != width || *h != height) {
      std::cerr << "__LoadPPM() : Invalid image dimensions." << std::endl;
    }
  } else {
    --offset;
    *data = (unsigned char *)(file_data + offset);
    *w = width;
    *h = height;
  }

  return true;
}

inline bool sdkLoadPPM4ub_from_mem(const char *file_data, unsigned char **data,
                          unsigned int *w, unsigned int *h) {
  unsigned char *idata = 0;
  unsigned int channels;

  if (__loadPPM_from_mem(file_data, &idata, w, h, &channels)) {
    // pad 4th component
    int size = *w * *h;
    // keep the original pointer
    unsigned char *idata_orig = idata;
    *data = (unsigned char *)malloc(sizeof(unsigned char) * size * 4);
    unsigned char *ptr = *data;

    for (int i = 0; i < size; i++) {
      *ptr++ = *idata++;
      *ptr++ = *idata++;
      *ptr++ = *idata++;
      *ptr++ = 0;
    }

    //free(idata_orig);
    return true;
  } else {
    //free(idata);
    return false;
  }
}

int main(int argc, char *argv[])
{
    double iotime = 0.0;
    uint8_t *compressImageBuffer;
    uint8_t *h_result;
    //ippSetNumThreads(8);
    //omp_set_num_threads(1);
    struct timeval tpstart, iostart, ioend, tpend, start_compute_time;
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
        if (!(entry->d_type & DT_DIR) && strstr(entry->d_name, ".ppm")!=NULL)//
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
        unsigned char *src = NULL; //需要free掉
        int readlen = (buf.st_size + blk_size - 1) / blk_size * blk_size;
        //测试执行时间
        posix_memalign((void **)&tempbuf, blk_size, readlen);
        gettimeofday(&iostart,NULL);
        cnt = read(fd, tempbuf, readlen);
        close(fd);
        gettimeofday(&ioend,NULL);
        /*解析文件*/
        unsigned int W, H;
        sdkLoadPPM4ub_from_mem((const char *)tempbuf, &src, &W, &H);

        const uint compressedSize = (W / 4) * (H / 4) * 8;
        uint *compressImageBuffer = (uint *)malloc(compressedSize);

        gettimeofday(&start_compute_time, NULL);
        CompressImageDXT1((const BYTE *)src, (BYTE *)compressImageBuffer, (int)W, (int)H);
        gettimeofday(&tpend,NULL);
        double temp_time = 1000000*(tpend.tv_sec-start_compute_time.tv_sec)+tpend.tv_usec-start_compute_time.tv_usec;
        //printf("CompressImageDXT1: %.2f\n",  temp_time);
        /*写入文件*/
        writeFile(entry->d_name, W, H, compressedSize, compressImageBuffer);
        free(compressImageBuffer);
        free(src);

        iotime += 1000000*(ioend.tv_sec-iostart.tv_sec)+ioend.tv_usec-iostart.tv_usec;
    }
    double timeuse=1000000*(tpend.tv_sec-tpstart.tv_sec)+tpend.tv_usec-tpstart.tv_usec;
    printf("total task: %d, IO: %.2f, TOTAL: %.2f\n", total, iotime, timeuse);
    return 0;
   
}

void writeFile(const char* file_name, int w, int h, int compressedSize, void *h_result)
{
    // Write out result data to DDS file
  char output_filename[1024]="./tmp/";
  strcat(output_filename, file_name);
  int len = strlen(output_filename);
  strcpy(output_filename + len - 4, "_host.dds");
  FILE *fp = fopen(output_filename, "wb");

  if (fp == 0)
  {
    printf("Error, unable to open output image <%s>\n", output_filename);
    exit(EXIT_FAILURE);
  }
   DDSHeader header;
  header.fourcc = FOURCC_DDS;
  header.size = 124;
  header.flags = (DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS | DDSD_PIXELFORMAT |
                  DDSD_LINEARSIZE);
  header.height = h;
  header.width = w;
  header.pitch = compressedSize;
  header.depth = 0;
  header.mipmapcount = 0;
  memset(header.reserved, 0, sizeof(header.reserved));
  header.pf.size = 32;
  header.pf.flags = DDPF_FOURCC;
  header.pf.fourcc = FOURCC_DXT1;
  header.pf.bitcount = 0;
  header.pf.rmask = 0;
  header.pf.gmask = 0;
  header.pf.bmask = 0;
  header.pf.amask = 0;
  header.caps.caps1 = DDSCAPS_TEXTURE;
  header.caps.caps2 = 0;
  header.caps.caps3 = 0;
  header.caps.caps4 = 0;
  header.notused = 0;
  fwrite(&header, sizeof(DDSHeader), 1, fp);
  fwrite(h_result, compressedSize, 1, fp);
  fclose(fp);
}
