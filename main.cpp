#include <mpi.h>
#include <sycl.hpp>
#include <ze_api.h>

#define ROOT 0

// UTILITIES
#include "../ExaComm/CommBench/util.h"

#define Type int

int main(int argc, char *argv[])
{
  // INITIALIZE
  int myid;
  int numproc;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &myid);
  MPI_Comm_size(MPI_COMM_WORLD, &numproc);

  sycl::queue q(sycl::gpu_selector_v);
  ze_context_handle_t ctx = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(q.get_context());
  ze_device_handle_t dev = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(q.get_device());

  size_t count = 17;
  Type *buffer_d = sycl::malloc_device<Type>(count, q);
  Type *buffer_h = sycl::malloc_host<Type>(count, q);

  if(myid == 0) {
    Type value = -6;
    q.fill(buffer_d, value, count).wait();
    q.memcpy(buffer_h, buffer_d, count * sizeof(Type)).wait(); 
    for(int i = 0; i < count; i++)
      printf("rank 0 check: %d\n", buffer_h[i]);
    ze_ipc_mem_handle_t myhandle;
    int error = zeMemGetIpcHandle(ctx, buffer_d, &myhandle);
    printf("error %d\n", error);
    MPI_Send(&myhandle, sizeof(ze_ipc_mem_handle_t), MPI_BYTE, 1, 0, MPI_COMM_WORLD);
  }
  if(myid == 1) {
    ze_ipc_mem_handle_t remote_handle;
    MPI_Recv(&remote_handle, sizeof(ze_ipc_mem_handle_t), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    void *buffer_ipc = nullptr;
    int error = zeMemOpenIpcHandle(ctx, dev, remote_handle, 0, (void **) &buffer_ipc);
    printf("error %d\n", error);
    Type value = -7;
    q.fill(buffer_d, value, count).wait();
    q.memcpy(buffer_d, buffer_ipc, count * sizeof(Type)).wait(); // CRITICAL LINE
    q.memcpy(buffer_h, buffer_d, count * sizeof(Type)).wait();
    for(int i = 0; i < count; i++)
      printf("rank 1 check %d\n", buffer_h[i]);
  }

  MPI_Barrier(MPI_COMM_WORLD);

  // FINALIZE
  MPI_Finalize();

  return 0;
} // main()
