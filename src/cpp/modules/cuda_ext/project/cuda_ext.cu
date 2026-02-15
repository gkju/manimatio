#include <cuda_runtime.h>
#include "cuda_ext.h"

__global__ void add_kernel(const int* a, const int* b, int* out) { *out = *a + *b; }

extern "C" int cuda_add(int a, int b) {
    int *da=nullptr, *db=nullptr, *dout=nullptr;
    cudaMalloc(&da, sizeof(int));
    cudaMalloc(&db, sizeof(int));
    cudaMalloc(&dout, sizeof(int));
    cudaMemcpy(da, &a, sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(db, &b, sizeof(int), cudaMemcpyHostToDevice);
    add_kernel<<<1,1>>>(da, db, dout);
    int out=0;
    cudaMemcpy(&out, dout, sizeof(int), cudaMemcpyDeviceToHost);
    cudaFree(da); cudaFree(db); cudaFree(dout);
    return out;
}
