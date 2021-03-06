#include <stdio.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/times.h>
#include <sys/time.h>
#endif

#include <time.h>
#include "sha256.h"
#include "haraka.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdint.h> // portable: uint64_t   MSVC: __int64 

// MSVC defines this in winsock2.h!?
typedef struct timeval {
    long tv_sec;
    long tv_usec;
} timeval;

int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
    // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
    // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
    // until 00:00:00 January 1, 1970 
    static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime( &system_time );
    SystemTimeToFileTime( &system_time, &file_time );
    time =  ((uint64_t)file_time.dwLowDateTime )      ;
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec  = (long) ((time - EPOCH) / 10000000L);
    tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
    return 0;
}
#endif

int main(int argc, char *argv[])
{

  printf("VerusHash Bruteforcer v0.01 by \x1B[01;32mDecker\x1B[0m (q) 2018\n\n");
  printf("[+] It's just a beginning ... \n");
  printf("[*] NTHREAD.%d \n", NTHREAD);

  typedef unsigned int beu32;

  cudaError_t err;
  int         device = (argc == 1) ? 0 : atoi(argv[1]);
 
  cudaDeviceProp props;
  err = cudaGetDeviceProperties(&props,device);
 
  if (err) 
    return -1;
 
  printf("%s (%2d)\n",props.name,props.multiProcessorCount);
 
  cudaSetDevice(device);
  //cudaDeviceSetCacheConfig(cudaFuncCachePreferL1);
  //cudaDeviceReset();
  //cudaSetDeviceFlags(cudaDeviceScheduleBlockingSync);

  char *message_cuda = NULL;
  char *message = "The quick brown fox jumps over the lazy dog";
  unsigned char dgst[32];
  unsigned char *dgst_cuda = NULL;

  memset(dgst, 0, sizeof(dgst));

  cudaMalloc(&message_cuda,strlen(message)+1);
  cudaMalloc(&dgst_cuda,sizeof(dgst));

  cudaMemcpy(message_cuda,message,strlen(message)+1, cudaMemcpyHostToDevice);
  cudaMemcpy(dgst_cuda,dgst,sizeof(dgst), cudaMemcpyHostToDevice);
  
  SHA256<<<1,1>>>(message_cuda, strlen(message), dgst_cuda);
  err = cudaDeviceSynchronize();
 
  if (err) {
    printf("Err = %d\n",err);
    exit(err);
  }

  cudaMemcpy(dgst,dgst_cuda, sizeof(dgst), cudaMemcpyDeviceToHost);

  char outputBuffer[65];
  int i = 0; 
  for(i = 0; i < 32; i++)
    {
        sprintf(outputBuffer + (i * 2), "%02x", dgst[i]);
    }
  outputBuffer[64] = 0;
  printf("sha256 dgst: %s\n", outputBuffer); 


  /* Check VerusHash */

  int j,k;
  unsigned char blockheader_template[1488] = {
    0x04, 0x00, 0x00, 0x00, // version
    0xa5, 0x4b, 0xe4, 0xca, 0x85, 0x1c, 0xb1, 0x9b, 0x00, 0x0d, 0x5e, 0x03, 0xf3, 0x28, 0xe0, 0x2c, 0x97, 0xbd, 0xae, 0x37, 0x89, 0xe4, 0x73, 0x54, 0xda, 0xf4, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, // previous block hash
    0x61, 0x43, 0xfd, 0xcc, 0x47, 0x63, 0xd0, 0xa3, 0x8f, 0x7d, 0xc3, 0xc1, 0x2d, 0x77, 0x62, 0x53, 0x1c, 0xcd, 0x18, 0xff, 0x67, 0xbd, 0xb0, 0x7c, 0xac, 0x66, 0x1e, 0x9c, 0x30, 0x64, 0x29, 0x6e, // merkle root
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // reserved
    0x9d, 0xc8, 0x29, 0x5b, // timestamp
    0xcc, 0x45, 0x01, 0x1c, // nbits
    0xe9, 0x5b, 0x04, 0x1d, 0xe2, 0xb7, 0x68, 0x5f, 0x14, 0xd1, 0x71, 0x9c, 0x99, 0x84, 0xdb, 0xdf, 0x7e, 0x51, 0xe5, 0x7f, 0xc3, 0xea, 0xe4, 0x1d, 0x76, 0xe8, 0x5d, 0xfa, 0xaf, 0xa3, 0x00, 0x00, // nonce
    0xfd, 0x40, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // solution

  blockheader_template[1487] = 0; // fill last array element as 0 

  /*
  unsigned char *blockheader_template_cuda = NULL;
  unsigned char verusdgst[32 * NTHREAD];
  unsigned char *verusdgst_cuda = NULL;
  memset(verusdgst, 0, sizeof(verusdgst));
  cudaMalloc(&blockheader_template_cuda, 1488 * NTHREAD);
  cudaMalloc(&verusdgst_cuda, 32 * NTHREAD);
  cudaMemcpy(blockheader_template_cuda, blockheader_template, sizeof(blockheader_template), cudaMemcpyHostToDevice);
  cudaMemcpy(verusdgst_cuda,verusdgst,sizeof(verusdgst), cudaMemcpyHostToDevice);
  VerusHash_GPU<<<BLOCKS,THREADS>>>(verusdgst_cuda, blockheader_template_cuda); // assuming data lenght as 1488 (blockheader)
  err = cudaDeviceSynchronize();
 
  if (err) {
    printf("Err = %d\n",err);
    exit(err);
  }

  cudaMemcpy(verusdgst,verusdgst_cuda, 32 * NTHREAD, cudaMemcpyDeviceToHost);

  for(i = 0; i < 32; i++)
    {
        sprintf(outputBuffer + (i * 2), "%02x", verusdgst[i]);
    }
  outputBuffer[64] = 0;
  printf("verushash dgst: %s\n", outputBuffer); 
  printf("              : fcbf0e05b0f030d1c24285701e2fe719d8c10455e36e9febb5759a0000000000\n");
  */


  unsigned char *blockheaders_arr = NULL;
  unsigned char *blockheaders_arr_cuda = NULL;
  unsigned char *verusdgst_arr = NULL;
  unsigned char *verusdgst_arr_cuda = NULL;

  // ToDo: https://devblogs.nvidia.com/how-optimize-data-transfers-cuda-cc/

  // blockheaders_arr = (unsigned char *) malloc (1488 * NTHREAD);
  cudaMallocHost((void**)&blockheaders_arr, 1488 * NTHREAD);

  cudaMalloc(&blockheaders_arr_cuda, 1488 * NTHREAD);
  // verusdgst_arr = (unsigned char *) malloc (32 * NTHREAD);
  cudaMallocHost((void**)&verusdgst_arr, 32 * NTHREAD);

  cudaMalloc(&verusdgst_arr_cuda, 1488 * NTHREAD);

  unsigned char nOnceStart[32];
  unsigned char      nOnce[32];

  time_t t;
  srand((unsigned) time(&t));
  struct timeval  tv1, tv2;
  gettimeofday(&tv1, NULL);

  //cudaFuncSetCacheConfig(VerusHash_GPU, cudaFuncCachePreferShared);
  //cudaFuncSetCacheConfig(VerusHash_GPU, cudaFuncCachePreferL1);


  for (k=0; k < 16 * 1000000 / NTHREAD; k++) { // main test loop

  for (j=0; j<32; j++) nOnceStart[j] = rand() & 0xFF;
  printf("nOnceStart [%d] = ", k * NTHREAD); for (j=0; j<32; j++) printf("%02x", nOnceStart[j]); printf("\n");

  for (i=0; i<NTHREAD; i++) {
        // blockheader_template[0] = i; // just for test
	
  	memcpy(blockheaders_arr + i * 1488, blockheader_template, 1488);
        // don't forget to set nonce
        if (0) {
        memcpy(blockheaders_arr + i * 1488 + 4+32+32+32+4+4, nOnceStart, 32);
        blockheaders_arr[i * 1488 + 4+32+32+32+4+4] = i & 0xFF;
        blockheaders_arr[i * 1488 + 4+32+32+32+4+4+1] = (i >> 8) & 0xFF;
        }
  	memset(verusdgst_arr + i * 32, 0, 32);
  }

  if (0) {
  for (i=0; i<NTHREAD; i++) {
	printf("data[%02d] = ",i);
	for (j=0; j<1488; j++) printf("%02x",*(blockheaders_arr + i * 1488 + j));
	printf("\n");
	printf("hash[%02d] = ",i);
	for (j=0; j<32; j++) printf("%02x",*(verusdgst_arr + i * 32 + j));
	printf("\n");
  }
  }


  //cudaMemcpy(blockheaders_arr_cuda, blockheaders_arr, 1488 * NTHREAD, cudaMemcpyHostToDevice);

  // http://cuda-programming.blogspot.com/2013/01/what-is-constant-memory-in-cuda.html
  cudaMemcpy(blockheaders_arr_cuda, blockheaders_arr, 1488 * NTHREAD, cudaMemcpyHostToDevice);


  //cudaMemcpy(verusdgst_arr_cuda, verusdgst_arr, 32 * NTHREAD, cudaMemcpyHostToDevice);
  cudaMemset(verusdgst_arr_cuda, 0, 32 * NTHREAD);

  VerusHash_GPU<<<BLOCKS,THREADS>>>(verusdgst_arr_cuda, blockheaders_arr_cuda);

  err = cudaDeviceSynchronize();
  if (err) {
    printf("Err = %d\n",err);
    exit(err);
  }

  cudaMemcpy(verusdgst_arr, verusdgst_arr_cuda, 32 * NTHREAD, cudaMemcpyDeviceToHost);

  // here we should check results (!!!)

  } // main test loop

  // print first 5 hashes from last iteration
  for (i=0; i<5; i++) {
	if (0) {
	printf("data[%02d] = ",i);
	for (j=0; j<1488; j++) printf("%02x",*(blockheaders_arr + i * 1488 + j));
	printf("\n"); }
	printf("hash[%02d] = ",i);
	for (j=0; j<32; j++) printf("%02x",*(verusdgst_arr + i * 32 + j));
	printf("\n");
  }

  gettimeofday(&tv2, NULL);
  printf ("Total time = %f seconds\n",
         (double) (tv2.tv_usec - tv1.tv_usec) / 1000000 +
         (double) (tv2.tv_sec - tv1.tv_sec));


  /* Example of acquire data in kernel */
  
  /*
  uint32_t data;
  uint32_t *data_cuda;

  data = 0xdeadc0de;

  printf("\n");
  cudaMalloc(&data_cuda, sizeof(data) * NTHREAD);
  for (int i=0; i < NTHREAD; i++) {
  	  data = 0xdead0000 | i;
  	  printf("[Host] tid = %02d, data = 0x%08x\n", i, data);
	  cudaMemcpy(&data_cuda[i],&data,sizeof(data), cudaMemcpyHostToDevice);
  }

  functest<<<BLOCKS,THREADS>>>(data_cuda);
  err = cudaDeviceSynchronize();
  if (err) { printf("Err = %d\n",err);  exit(err); }
  */

}