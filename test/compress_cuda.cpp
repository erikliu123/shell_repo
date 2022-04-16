
#include "ndp/helper_timer.h"
namespace cg = cooperative_groups;

template <class T>
__device__ inline void swap(T &a, T &b)
{
  T tmp = a;
  a = b;
  b = tmp;
}

inline bool __loadPPM_from_mem(const char *file_data, unsigned char **data, unsigned int *w,
                      unsigned int *h, unsigned int *channels) {

  int offset = 0;
  // check header
  char header[helper_image_internal::PGMHeaderSize];
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


static void computePermutations(uint permutations[1024]);
__global__ void compress(const uint *permutations, const uint *image,
                         uint2 *result, int blockOffset);

static int ndp_compute_compress(struct ndp_subrequest *sub_req)
{
  struct ndp_request *ndp_req = sub_req->req;
  struct spdk_nvmf_request *req = ndp_req->nvmf_req;
  struct spdk_nvme_cpl *response = &req->rsp->nvme_cpl; //可能会因为超时被释放掉
  bool cnn_flag = false;
  int sc = 0, sct = 0;
  int ret = 0;
  uint32_t cdw0 = 0;
  unsigned char *compressImageBuffer = (unsigned char *)sub_req->read_ptr + sizeof(DDSHeader);
  unsigned char *src = NULL; //需要free掉
  /*
    DDSHeader header;
   memcpy(&header, sub_req->read_ptr, sizeof(DDSHeader));
   uint w = header.width, h = header.height;
   */
 
  uint W, H;
    
  if (!sdkLoadPPM4ub_from_mem((const char *)(sub_req->read_ptr), &src, &W, &H)) {
    printf("Error, unable to open source image file <%s>\n", sub_req->read_file);

    return -EXIT_FAILURE;
  }
  uint w = W, h = H;
  const uint memSize = w * h * 4;
  uint *block_image = (uint *)malloc(memSize);
   // Convert linear image to block linear.
  for (uint by = 0; by < h / 4; by++) {
    for (uint bx = 0; bx < w / 4; bx++) {
      for (int i = 0; i < 16; i++) {
        const int x = i & 3;
        const int y = i / 4;
        block_image[(by * w / 4 + bx) * 16 + i] =
            ((uint *)src)[(by * 4 + y) * 4 * (W / 4) + bx * 4 + x];
      }
    }
  }

  const uint compressedSize = (w / 4) * (h / 4) * 8;
  uint *h_result = (uint *)malloc(compressedSize);

  // Compute permutations.
  uint permutations[1024];
  computePermutations(permutations);
  uint blocks = ((w + 3) / 4) *
                ((h + 3) / 4); // rounds up by 1 block in each dim if %4 != 0
 
  int devID;
  cudaDeviceProp deviceProp;

  // get number of SMs on this GPU
  checkCudaErrors(cudaGetDevice(&devID));
  checkCudaErrors(cudaGetDeviceProperties(&deviceProp, devID));
  uint *d_data = (uint *)gAlloc.compressTask.inputImage[sub_req->sub_id];
  uint *d_result = (uint *)gAlloc.compressTask.compressResult[sub_req->sub_id];

  checkCudaErrors(
      cudaMemcpy(d_data, block_image, memSize, cudaMemcpyHostToDevice)); //加载bmp文件, 需要预处理不可。
  // Restrict the numbers of blocks to launch on low end GPUs to avoid kernel
  // timeout
  int blocksPerLaunch = blocks; // min(blocks, 768 * deviceProp.multiProcessorCount);

  printf("Running DXT Compression on %u x %u image...\n", w, h);
  printf("\n%u Blocks, %u Threads per Block, %u Threads in Grid...\n\n", blocks,
         NUM_THREADS, blocks * NUM_THREADS);

  uint *d_permutations = (uint *)gAlloc.compressTask.d_permutations;

  checkCudaErrors(cudaMemcpy(d_permutations, permutations, 1024 * sizeof(uint),
                             cudaMemcpyHostToDevice));
  //压缩数据块
  for (int j = 0; j < (int)blocks; j += blocksPerLaunch)
  {
    compress<<<min(blocksPerLaunch, blocks - j), NUM_THREADS>>>(
        d_permutations, d_data, (uint2 *)d_result, j);
  }
  checkCudaErrors(cudaDeviceSynchronize());
  
    checkCudaErrors(
      cudaMemcpy(h_result, d_result, compressedSize, cudaMemcpyDeviceToHost));
  sub_req->end_compute_time = spdk_get_ticks(); //计算结束时间
  //打印时间戳
  SPDK_INFOLOG(ndp, "file[%s], exclude malloc, total time: %lu, SPDK malloc time: %lu , SPDK IO consume: %lu, computation time: %lu\n", sub_req->read_file, (sub_req->end_compute_time - sub_req->start_io_time) / 3500, (sub_req->start_io_time - sub_req->malloc_time) / 3500, (sub_req->end_io_time - sub_req->start_io_time) / 3500, (sub_req->end_compute_time - sub_req->end_io_time) / 3500);

  // Write out result data to DDS file
  char output_filename[1024];
  strcpy(output_filename, sub_req->read_file);
  strcpy(output_filename + strlen(sub_req->read_file) - 4, "_spdk.dds");
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
  free(h_result);
  free(src);
  free(block_image);
  return 0;
}

//__constant__ float3 kColorMetric = { 0.2126f, 0.7152f, 0.0722f };
__constant__ float3 kColorMetric = {1.0f, 1.0f, 1.0f};

inline __device__ __host__ float3 firstEigenVector(float matrix[6])
{
  // 8 iterations seems to be more than enough.

  float3 v = make_float3(1.0f, 1.0f, 1.0f);

  for (int i = 0; i < 8; i++)
  {
    float x = v.x * matrix[0] + v.y * matrix[1] + v.z * matrix[2];
    float y = v.x * matrix[1] + v.y * matrix[3] + v.z * matrix[4];
    float z = v.x * matrix[2] + v.y * matrix[4] + v.z * matrix[5];
    float m = max(max(x, y), z);
    float iv = 1.0f / m;
    v = make_float3(x * iv, y * iv, z * iv);
  }

  return v;
}

inline __device__ void colorSums(const float3 *colors, float3 *sums,
                                 cg::thread_group tile)
{
  const int idx = threadIdx.x;

  sums[idx] = colors[idx];
  cg::sync(tile);
  sums[idx] += sums[idx ^ 8];
  cg::sync(tile);
  sums[idx] += sums[idx ^ 4];
  cg::sync(tile);
  sums[idx] += sums[idx ^ 2];
  cg::sync(tile);
  sums[idx] += sums[idx ^ 1];
}

inline __device__ float3 bestFitLine(const float3 *colors, float3 color_sum,
                                     cg::thread_group tile)
{
  // Compute covariance matrix of the given colors.
  const int idx = threadIdx.x;

  float3 diff = colors[idx] - color_sum * (1.0f / 16.0f);

  // @@ Eliminate two-way bank conflicts here.
  // @@ It seems that doing that and unrolling the reduction doesn't help...
  __shared__ float covariance[16 * 6];

  covariance[6 * idx + 0] = diff.x * diff.x; // 0, 6, 12, 2, 8, 14, 4, 10, 0
  covariance[6 * idx + 1] = diff.x * diff.y;
  covariance[6 * idx + 2] = diff.x * diff.z;
  covariance[6 * idx + 3] = diff.y * diff.y;
  covariance[6 * idx + 4] = diff.y * diff.z;
  covariance[6 * idx + 5] = diff.z * diff.z;

  cg::sync(tile);
  for (int d = 8; d > 0; d >>= 1)
  {
    if (idx < d)
    {
      covariance[6 * idx + 0] += covariance[6 * (idx + d) + 0];
      covariance[6 * idx + 1] += covariance[6 * (idx + d) + 1];
      covariance[6 * idx + 2] += covariance[6 * (idx + d) + 2];
      covariance[6 * idx + 3] += covariance[6 * (idx + d) + 3];
      covariance[6 * idx + 4] += covariance[6 * (idx + d) + 4];
      covariance[6 * idx + 5] += covariance[6 * (idx + d) + 5];
    }
    cg::sync(tile);
  }

  // Compute first eigen vector.
  return firstEigenVector(covariance);
}

////////////////////////////////////////////////////////////////////////////////
// Sort colors
////////////////////////////////////////////////////////////////////////////////
__device__ void sortColors(const float *values, int *ranks,
                           cg::thread_group tile)
{
  const int tid = threadIdx.x;

  int rank = 0;

#pragma unroll

  for (int i = 0; i < 16; i++)
  {
    rank += (values[i] < values[tid]);
  }

  ranks[tid] = rank;

  cg::sync(tile);

  // Resolve elements with the same index.
  for (int i = 0; i < 15; i++)
  {
    if (tid > i && ranks[tid] == ranks[i])
    {
      ++ranks[tid];
    }
    cg::sync(tile);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Load color block to shared mem
////////////////////////////////////////////////////////////////////////////////
__device__ void loadColorBlock(const uint *image, float3 colors[16],
                               float3 sums[16], int xrefs[16], int blockOffset,
                               cg::thread_block cta)
{
  const int bid = blockIdx.x + blockOffset;
  const int idx = threadIdx.x;

  __shared__ float dps[16];

  float3 tmp;

  cg::thread_group tile = cg::tiled_partition(cta, 16);

  if (idx < 16)
  {
    // Read color and copy to shared mem.
    uint c = image[(bid)*16 + idx];

    colors[idx].x = ((c >> 0) & 0xFF) * (1.0f / 255.0f);
    colors[idx].y = ((c >> 8) & 0xFF) * (1.0f / 255.0f);
    colors[idx].z = ((c >> 16) & 0xFF) * (1.0f / 255.0f);

    cg::sync(tile);
    // Sort colors along the best fit line.
    colorSums(colors, sums, tile);

    cg::sync(tile);

    float3 axis = bestFitLine(colors, sums[0], tile); //这是？

    cg::sync(tile);

    dps[idx] = dot(colors[idx], axis);

    cg::sync(tile);

    sortColors(dps, xrefs, tile);

    cg::sync(tile);

    tmp = colors[idx];

    cg::sync(tile);

    colors[xrefs[idx]] = tmp;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Round color to RGB565 and expand
////////////////////////////////////////////////////////////////////////////////
inline __device__ float3 roundAndExpand(float3 v, ushort *w)
{
  v.x = rintf(__saturatef(v.x) * 31.0f);
  v.y = rintf(__saturatef(v.y) * 63.0f);
  v.z = rintf(__saturatef(v.z) * 31.0f);

  *w = ((ushort)v.x << 11) | ((ushort)v.y << 5) | (ushort)v.z;
  v.x *= 0.03227752766457f; // approximate integer bit expansion.
  v.y *= 0.01583151765563f;
  v.z *= 0.03227752766457f;
  return v;
}

__constant__ float alphaTable4[4] = {9.0f, 0.0f, 6.0f, 3.0f};
__constant__ float alphaTable3[4] = {4.0f, 0.0f, 2.0f, 2.0f};
__constant__ const int prods4[4] = {0x090000, 0x000900, 0x040102, 0x010402};
__constant__ const int prods3[4] = {0x040000, 0x000400, 0x040101, 0x010401};

#define USE_TABLES 1

////////////////////////////////////////////////////////////////////////////////
// Evaluate permutations
////////////////////////////////////////////////////////////////////////////////
static __device__ float evalPermutation4(const float3 *colors, uint permutation,
                                         ushort *start, ushort *end,
                                         float3 color_sum)
{
// Compute endpoints using least squares.
#if USE_TABLES
  float3 alphax_sum = make_float3(0.0f, 0.0f, 0.0f);

  int akku = 0;

  // Compute alpha & beta for this permutation.
  for (int i = 0; i < 16; i++)
  {
    const uint bits = permutation >> (2 * i);

    alphax_sum += alphaTable4[bits & 3] * colors[i];
    akku += prods4[bits & 3];
  }

  float alpha2_sum = float(akku >> 16);
  float beta2_sum = float((akku >> 8) & 0xff);
  float alphabeta_sum = float((akku >> 0) & 0xff);
  float3 betax_sum = (9.0f * color_sum) - alphax_sum;
#else
  float alpha2_sum = 0.0f;
  float beta2_sum = 0.0f;
  float alphabeta_sum = 0.0f;
  float3 alphax_sum = make_float3(0.0f, 0.0f, 0.0f);

  // Compute alpha & beta for this permutation.
  for (int i = 0; i < 16; i++)
  {
    const uint bits = permutation >> (2 * i);

    float beta = (bits & 1);

    if (bits & 2)
    {
      beta = (1 + beta) * (1.0f / 3.0f);
    }

    float alpha = 1.0f - beta;

    alpha2_sum += alpha * alpha;
    beta2_sum += beta * beta;
    alphabeta_sum += alpha * beta;
    alphax_sum += alpha * colors[i];
  }

  float3 betax_sum = color_sum - alphax_sum;
#endif

  // alpha2, beta2, alphabeta and factor could be precomputed for each
  // permutation, but it's faster to recompute them.
  const float factor =
      1.0f / (alpha2_sum * beta2_sum - alphabeta_sum * alphabeta_sum);

  float3 a = (alphax_sum * beta2_sum - betax_sum * alphabeta_sum) * factor;
  float3 b = (betax_sum * alpha2_sum - alphax_sum * alphabeta_sum) * factor;

  // Round a, b to the closest 5-6-5 color and expand...
  a = roundAndExpand(a, start);
  b = roundAndExpand(b, end);

  // compute the error
  float3 e = a * a * alpha2_sum + b * b * beta2_sum +
             2.0f * (a * b * alphabeta_sum - a * alphax_sum - b * betax_sum);

  return (0.111111111111f) * dot(e, kColorMetric);
}

static __device__ float evalPermutation3(const float3 *colors, uint permutation,
                                         ushort *start, ushort *end,
                                         float3 color_sum)
{
// Compute endpoints using least squares.
#if USE_TABLES
  float3 alphax_sum = make_float3(0.0f, 0.0f, 0.0f);

  int akku = 0;

  // Compute alpha & beta for this permutation.
  for (int i = 0; i < 16; i++)
  {
    const uint bits = permutation >> (2 * i);

    alphax_sum += alphaTable3[bits & 3] * colors[i];
    akku += prods3[bits & 3];
  }

  float alpha2_sum = float(akku >> 16);
  float beta2_sum = float((akku >> 8) & 0xff);
  float alphabeta_sum = float((akku >> 0) & 0xff);
  float3 betax_sum = (4.0f * color_sum) - alphax_sum;
#else
  float alpha2_sum = 0.0f;
  float beta2_sum = 0.0f;
  float alphabeta_sum = 0.0f;
  float3 alphax_sum = make_float3(0.0f, 0.0f, 0.0f);

  // Compute alpha & beta for this permutation.
  for (int i = 0; i < 16; i++)
  {
    const uint bits = permutation >> (2 * i);

    float beta = (bits & 1);

    if (bits & 2)
    {
      beta = 0.5f;
    }

    float alpha = 1.0f - beta;

    alpha2_sum += alpha * alpha;
    beta2_sum += beta * beta;
    alphabeta_sum += alpha * beta;
    alphax_sum += alpha * colors[i];
  }

  float3 betax_sum = color_sum - alphax_sum;
#endif

  const float factor =
      1.0f / (alpha2_sum * beta2_sum - alphabeta_sum * alphabeta_sum);

  float3 a = (alphax_sum * beta2_sum - betax_sum * alphabeta_sum) * factor;
  float3 b = (betax_sum * alpha2_sum - alphax_sum * alphabeta_sum) * factor;

  // Round a, b to the closest 5-6-5 color and expand...
  a = roundAndExpand(a, start);
  b = roundAndExpand(b, end);

  // compute the error
  float3 e = a * a * alpha2_sum + b * b * beta2_sum +
             2.0f * (a * b * alphabeta_sum - a * alphax_sum - b * betax_sum);

  return (0.25f) * dot(e, kColorMetric);
}

__device__ void evalAllPermutations(const float3 *colors,
                                    const uint *permutations, ushort &bestStart,
                                    ushort &bestEnd, uint &bestPermutation,
                                    float *errors, float3 color_sum,
                                    cg::thread_block cta)
{
  const int idx = threadIdx.x;

  float bestError = FLT_MAX;

  __shared__ uint s_permutations[160];

  for (int i = 0; i < 16; i++)
  {
    int pidx = idx + NUM_THREADS * i;

    if (pidx >= 992)
    { //为什么是992？
      break;
    }

    ushort start, end;
    uint permutation = permutations[pidx];

    if (pidx < 160)
    {
      s_permutations[pidx] = permutation;
    }

    float error =
        evalPermutation4(colors, permutation, &start, &end, color_sum);

    if (error < bestError)
    {
      bestError = error;
      bestPermutation = permutation;
      bestStart = start;
      bestEnd = end;
    }
  }

  if (bestStart < bestEnd)
  {
    swap(bestEnd, bestStart);
    bestPermutation ^= 0x55555555; // Flip indices.
  }

  cg::sync(cta); // Sync here to ensure s_permutations is valid going forward

  for (int i = 0; i < 3; i++)
  {
    int pidx = idx + NUM_THREADS * i;

    if (pidx >= 160)
    {
      break;
    }

    ushort start, end;
    uint permutation = s_permutations[pidx];
    float error =
        evalPermutation3(colors, permutation, &start, &end, color_sum);

    if (error < bestError)
    {
      bestError = error;
      bestPermutation = permutation;
      bestStart = start;
      bestEnd = end;

      if (bestStart > bestEnd)
      {
        swap(bestEnd, bestStart);
        bestPermutation ^=
            (~bestPermutation >> 1) & 0x55555555; // Flip indices.
      }
    }
  }

  errors[idx] = bestError;
}

////////////////////////////////////////////////////////////////////////////////
// Find index with minimum error
////////////////////////////////////////////////////////////////////////////////
__device__ int findMinError(float *errors, cg::thread_block cta)
{
  const int idx = threadIdx.x;
  __shared__ int indices[NUM_THREADS];
  indices[idx] = idx;

  cg::sync(cta);

  for (int d = NUM_THREADS / 2; d > 0; d >>= 1)
  {
    float err0 = errors[idx];
    float err1 = (idx + d) < NUM_THREADS ? errors[idx + d] : FLT_MAX;
    int index1 = (idx + d) < NUM_THREADS ? indices[idx + d] : 0;

    cg::sync(cta);

    if (err1 < err0)
    {
      errors[idx] = err1;
      indices[idx] = index1;
    }

    cg::sync(cta);
  }

  return indices[0];
}

////////////////////////////////////////////////////////////////////////////////
// Save DXT block
////////////////////////////////////////////////////////////////////////////////
__device__ void saveBlockDXT1(ushort start, ushort end, uint permutation,
                              int xrefs[16], uint2 *result, int blockOffset)
{
  const int bid = blockIdx.x + blockOffset;

  if (start == end)
  {
    permutation = 0;
  }

  // Reorder permutation.
  uint indices = 0;

  for (int i = 0; i < 16; i++)
  {
    int ref = xrefs[i];
    indices |= ((permutation >> (2 * ref)) & 3) << (2 * i);
  }

  // Write endpoints.
  result[bid].x = (end << 16) | start;

  // Write palette indices.
  result[bid].y = indices;
}

__global__ void compress(const uint *permutations, const uint *image,
                         uint2 *result, int blockOffset)
{
  // Handle to thread block group
  cg::thread_block cta = cg::this_thread_block();

  const int idx = threadIdx.x;

  __shared__ float3 colors[16];
  __shared__ float3 sums[16];
  __shared__ int xrefs[16];

  loadColorBlock(image, colors, sums, xrefs, blockOffset, cta);

  cg::sync(cta);

  ushort bestStart, bestEnd;
  uint bestPermutation;

  __shared__ float errors[NUM_THREADS];

  evalAllPermutations(colors, permutations, bestStart, bestEnd, bestPermutation,
                      errors, sums[0], cta);

  // Use a parallel reduction to find minimum error.
  const int minIdx = findMinError(errors, cta); //寻找损失函数最小的一个点

  cg::sync(cta);

  // Only write the result of the winner thread.
  if (idx == minIdx)
  {
    saveBlockDXT1(bestStart, bestEnd, bestPermutation, xrefs, result,
                  blockOffset);
  }
}

static void computePermutations(uint permutations[1024])
{
  int indices[16];
  int num = 0;

  // 3 element permutations:

  // first cluster [0,i) is at the start
  for (int m = 0; m < 16; ++m)
  {
    indices[m] = 0;
  }

  const int imax = 15;

  for (int i = imax; i >= 0; --i)
  {
    // second cluster [i,j) is half along
    for (int m = i; m < 16; ++m)
    {
      indices[m] = 2;
    }

    const int jmax = (i == 0) ? 15 : 16;

    for (int j = jmax; j >= i; --j)
    {
      // last cluster [j,k) is at the end
      if (j < 16)
      {
        indices[j] = 1;
      }

      uint permutation = 0;

      for (int p = 0; p < 16; p++)
      {
        permutation |= indices[p] << (p * 2);
        // permutation |= indices[15-p] << (p * 2);
      }

      permutations[num] = permutation;

      num++;
    }
  }

  assert(num == 151);

  for (int i = 0; i < 9; i++)
  {
    permutations[num] = 0x000AA555;
    num++;
  }

  assert(num == 160);

  // Append 4 element permutations:

  // first cluster [0,i) is at the start
  for (int m = 0; m < 16; ++m)
  {
    indices[m] = 0;
  }

  for (int i = imax; i >= 0; --i)
  {
    // second cluster [i,j) is one third along
    for (int m = i; m < 16; ++m)
    {
      indices[m] = 2;
    }

    const int jmax = (i == 0) ? 15 : 16;

    for (int j = jmax; j >= i; --j)
    {
      // third cluster [j,k) is two thirds along
      for (int m = j; m < 16; ++m)
      {
        indices[m] = 3;
      }

      int kmax = (j == 0) ? 15 : 16;

      for (int k = kmax; k >= j; --k)
      {
        // last cluster [k,n) is at the end
        if (k < 16)
        {
          indices[k] = 1;
        }

        uint permutation = 0;

        bool hasThree = false;

        for (int p = 0; p < 16; p++)
        {
          permutation |= indices[p] << (p * 2);
          // permutation |= indices[15-p] << (p * 2);

          if (indices[p] == 3)
            hasThree = true;
        }

        if (hasThree)
        {
          permutations[num] = permutation;
          num++;
        }
      }
    }
  }

  assert(num == 975);

  // 1024 - 969 - 7 = 48 extra elements

  // It would be nice to set these extra elements with better values...
  for (int i = 0; i < 49; i++)
  {
    permutations[num] = 0x00AAFF55;
    num++;
  }

  assert(num == 1024);
}