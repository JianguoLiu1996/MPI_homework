#include <petsc/private/dmdaimpl.h>
/*
   Note that the API for using PETSCVIEWERVTK is totally wrong since its use requires
   including the private vtkvimpl.h file. The code should be refactored.
*/
#include <../src/sys/classes/viewer/impls/vtk/vtkvimpl.h>

/* Helper function which determines if any DMDA fields are named.  This is used
   as a proxy for the user's intention to use DMDA fields as distinct
   scalar-valued fields as opposed to a single vector-valued field */
static PetscErrorCode DMDAGetFieldsNamed(DM da, PetscBool *fieldsnamed)
{
  PetscInt f, bs;

  PetscFunctionBegin;
  *fieldsnamed = PETSC_FALSE;
  PetscCall(DMDAGetDof(da, &bs));
  for (f = 0; f < bs; ++f) {
    const char *fieldname;
    PetscCall(DMDAGetFieldName(da, f, &fieldname));
    if (fieldname) {
      *fieldsnamed = PETSC_TRUE;
      break;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMDAVTKWriteAll_VTS(DM da, PetscViewer viewer)
{
  const char *byte_order = PetscBinaryBigEndian() ? "BigEndian" : "LittleEndian";
#if defined(PETSC_USE_REAL_SINGLE)
  const char precision[] = "Float32";
#elif defined(PETSC_USE_REAL_DOUBLE)
  const char precision[] = "Float64";
#else
  const char precision[] = "UnknownPrecision";
#endif
  MPI_Comm                 comm;
  Vec                      Coords;
  PetscViewer_VTK         *vtk = (PetscViewer_VTK *)viewer->data;
  PetscViewerVTKObjectLink link;
  FILE                    *fp;
  PetscMPIInt              rank, size, tag;
  DMDALocalInfo            info;
  PetscInt                 dim, mx, my, mz, cdim, bs, maxnnodes, maxbs, i, j, k, r;
  PetscInt64               boffset;
  PetscInt                 rloc[6], (*grloc)[6] = NULL;
  PetscScalar             *array, *array2;

  PetscFunctionBegin;
  PetscCall(PetscObjectGetComm((PetscObject)da, &comm));
  PetscCheck(!PetscDefined(USE_COMPLEX), PETSC_COMM_SELF, PETSC_ERR_SUP, "Complex values not supported");
  PetscCallMPI(MPI_Comm_size(comm, &size));
  PetscCallMPI(MPI_Comm_rank(comm, &rank));
  PetscCall(DMDAGetInfo(da, &dim, &mx, &my, &mz, NULL, NULL, NULL, &bs, NULL, NULL, NULL, NULL, NULL));
  PetscCall(DMDAGetLocalInfo(da, &info));
  PetscCall(DMGetCoordinates(da, &Coords));
  if (Coords) {
    PetscInt csize;
    PetscCall(VecGetSize(Coords, &csize));
    PetscCheck(csize % (mx * my * mz) == 0, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Coordinate vector size mismatch");
    cdim = csize / (mx * my * mz);
    PetscCheck(cdim >= dim && cdim <= 3, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Coordinate vector size mismatch");
  } else {
    cdim = dim;
  }

  PetscCall(PetscFOpen(comm, vtk->filename, "wb", &fp));
  PetscCall(PetscFPrintf(comm, fp, "<?xml version=\"1.0\"?>\n"));
  PetscCall(PetscFPrintf(comm, fp, "<VTKFile type=\"StructuredGrid\" version=\"0.1\" byte_order=\"%s\" header_type=\"UInt64\">\n", byte_order));
  PetscCall(PetscFPrintf(comm, fp, "  <StructuredGrid WholeExtent=\"%d %" PetscInt_FMT " %d %" PetscInt_FMT " %d %" PetscInt_FMT "\">\n", 0, mx - 1, 0, my - 1, 0, mz - 1));

  if (rank == 0) PetscCall(PetscMalloc1(size * 6, &grloc));
  rloc[0] = info.xs;
  rloc[1] = info.xm;
  rloc[2] = info.ys;
  rloc[3] = info.ym;
  rloc[4] = info.zs;
  rloc[5] = info.zm;
  PetscCallMPI(MPI_Gather(rloc, 6, MPIU_INT, &grloc[0][0], 6, MPIU_INT, 0, comm));

  /* Write XML header */
  maxnnodes = 0; /* Used for the temporary array size on rank 0 */
  maxbs     = 0; /* Used for the temporary array size on rank 0 */
  boffset   = 0; /* Offset into binary file */
  for (r = 0; r < size; r++) {
    PetscInt xs = -1, xm = -1, ys = -1, ym = -1, zs = -1, zm = -1, nnodes = 0;
    if (rank == 0) {
      xs     = grloc[r][0];
      xm     = grloc[r][1];
      ys     = grloc[r][2];
      ym     = grloc[r][3];
      zs     = grloc[r][4];
      zm     = grloc[r][5];
      nnodes = xm * ym * zm;
    }
    maxnnodes = PetscMax(maxnnodes, nnodes);
    PetscCall(PetscFPrintf(comm, fp, "    <Piece Extent=\"%" PetscInt_FMT " %" PetscInt_FMT " %" PetscInt_FMT " %" PetscInt_FMT " %" PetscInt_FMT " %" PetscInt_FMT "\">\n", xs, xs + xm - 1, ys, ys + ym - 1, zs, zs + zm - 1));
    PetscCall(PetscFPrintf(comm, fp, "      <Points>\n"));
    PetscCall(PetscFPrintf(comm, fp, "        <DataArray type=\"%s\" Name=\"Position\" NumberOfComponents=\"3\" format=\"appended\" offset=\"%" PetscInt64_FMT "\" />\n", precision, boffset));
    boffset += 3 * nnodes * sizeof(PetscScalar) + sizeof(PetscInt64);
    PetscCall(PetscFPrintf(comm, fp, "      </Points>\n"));

    PetscCall(PetscFPrintf(comm, fp, "      <PointData Scalars=\"ScalarPointData\">\n"));
    for (link = vtk->link; link; link = link->next) {
      Vec         X = (Vec)link->vec;
      PetscInt    bs, f;
      DM          daCurr;
      PetscBool   fieldsnamed;
      const char *vecname = "Unnamed";

      PetscCall(VecGetDM(X, &daCurr));
      PetscCall(DMDAGetInfo(daCurr, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &bs, NULL, NULL, NULL, NULL, NULL));
      maxbs = PetscMax(maxbs, bs);

      if (((PetscObject)X)->name || link != vtk->link) PetscCall(PetscObjectGetName((PetscObject)X, &vecname));

      /* If any fields are named, add scalar fields. Otherwise, add a vector field */
      PetscCall(DMDAGetFieldsNamed(daCurr, &fieldsnamed));
      if (fieldsnamed) {
        for (f = 0; f < bs; f++) {
          char        buf[256];
          const char *fieldname;
          PetscCall(DMDAGetFieldName(daCurr, f, &fieldname));
          if (!fieldname) {
            PetscCall(PetscSNPrintf(buf, sizeof(buf), "%" PetscInt_FMT, f));
            fieldname = buf;
          }
          PetscCall(PetscFPrintf(comm, fp, "        <DataArray type=\"%s\" Name=\"%s.%s\" NumberOfComponents=\"1\" format=\"appended\" offset=\"%" PetscInt64_FMT "\" />\n", precision, vecname, fieldname, boffset));
          boffset += nnodes * sizeof(PetscScalar) + sizeof(PetscInt64);
        }
      } else {
        PetscCall(PetscFPrintf(comm, fp, "        <DataArray type=\"%s\" Name=\"%s\" NumberOfComponents=\"%" PetscInt_FMT "\" format=\"appended\" offset=\"%" PetscInt64_FMT "\" />\n", precision, vecname, bs, boffset));
        boffset += bs * nnodes * sizeof(PetscScalar) + sizeof(PetscInt64);
      }
    }
    PetscCall(PetscFPrintf(comm, fp, "      </PointData>\n"));
    PetscCall(PetscFPrintf(comm, fp, "    </Piece>\n"));
  }
  PetscCall(PetscFPrintf(comm, fp, "  </StructuredGrid>\n"));
  PetscCall(PetscFPrintf(comm, fp, "  <AppendedData encoding=\"raw\">\n"));
  PetscCall(PetscFPrintf(comm, fp, "_"));

  /* Now write the arrays. */
  tag = ((PetscObject)viewer)->tag;
  PetscCall(PetscMalloc2(maxnnodes * PetscMax(3, maxbs), &array, maxnnodes * PetscMax(3, maxbs), &array2));
  for (r = 0; r < size; r++) {
    MPI_Status status;
    PetscInt   xs = -1, xm = -1, ys = -1, ym = -1, zs = -1, zm = -1, nnodes = 0;
    if (rank == 0) {
      xs     = grloc[r][0];
      xm     = grloc[r][1];
      ys     = grloc[r][2];
      ym     = grloc[r][3];
      zs     = grloc[r][4];
      zm     = grloc[r][5];
      nnodes = xm * ym * zm;
    } else if (r == rank) {
      nnodes = info.xm * info.ym * info.zm;
    }

    /* Write the coordinates */
    if (Coords) {
      const PetscScalar *coords;
      PetscCall(VecGetArrayRead(Coords, &coords));
      if (rank == 0) {
        if (r) {
          PetscMPIInt nn;
          PetscCallMPI(MPI_Recv(array, nnodes * cdim, MPIU_SCALAR, r, tag, comm, &status));
          PetscCallMPI(MPI_Get_count(&status, MPIU_SCALAR, &nn));
          PetscCheck(nn == nnodes * cdim, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Array size mismatch");
        } else PetscCall(PetscArraycpy(array, coords, nnodes * cdim));
        /* Transpose coordinates to VTK (C-style) ordering */
        for (k = 0; k < zm; k++) {
          for (j = 0; j < ym; j++) {
            for (i = 0; i < xm; i++) {
              PetscInt Iloc        = i + xm * (j + ym * k);
              array2[Iloc * 3 + 0] = array[Iloc * cdim + 0];
              array2[Iloc * 3 + 1] = cdim > 1 ? array[Iloc * cdim + 1] : 0.0;
              array2[Iloc * 3 + 2] = cdim > 2 ? array[Iloc * cdim + 2] : 0.0;
            }
          }
        }
      } else if (r == rank) {
        PetscCallMPI(MPI_Send((void *)coords, nnodes * cdim, MPIU_SCALAR, 0, tag, comm));
      }
      PetscCall(VecRestoreArrayRead(Coords, &coords));
    } else { /* Fabricate some coordinates using grid index */
      for (k = 0; k < zm; k++) {
        for (j = 0; j < ym; j++) {
          for (i = 0; i < xm; i++) {
            PetscInt Iloc        = i + xm * (j + ym * k);
            array2[Iloc * 3 + 0] = xs + i;
            array2[Iloc * 3 + 1] = ys + j;
            array2[Iloc * 3 + 2] = zs + k;
          }
        }
      }
    }
    PetscCall(PetscViewerVTKFWrite(viewer, fp, array2, nnodes * 3, MPIU_SCALAR));

    /* Write each of the objects queued up for this file */
    for (link = vtk->link; link; link = link->next) {
      Vec                X = (Vec)link->vec;
      const PetscScalar *x;
      PetscInt           bs, f;
      DM                 daCurr;
      PetscBool          fieldsnamed;
      PetscCall(VecGetDM(X, &daCurr));
      PetscCall(DMDAGetInfo(daCurr, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &bs, NULL, NULL, NULL, NULL, NULL));
      PetscCall(VecGetArrayRead(X, &x));
      if (rank == 0) {
        if (r) {
          PetscMPIInt nn;
          PetscCallMPI(MPI_Recv(array, nnodes * bs, MPIU_SCALAR, r, tag, comm, &status));
          PetscCallMPI(MPI_Get_count(&status, MPIU_SCALAR, &nn));
          PetscCheck(nn == nnodes * bs, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Array size mismatch receiving from rank %" PetscInt_FMT, r);
        } else PetscCall(PetscArraycpy(array, x, nnodes * bs));

        /* If any fields are named, add scalar fields. Otherwise, add a vector field */
        PetscCall(DMDAGetFieldsNamed(daCurr, &fieldsnamed));
        if (fieldsnamed) {
          for (f = 0; f < bs; f++) {
            /* Extract and transpose the f'th field */
            for (k = 0; k < zm; k++) {
              for (j = 0; j < ym; j++) {
                for (i = 0; i < xm; i++) {
                  PetscInt Iloc = i + xm * (j + ym * k);
                  array2[Iloc]  = array[Iloc * bs + f];
                }
              }
            }
            PetscCall(PetscViewerVTKFWrite(viewer, fp, array2, nnodes, MPIU_SCALAR));
          }
        } else PetscCall(PetscViewerVTKFWrite(viewer, fp, array, bs * nnodes, MPIU_SCALAR));
      } else if (r == rank) PetscCallMPI(MPI_Send((void *)x, nnodes * bs, MPIU_SCALAR, 0, tag, comm));
      PetscCall(VecRestoreArrayRead(X, &x));
    }
  }
  PetscCall(PetscFree2(array, array2));
  PetscCall(PetscFree(grloc));

  PetscCall(PetscFPrintf(comm, fp, "\n </AppendedData>\n"));
  PetscCall(PetscFPrintf(comm, fp, "</VTKFile>\n"));
  PetscCall(PetscFClose(comm, fp));
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode DMDAVTKWriteAll_VTR(DM da, PetscViewer viewer)
{
  const char *byte_order = PetscBinaryBigEndian() ? "BigEndian" : "LittleEndian";
#if defined(PETSC_USE_REAL_SINGLE)
  const char precision[] = "Float32";
#elif defined(PETSC_USE_REAL_DOUBLE)
  const char precision[] = "Float64";
#else
  const char precision[] = "UnknownPrecision";
#endif
  MPI_Comm                 comm;
  PetscViewer_VTK         *vtk = (PetscViewer_VTK *)viewer->data;
  PetscViewerVTKObjectLink link;
  FILE                    *fp;
  PetscMPIInt              rank, size, tag;
  DMDALocalInfo            info;
  PetscInt                 dim, mx, my, mz, maxnnodes, maxbs, i, j, k, r;
  PetscInt64               boffset;
  PetscInt                 rloc[6], (*grloc)[6] = NULL;
  PetscScalar             *array, *array2;

  PetscFunctionBegin;
  PetscCall(PetscObjectGetComm((PetscObject)da, &comm));
  PetscCheck(!PetscDefined(USE_COMPLEX), PETSC_COMM_SELF, PETSC_ERR_SUP, "Complex values not supported");
  PetscCallMPI(MPI_Comm_size(comm, &size));
  PetscCallMPI(MPI_Comm_rank(comm, &rank));
  PetscCall(DMDAGetInfo(da, &dim, &mx, &my, &mz, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
  PetscCall(DMDAGetLocalInfo(da, &info));
  PetscCall(PetscFOpen(comm, vtk->filename, "wb", &fp));
  PetscCall(PetscFPrintf(comm, fp, "<?xml version=\"1.0\"?>\n"));
  PetscCall(PetscFPrintf(comm, fp, "<VTKFile type=\"RectilinearGrid\" version=\"0.1\" byte_order=\"%s\" header_type=\"UInt64\">\n", byte_order));
  PetscCall(PetscFPrintf(comm, fp, "  <RectilinearGrid WholeExtent=\"%d %" PetscInt_FMT " %d %" PetscInt_FMT " %d %" PetscInt_FMT "\">\n", 0, mx - 1, 0, my - 1, 0, mz - 1));

  if (rank == 0) PetscCall(PetscMalloc1(size * 6, &grloc));
  rloc[0] = info.xs;
  rloc[1] = info.xm;
  rloc[2] = info.ys;
  rloc[3] = info.ym;
  rloc[4] = info.zs;
  rloc[5] = info.zm;
  PetscCallMPI(MPI_Gather(rloc, 6, MPIU_INT, &grloc[0][0], 6, MPIU_INT, 0, comm));

  /* Write XML header */
  maxnnodes = 0; /* Used for the temporary array size on rank 0 */
  maxbs     = 0; /* Used for the temporary array size on rank 0 */
  boffset   = 0; /* Offset into binary file */
  for (r = 0; r < size; r++) {
    PetscInt xs = -1, xm = -1, ys = -1, ym = -1, zs = -1, zm = -1, nnodes = 0;
    if (rank == 0) {
      xs     = grloc[r][0];
      xm     = grloc[r][1];
      ys     = grloc[r][2];
      ym     = grloc[r][3];
      zs     = grloc[r][4];
      zm     = grloc[r][5];
      nnodes = xm * ym * zm;
    }
    maxnnodes = PetscMax(maxnnodes, nnodes);
    PetscCall(PetscFPrintf(comm, fp, "    <Piece Extent=\"%" PetscInt_FMT " %" PetscInt_FMT " %" PetscInt_FMT " %" PetscInt_FMT " %" PetscInt_FMT " %" PetscInt_FMT "\">\n", xs, xs + xm - 1, ys, ys + ym - 1, zs, zs + zm - 1));
    PetscCall(PetscFPrintf(comm, fp, "      <Coordinates>\n"));
    PetscCall(PetscFPrintf(comm, fp, "        <DataArray type=\"%s\" Name=\"Xcoord\"  format=\"appended\"  offset=\"%" PetscInt64_FMT "\" />\n", precision, boffset));
    boffset += xm * sizeof(PetscScalar) + sizeof(PetscInt64);
    PetscCall(PetscFPrintf(comm, fp, "        <DataArray type=\"%s\" Name=\"Ycoord\"  format=\"appended\"  offset=\"%" PetscInt64_FMT "\" />\n", precision, boffset));
    boffset += ym * sizeof(PetscScalar) + sizeof(PetscInt64);
    PetscCall(PetscFPrintf(comm, fp, "        <DataArray type=\"%s\" Name=\"Zcoord\"  format=\"appended\"  offset=\"%" PetscInt64_FMT "\" />\n", precision, boffset));
    boffset += zm * sizeof(PetscScalar) + sizeof(PetscInt64);
    PetscCall(PetscFPrintf(comm, fp, "      </Coordinates>\n"));
    PetscCall(PetscFPrintf(comm, fp, "      <PointData Scalars=\"ScalarPointData\">\n"));
    for (link = vtk->link; link; link = link->next) {
      Vec         X = (Vec)link->vec;
      PetscInt    bs, f;
      DM          daCurr;
      PetscBool   fieldsnamed;
      const char *vecname = "Unnamed";

      PetscCall(VecGetDM(X, &daCurr));
      PetscCall(DMDAGetInfo(daCurr, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &bs, NULL, NULL, NULL, NULL, NULL));
      maxbs = PetscMax(maxbs, bs);
      if (((PetscObject)X)->name || link != vtk->link) PetscCall(PetscObjectGetName((PetscObject)X, &vecname));

      /* If any fields are named, add scalar fields. Otherwise, add a vector field */
      PetscCall(DMDAGetFieldsNamed(daCurr, &fieldsnamed));
      if (fieldsnamed) {
        for (f = 0; f < bs; f++) {
          char        buf[256];
          const char *fieldname;
          PetscCall(DMDAGetFieldName(daCurr, f, &fieldname));
          if (!fieldname) {
            PetscCall(PetscSNPrintf(buf, sizeof(buf), "%" PetscInt_FMT, f));
            fieldname = buf;
          }
          PetscCall(PetscFPrintf(comm, fp, "        <DataArray type=\"%s\" Name=\"%s.%s\" NumberOfComponents=\"1\" format=\"appended\" offset=\"%" PetscInt64_FMT "\" />\n", precision, vecname, fieldname, boffset));
          boffset += nnodes * sizeof(PetscScalar) + sizeof(PetscInt64);
        }
      } else {
        PetscCall(PetscFPrintf(comm, fp, "        <DataArray type=\"%s\" Name=\"%s\" NumberOfComponents=\"%" PetscInt_FMT "\" format=\"appended\" offset=\"%" PetscInt64_FMT "\" />\n", precision, vecname, bs, boffset));
        boffset += bs * nnodes * sizeof(PetscScalar) + sizeof(PetscInt64);
      }
    }
    PetscCall(PetscFPrintf(comm, fp, "      </PointData>\n"));
    PetscCall(PetscFPrintf(comm, fp, "    </Piece>\n"));
  }
  PetscCall(PetscFPrintf(comm, fp, "  </RectilinearGrid>\n"));
  PetscCall(PetscFPrintf(comm, fp, "  <AppendedData encoding=\"raw\">\n"));
  PetscCall(PetscFPrintf(comm, fp, "_"));

  /* Now write the arrays. */
  tag = ((PetscObject)viewer)->tag;
  PetscCall(PetscMalloc2(PetscMax(maxnnodes * maxbs, info.xm + info.ym + info.zm), &array, PetscMax(maxnnodes * maxbs, info.xm + info.ym + info.zm), &array2));
  for (r = 0; r < size; r++) {
    MPI_Status status;
    PetscInt   xs = -1, xm = -1, ys = -1, ym = -1, zs = -1, zm = -1, nnodes = 0;
    if (rank == 0) {
      xs     = grloc[r][0];
      xm     = grloc[r][1];
      ys     = grloc[r][2];
      ym     = grloc[r][3];
      zs     = grloc[r][4];
      zm     = grloc[r][5];
      nnodes = xm * ym * zm;
    } else if (r == rank) {
      nnodes = info.xm * info.ym * info.zm;
    }
    { /* Write the coordinates */
      Vec Coords;

      PetscCall(DMGetCoordinates(da, &Coords));
      if (Coords) {
        const PetscScalar *coords;
        PetscCall(VecGetArrayRead(Coords, &coords));
        if (rank == 0) {
          if (r) {
            PetscMPIInt nn;
            PetscCallMPI(MPI_Recv(array, xm + ym + zm, MPIU_SCALAR, r, tag, comm, &status));
            PetscCallMPI(MPI_Get_count(&status, MPIU_SCALAR, &nn));
            PetscCheck(nn == xm + ym + zm, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Array size mismatch");
          } else {
            /* x coordinates */
            for (j = 0, k = 0, i = 0; i < xm; i++) {
              PetscInt Iloc = i + xm * (j + ym * k);
              array[i]      = coords[Iloc * dim + 0];
            }
            /* y coordinates */
            for (i = 0, k = 0, j = 0; j < ym; j++) {
              PetscInt Iloc = i + xm * (j + ym * k);
              array[j + xm] = dim > 1 ? coords[Iloc * dim + 1] : 0;
            }
            /* z coordinates */
            for (i = 0, j = 0, k = 0; k < zm; k++) {
              PetscInt Iloc      = i + xm * (j + ym * k);
              array[k + xm + ym] = dim > 2 ? coords[Iloc * dim + 2] : 0;
            }
          }
        } else if (r == rank) {
          xm = info.xm;
          ym = info.ym;
          zm = info.zm;
          for (j = 0, k = 0, i = 0; i < xm; i++) {
            PetscInt Iloc = i + xm * (j + ym * k);
            array2[i]     = coords[Iloc * dim + 0];
          }
          for (i = 0, k = 0, j = 0; j < ym; j++) {
            PetscInt Iloc  = i + xm * (j + ym * k);
            array2[j + xm] = dim > 1 ? coords[Iloc * dim + 1] : 0;
          }
          for (i = 0, j = 0, k = 0; k < zm; k++) {
            PetscInt Iloc       = i + xm * (j + ym * k);
            array2[k + xm + ym] = dim > 2 ? coords[Iloc * dim + 2] : 0;
          }
          PetscCallMPI(MPI_Send((void *)array2, xm + ym + zm, MPIU_SCALAR, 0, tag, comm));
        }
        PetscCall(VecRestoreArrayRead(Coords, &coords));
      } else { /* Fabricate some coordinates using grid index */
        for (i = 0; i < xm; i++) array[i] = xs + i;
        for (j = 0; j < ym; j++) array[j + xm] = ys + j;
        for (k = 0; k < zm; k++) array[k + xm + ym] = zs + k;
      }
      if (rank == 0) {
        PetscCall(PetscViewerVTKFWrite(viewer, fp, &(array[0]), xm, MPIU_SCALAR));
        PetscCall(PetscViewerVTKFWrite(viewer, fp, &(array[xm]), ym, MPIU_SCALAR));
        PetscCall(PetscViewerVTKFWrite(viewer, fp, &(array[xm + ym]), zm, MPIU_SCALAR));
      }
    }

    /* Write each of the objects queued up for this file */
    for (link = vtk->link; link; link = link->next) {
      Vec                X = (Vec)link->vec;
      const PetscScalar *x;
      PetscInt           bs, f;
      DM                 daCurr;
      PetscBool          fieldsnamed;
      PetscCall(VecGetDM(X, &daCurr));
      PetscCall(DMDAGetInfo(daCurr, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &bs, NULL, NULL, NULL, NULL, NULL));

      PetscCall(VecGetArrayRead(X, &x));
      if (rank == 0) {
        if (r) {
          PetscMPIInt nn;
          PetscCallMPI(MPI_Recv(array, nnodes * bs, MPIU_SCALAR, r, tag, comm, &status));
          PetscCallMPI(MPI_Get_count(&status, MPIU_SCALAR, &nn));
          PetscCheck(nn == nnodes * bs, PETSC_COMM_SELF, PETSC_ERR_PLIB, "Array size mismatch receiving from rank %" PetscInt_FMT, r);
        } else PetscCall(PetscArraycpy(array, x, nnodes * bs));
        /* If any fields are named, add scalar fields. Otherwise, add a vector field */
        PetscCall(DMDAGetFieldsNamed(daCurr, &fieldsnamed));
        if (fieldsnamed) {
          for (f = 0; f < bs; f++) {
            /* Extract and transpose the f'th field */
            for (k = 0; k < zm; k++) {
              for (j = 0; j < ym; j++) {
                for (i = 0; i < xm; i++) {
                  PetscInt Iloc = i + xm * (j + ym * k);
                  array2[Iloc]  = array[Iloc * bs + f];
                }
              }
            }
            PetscCall(PetscViewerVTKFWrite(viewer, fp, array2, nnodes, MPIU_SCALAR));
          }
        }
        PetscCall(PetscViewerVTKFWrite(viewer, fp, array, nnodes * bs, MPIU_SCALAR));

      } else if (r == rank) {
        PetscCallMPI(MPI_Send((void *)x, nnodes * bs, MPIU_SCALAR, 0, tag, comm));
      }
      PetscCall(VecRestoreArrayRead(X, &x));
    }
  }
  PetscCall(PetscFree2(array, array2));
  PetscCall(PetscFree(grloc));

  PetscCall(PetscFPrintf(comm, fp, "\n </AppendedData>\n"));
  PetscCall(PetscFPrintf(comm, fp, "</VTKFile>\n"));
  PetscCall(PetscFClose(comm, fp));
  PetscFunctionReturn(PETSC_SUCCESS);
}

/*@C
   DMDAVTKWriteAll - Write a file containing all the fields that have been provided to the viewer

   Collective

   Input Parameters:
+  odm - `DMDA` specifying the grid layout, passed as a `PetscObject`
-  viewer - viewer of type `PETSCVIEWERVTK`

   Level: developer

   Notes:
   This function is a callback used by the VTK viewer to actually write the file.
   The reason for this odd model is that the VTK file format does not provide any way to write one field at a time.
   Instead, metadata for the entire file needs to be available up-front before you can start writing the file.

   If any fields have been named (see e.g. DMDASetFieldName()), then individual scalar
   fields are written. Otherwise, a single multi-dof (vector) field is written.

.seealso: `DMDA`, `DM`, `PETSCVIEWERVTK`
@*/
PetscErrorCode DMDAVTKWriteAll(PetscObject odm, PetscViewer viewer)
{
  DM        dm = (DM)odm;
  PetscBool isvtk;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 2);
  PetscCall(PetscObjectTypeCompare((PetscObject)viewer, PETSCVIEWERVTK, &isvtk));
  PetscCheck(isvtk, PetscObjectComm((PetscObject)viewer), PETSC_ERR_ARG_INCOMP, "Cannot use viewer type %s", ((PetscObject)viewer)->type_name);
  switch (viewer->format) {
  case PETSC_VIEWER_VTK_VTS:
    PetscCall(DMDAVTKWriteAll_VTS(dm, viewer));
    break;
  case PETSC_VIEWER_VTK_VTR:
    PetscCall(DMDAVTKWriteAll_VTR(dm, viewer));
    break;
  default:
    SETERRQ(PetscObjectComm((PetscObject)dm), PETSC_ERR_SUP, "No support for format '%s'", PetscViewerFormats[viewer->format]);
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}
