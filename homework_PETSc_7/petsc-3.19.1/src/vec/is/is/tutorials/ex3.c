
static char help[] = "Demonstrates creating a blocked index set.\n\n";

#include <petscis.h>
#include <petscviewer.h>

int main(int argc, char **argv)
{
  PetscInt        i, n = 4, inputindices[] = {0, 1, 3, 4}, bs = 3, issize;
  const PetscInt *indices;
  IS              set;
  PetscBool       isblock;

  PetscFunctionBeginUser;
  PetscCall(PetscInitialize(&argc, &argv, (char *)0, help));

  /*
    Create a block index set. The index set has 4 blocks each of size 3.
    The indices are {0,1,2,3,4,5,9,10,11,12,13,14}
    Note each processor is generating its own index set
    (in this case they are all identical)
  */
  PetscCall(ISCreateBlock(PETSC_COMM_SELF, bs, n, inputindices, PETSC_COPY_VALUES, &set));
  PetscCall(ISView(set, PETSC_VIEWER_STDOUT_SELF));

  /*
    Extract indices from set.
  */
  PetscCall(ISGetLocalSize(set, &issize));
  PetscCall(ISGetIndices(set, &indices));
  PetscCall(PetscPrintf(PETSC_COMM_SELF, "Printing indices directly\n"));
  for (i = 0; i < issize; i++) PetscCall(PetscPrintf(PETSC_COMM_SELF, "%" PetscInt_FMT "\n", indices[i]));
  PetscCall(ISRestoreIndices(set, &indices));

  /*
    Extract the block indices. This returns one index per block.
  */
  PetscCall(ISBlockGetIndices(set, &indices));
  PetscCall(PetscPrintf(PETSC_COMM_SELF, "Printing block indices directly\n"));
  for (i = 0; i < n; i++) PetscCall(PetscPrintf(PETSC_COMM_SELF, "%" PetscInt_FMT "\n", indices[i]));
  PetscCall(ISBlockRestoreIndices(set, &indices));

  /*
    Check if this is really a block index set
  */
  PetscCall(PetscObjectTypeCompare((PetscObject)set, ISBLOCK, &isblock));
  PetscCheck(isblock, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Index set is not blocked!");

  /*
    Determine the block size of the index set
  */
  PetscCall(ISGetBlockSize(set, &bs));
  PetscCheck(bs == 3, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Block size is not 3!");

  /*
    Get the number of blocks
  */
  PetscCall(ISBlockGetLocalSize(set, &n));
  PetscCheck(n == 4, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Number of blocks not 4!");

  PetscCall(ISDestroy(&set));
  PetscCall(PetscFinalize());
  return 0;
}

/*TEST

   test:

TEST*/
