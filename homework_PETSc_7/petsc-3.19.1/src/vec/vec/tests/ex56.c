
static char help[] = "Update the data in a VECVIENNACL via a CL kernel.\n\n";

#include <petscvec.h>
#include <CL/cl.h>

const char *kernelSrc = "\n"
                        "#pragma OPENCL EXTENSION cl_khr_fp64 : enable                 \n"
                        "__kernel void doublify(  __global double *x,                  \n"
                        "                       const unsigned int n)                  \n"
                        "{                                                             \n"
                        "  //Get our global thread ID                                  \n"
                        "  int gid = get_global_id(0);                                 \n"
                        "                                                              \n"
                        "  if (gid < n)                                                \n"
                        "    x[gid] = 2*x[gid];                                        \n"
                        "}                                                             \n"
                        "\n";

int main(int argc, char **argv)
{
  PetscInt        size = 5;
  Vec             x;
  cl_program      prg;
  cl_kernel       knl;
  PETSC_UINTPTR_T clctxptr;
  PETSC_UINTPTR_T clqueueptr;
  PETSC_UINTPTR_T clmemptr;
  const size_t    gsize = 10, lsize = 2;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, (char *)0, help));

  PetscCall(VecCreate(PETSC_COMM_WORLD, &x));
  PetscCall(VecSetSizes(x, size, PETSC_DECIDE));
  PetscCall(VecSetType(x, VECVIENNACL));
  PetscCall(VecSet(x, 42.0));

  PetscCall(VecViennaCLGetCLContext(x, &clctxptr));
  PetscCall(VecViennaCLGetCLQueue(x, &clqueueptr));
  PetscCall(VecViennaCLGetCLMem(x, &clmemptr));

  const cl_context       ctx   = ((const cl_context)clctxptr);
  const cl_command_queue queue = ((const cl_command_queue)clqueueptr);
  const cl_mem           mem   = ((const cl_mem)clmemptr);

  prg = clCreateProgramWithSource(ctx, 1, (const char **)&kernelSrc, NULL, NULL);
  clBuildProgram(prg, 0, NULL, NULL, NULL, NULL);
  knl = clCreateKernel(prg, "doublify", NULL);

  clSetKernelArg(knl, 0, sizeof(cl_mem), &mem);
  clSetKernelArg(knl, 1, sizeof(PetscInt), &size);

  // Launch the kernel. (gsize > size: masked execution of some work items)
  clEnqueueNDRangeKernel(queue, knl, 1, NULL, &gsize, &lsize, 0, NULL, NULL);
  clFinish(queue);

  // let petsc know that device data is altered
  PetscCall(VecViennaCLRestoreCLMem(x));

  // 'x' should contain 84 as all its entries
  PetscCall(VecView(x, PETSC_VIEWER_STDOUT_WORLD));

  PetscCall(VecDestroy(&x));
  clReleaseContext(ctx);
  clReleaseCommandQueue(queue);
  clReleaseMemObject(mem);
  clReleaseProgram(prg);
  clReleaseKernel(knl);

  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

   build:
      requires: viennacl

   test:
      nsize: 1
      suffix: 1
      args: -viennacl_backend opencl -viennacl_opencl_device_type gpu

TEST*/
