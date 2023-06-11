#cython: cdivision=True
#cython: binding=False
#cython: auto_pickle=False
#cython: autotestdict=False
#cython: warn.multiple_declarators=False

# --------------------------------------------------------------------

cdef extern from "Python.h":
    ctypedef struct PyObject
    void Py_INCREF(PyObject*)
    void Py_DECREF(PyObject*)
    void Py_CLEAR(PyObject*)
    object PyModule_New(const char[])
    bint PyModule_Check(object)
    object PyImport_Import(object)

# --------------------------------------------------------------------

cdef extern from * nogil:
    ctypedef struct _p_PetscOptionItems
    ctypedef _p_PetscOptionItems* PetscOptionItems
    PetscErrorCode PetscOptionsString(char[],char[],char[],char[],char[],size_t,PetscBool*)

cdef extern from * nogil: # custom.h
    PetscErrorCode PetscObjectComposedDataRegisterPy(PetscInt*)
    PetscErrorCode PetscObjectComposedDataGetIntPy(PetscObject,PetscInt,PetscInt*,PetscBool*)
    PetscErrorCode PetscObjectComposedDataSetIntPy(PetscObject,PetscInt,PetscInt)

# --------------------------------------------------------------------

cdef char *FUNCT = NULL
cdef char *fstack[1024]
cdef int   istack = 0

cdef inline void FunctionBegin(char name[]) nogil:
    global istack, fstack, FUNCT
    FUNCT = name
    fstack[istack] = FUNCT
    istack += 1
    if istack >= 1024:
        istack = 0
    return

cdef inline PetscErrorCode FunctionEnd() nogil:
    global istack, fstack, FUNCT
    FUNCT = NULL
    istack -= 1
    if istack < 0:
        istack = 1024
    FUNCT = fstack[istack]
    return PETSC_SUCCESS

cdef PetscErrorCode PetscSETERR(PetscErrorCode ierr,char msg[]) nogil:
    global istack, fstack
    istack = 0
    fstack[istack] = NULL;
    return PetscERROR(PETSC_COMM_SELF,FUNCT,ierr,
                      PETSC_ERROR_INITIAL, msg, NULL)

cdef PetscErrorCode UNSUPPORTED(char msg[]) nogil:
    return PetscERROR(PETSC_COMM_SELF,FUNCT,PETSC_ERR_USER,
                      PETSC_ERROR_INITIAL,b"method %s()",msg)

# --------------------------------------------------------------------

cdef inline PetscInt getRef(void *pobj) nogil:
    cdef PetscObject obj = <PetscObject>pobj
    if obj == NULL: return 0
    else: return obj.refct

cdef inline void addRef(void *pobj) nogil:
    cdef PetscObject obj = <PetscObject>pobj
    if obj != NULL: obj.refct += 1

cdef inline void delRef(void *pobj) nogil:
    cdef PetscObject obj = <PetscObject>pobj
    if obj != NULL: obj.refct -= 1

cdef inline PetscObject newRef(void *pobj) nogil:
    cdef PetscObject obj = <PetscObject>pobj
    cdef int ierr = 0
    if obj != NULL:
        ierr = PetscObjectReference(obj)
        if ierr: return NULL # XXX warning!
    return obj

cdef inline const char* getPrefix(void *pobj) nogil:
    cdef PetscObject obj = <PetscObject>pobj
    if obj == NULL: return NULL
    return obj.prefix

cdef inline int getCommSize(void *pobj) nogil:
    cdef PetscObject obj = <PetscObject>pobj
    if obj == NULL: return 0
    cdef int size = 0
    MPI_Comm_size(obj.comm, &size)
    return size

cdef inline Viewer Viewer_(PetscViewer p):
    cdef Viewer ob = Viewer.__new__(Viewer)
    ob.obj[0] = newRef(p)
    return ob

cdef inline IS IS_(PetscIS p):
    cdef IS ob = IS.__new__(IS)
    ob.obj[0] = newRef(p)
    return ob

cdef inline Vec Vec_(PetscVec p):
    cdef Vec ob = Vec.__new__(Vec)
    ob.obj[0] = newRef(p)
    return ob

cdef inline Mat Mat_(PetscMat p):
    cdef Mat ob = Mat.__new__(Mat)
    ob.obj[0] = newRef(p)
    return ob

cdef inline PC PC_(PetscPC p):
    cdef PC ob = PC.__new__(PC)
    ob.obj[0] = newRef(p)
    return ob

cdef inline KSP KSP_(PetscKSP p):
    cdef KSP ob = KSP.__new__(KSP)
    ob.obj[0] = newRef(p)
    return ob

cdef inline SNES SNES_(PetscSNES p):
    cdef SNES ob = SNES.__new__(SNES)
    ob.obj[0] = newRef(p)
    return ob

cdef inline TS TS_(PetscTS p):
    cdef TS ob = TS.__new__(TS)
    ob.obj[0] = newRef(p)
    return ob

cdef inline TAO TAO_(PetscTAO p):
    cdef TAO ob = TAO.__new__(TAO)
    ob.obj[0] = newRef(p)
    return ob

# --------------------------------------------------------------------

cdef object parse_url(object url):
    path, name = url.rsplit(":", 1)
    return (path, name)

cdef dict module_cache = {}

cdef object load_module(object path):
    if path in module_cache:
        return module_cache[path]
    module = PyModule_New("__petsc__")
    module.__file__ = path
    module.__package__ = None
    module_cache[path] = module
    try:
        with open(path, 'r') as source:
            code = compile(source.read(), path, 'exec')
        exec(code, module.__dict__)
    except:
        del module_cache[path]
        raise
    return module

# -----------------------------------------------------------------------------

@cython.internal
cdef class _PyObj:

    cdef object self
    cdef bytes  name

    def __getattr__(self, attr):
        return getattr(self.self, attr, None)

    cdef int setcontext(self, void *ctx, Object base) except -1:
        #
        if ctx == <void*>self.self:
            return 0
        #
        cdef object destroy = self.destroy
        if destroy is not None:
            destroy(base)
            destroy = None
        #
        if ctx == NULL:
            self.self = None
            self.name = None
            return 0
        #
        self.self = <object>ctx
        self.name = None
        cdef object create = self.create
        if create is not None:
            create(base)
            create = None
        return 0

    cdef int getcontext(self, void **ctx) except -1:
        if ctx == NULL: return 0
        if self.self is not None:
            ctx[0] = <void*> self.self
        else:
            ctx[0] = NULL
        return 0

    cdef int setname(self, char name[]) except -1:
        if name != NULL and name[0] != 0:
            self.name = name
        else:
            self.name = None
        return 0

    cdef char* getname(self) except? NULL:
        if self.self is None:
            return NULL
        if self.name is not None:
            return self.name
        cdef ctx = self.self
        cdef name = None
        if PyModule_Check(ctx):
            name = getattr(ctx, '__name__', None)
        else:
            modname = getattr(ctx, '__module__', None)
            clsname = None
            cls = getattr(ctx, '__class__', None)
            if cls:
                clsname = getattr(cls, '__name__', None)
                if not modname:
                    modname = getattr(cls, '__module__', None)
            if modname and clsname:
                name = modname + '.' + clsname
            elif clsname:
                name = clsname
            elif modname:
                name = modname
        if name is not None:
            self.name = name.encode()
        if self.name is not None:
            return self.name
        return NULL

cdef createcontext(char name_p[]):
    if name_p == NULL: return None
    cdef name = bytes2str(name_p)
    cdef mod, path, modname=None
    cdef cls, attr, clsname=None
    # path/to/filename.py:{function|class}
    if ':' in name:
        path, attr = parse_url(name)
        mod = load_module(path)
        if attr:
            cls = getattr(mod, attr)
            return cls()
        else:
            return mod
    # package.module[.{function|class}]
    if '.' in name:
        modname, clsname = name.rsplit('.', 1)
        mod = PyImport_Import(modname)
        if hasattr(mod, clsname):
            cls = getattr(mod, clsname)
            if not PyModule_Check(cls):
                return cls()
    # package[.module]
    mod = PyImport_Import(name)
    return mod

cdef int viewcontext(_PyObj ctx, PetscViewer viewer) except -1:
    cdef PetscBool isascii = PETSC_FALSE, isstring = PETSC_FALSE
    CHKERR( PetscObjectTypeCompare(<PetscObject>viewer, PETSCVIEWERASCII,  &isascii)  )
    CHKERR( PetscObjectTypeCompare(<PetscObject>viewer, PETSCVIEWERSTRING, &isstring) )
    cdef char *name = ctx.getname()
    if isascii:
        if name == NULL: name = b"unknown/no yet set"
        CHKERR( PetscViewerASCIIPrintf(viewer, b"  Python: %s\n", name) )
    if isstring:
        if name == NULL: name = b"<unknown>"
        CHKERR( PetscViewerStringSPrintf(viewer, "%s", name) )
    return 0

# --------------------------------------------------------------------

cdef extern from * nogil:
    struct _MatOps:
        PetscErrorCode (*destroy)(PetscMat) except PETSC_ERR_PYTHON
        PetscErrorCode (*setfromoptions)(PetscMat,PetscOptionItems*) except PETSC_ERR_PYTHON
        PetscErrorCode (*view)(PetscMat,PetscViewer) except PETSC_ERR_PYTHON
        PetscErrorCode (*duplicate)(PetscMat,PetscMatDuplicateOption,PetscMat*) except PETSC_ERR_PYTHON
        PetscErrorCode (*copy)(PetscMat,PetscMat,PetscMatStructure) except PETSC_ERR_PYTHON
        PetscErrorCode (*createsubmatrix)(PetscMat,PetscIS,PetscIS,PetscMatReuse,PetscMat*) except PETSC_ERR_PYTHON
        PetscErrorCode (*setoption)(PetscMat,PetscMatOption,PetscBool) except PETSC_ERR_PYTHON
        PetscErrorCode (*setup)(PetscMat) except PETSC_ERR_PYTHON
        PetscErrorCode (*assemblybegin)(PetscMat,PetscMatAssemblyType) except PETSC_ERR_PYTHON
        PetscErrorCode (*assemblyend)(PetscMat,PetscMatAssemblyType) except PETSC_ERR_PYTHON
        PetscErrorCode (*zeroentries)(PetscMat) except PETSC_ERR_PYTHON
        PetscErrorCode (*zerorowscolumns)(PetscMat,PetscInt,PetscInt*,PetscScalar,PetscVec,PetscVec) except PETSC_ERR_PYTHON
        PetscErrorCode (*scale)(PetscMat,PetscScalar) except PETSC_ERR_PYTHON
        PetscErrorCode (*shift)(PetscMat,PetscScalar) except PETSC_ERR_PYTHON
        PetscErrorCode (*sor)(PetscMat,PetscVec,PetscReal,PetscMatSORType,PetscReal,PetscInt,PetscInt,PetscVec) except PETSC_ERR_PYTHON
        PetscErrorCode (*getvecs)(PetscMat,PetscVec*,PetscVec*) except PETSC_ERR_PYTHON
        PetscErrorCode (*mult)(PetscMat,PetscVec,PetscVec) except PETSC_ERR_PYTHON
        PetscErrorCode (*multtranspose)(PetscMat,PetscVec,PetscVec) except PETSC_ERR_PYTHON
        PetscErrorCode (*multhermitian"multhermitiantranspose")(PetscMat,PetscVec,PetscVec) except PETSC_ERR_PYTHON
        PetscErrorCode (*multadd)(PetscMat,PetscVec,PetscVec,PetscVec) except PETSC_ERR_PYTHON
        PetscErrorCode (*multtransposeadd)(PetscMat,PetscVec,PetscVec,PetscVec) except PETSC_ERR_PYTHON
        PetscErrorCode (*multhermitianadd"multhermitiantransposeadd")(PetscMat,PetscVec,PetscVec,PetscVec) except PETSC_ERR_PYTHON
        PetscErrorCode (*multdiagonalblock)(PetscMat,PetscVec,PetscVec) except PETSC_ERR_PYTHON
        PetscErrorCode (*solve)(PetscMat,PetscVec,PetscVec) except PETSC_ERR_PYTHON
        PetscErrorCode (*solvetranspose)(PetscMat,PetscVec,PetscVec) except PETSC_ERR_PYTHON
        PetscErrorCode (*solveadd)(PetscMat,PetscVec,PetscVec,PetscVec) except PETSC_ERR_PYTHON
        PetscErrorCode (*solvetransposeadd)(PetscMat,PetscVec,PetscVec,PetscVec) except PETSC_ERR_PYTHON
        PetscErrorCode (*getdiagonal)(PetscMat,PetscVec) except PETSC_ERR_PYTHON
        PetscErrorCode (*setdiagonal"diagonalset")(PetscMat,PetscVec,PetscInsertMode) except PETSC_ERR_PYTHON
        PetscErrorCode (*diagonalscale)(PetscMat,PetscVec,PetscVec) except PETSC_ERR_PYTHON
        PetscErrorCode (*missingdiagonal)(PetscMat,PetscBool*,PetscInt*) except PETSC_ERR_PYTHON
        PetscErrorCode (*norm)(PetscMat,PetscNormType,PetscReal*) except PETSC_ERR_PYTHON
        PetscErrorCode (*realpart)(PetscMat) except PETSC_ERR_PYTHON
        PetscErrorCode (*imagpart"imaginarypart")(PetscMat) except PETSC_ERR_PYTHON
        PetscErrorCode (*conjugate)(PetscMat) except PETSC_ERR_PYTHON
        PetscErrorCode (*getdiagonalblock)(PetscMat,PetscMat*) except PETSC_ERR_PYTHON
        PetscErrorCode (*productsetfromoptions)(PetscMat) except PETSC_ERR_PYTHON
        PetscErrorCode (*productsymbolic)(PetscMat) except PETSC_ERR_PYTHON
        PetscErrorCode (*productnumeric)(PetscMat) except PETSC_ERR_PYTHON
        PetscErrorCode (*hasoperation)(PetscMat,PetscMatOperation,PetscBool*) except PETSC_ERR_PYTHON
    ctypedef _MatOps *MatOps
    ctypedef struct Mat_Product:
        void *data
    struct _p_Mat:
        void *data
        MatOps ops
        PetscBool assembled
        PetscBool preallocated
        PetscLayout rmap, cmap
        Mat_Product *product


@cython.internal
cdef class _PyMat(_PyObj): pass
cdef inline _PyMat PyMat(PetscMat mat):
    if mat != NULL and mat.data != NULL:
        return <_PyMat>mat.data
    else:
        return _PyMat.__new__(_PyMat)

cdef public PetscErrorCode MatPythonGetContext(PetscMat mat, void **ctx) \
    except PETSC_ERR_PYTHON:
    FunctionBegin(b"MatPythonGetContext")
    PyMat(mat).getcontext(ctx)
    return FunctionEnd()

cdef public PetscErrorCode MatPythonSetContext(PetscMat mat, void *ctx) \
    except PETSC_ERR_PYTHON:
    FunctionBegin(b"MatPythonSetContext")
    PyMat(mat).setcontext(ctx, Mat_(mat))
    return FunctionEnd()

cdef PetscErrorCode MatPythonSetType_PYTHON(PetscMat mat, char name[]) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatPythonSetType_PYTHON")
    if name == NULL: return FunctionEnd() # XXX
    cdef object ctx = createcontext(name)
    MatPythonSetContext(mat, <void*>ctx)
    PyMat(mat).setname(name)
    return FunctionEnd()

cdef PetscErrorCode MatPythonGetType_PYTHON(PetscMat mat, const char *name[]) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatPythonGetType_PYTHON")
    name[0] = PyMat(mat).getname()
    return FunctionEnd()

cdef dict dMatOps = {   3 : 'mult',
                        4 : 'multAdd',
                        5 : 'multTranspose',
                        6 : 'multTransposeAdd',
                        7 : 'solve',
                        8 : 'solveAdd',
                        9 : 'solveTranspose',
                       10 : 'solveTransposeAdd',
                       13 : 'SOR',
                       17 : 'getDiagonal',
                       18 : 'diagonalScale',
                       19 : 'norm',
                       23 : 'zeroEntries',
                       32 : 'getDiagonalBlock',
                       34 : 'duplicate',
                       43 : 'copy',
                       45 : 'scale',
                       46 : 'shift',
                       47 : 'setDiagonal',
                       48 : 'zeroRowsColumns',
                       59 : 'createSubMatrix',
                       88 : 'getVecs',
                      102 : 'conjugate',
                      105 : 'realPart',
                      106 : 'imagPart',
                      113 : 'missingDiagonal',
                      119 : 'multDiagonalBlock',
                      121 : 'multHermitian',
                      122 : 'multHermitianAdd',
                    }

cdef PetscErrorCode MatCreate_Python(
    PetscMat mat,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatCreate_Python")
    #
    cdef MatOps ops       = mat.ops
    ops.destroy           = MatDestroy_Python
    ops.setfromoptions    = MatSetFromOptions_Python
    ops.view              = MatView_Python
    ops.duplicate         = MatDuplicate_Python
    ops.copy              = MatCopy_Python
    ops.createsubmatrix   = MatCreateSubMatrix_Python
    ops.setoption         = MatSetOption_Python
    ops.setup             = MatSetUp_Python
    ops.assemblybegin     = MatAssemblyBegin_Python
    ops.assemblyend       = MatAssemblyEnd_Python
    ops.zeroentries       = MatZeroEntries_Python
    ops.zerorowscolumns   = MatZeroRowsColumns_Python
    ops.scale             = MatScale_Python
    ops.shift             = MatShift_Python
    ops.getvecs           = MatCreateVecs_Python
    ops.mult              = MatMult_Python
    ops.sor               = MatSOR_Python
    ops.multtranspose     = MatMultTranspose_Python
    ops.multhermitian     = MatMultHermitian_Python
    ops.multadd           = MatMultAdd_Python
    ops.multtransposeadd  = MatMultTransposeAdd_Python
    ops.multhermitianadd  = MatMultHermitianAdd_Python
    ops.multdiagonalblock = MatMultDiagonalBlock_Python
    ops.solve             = MatSolve_Python
    ops.solvetranspose    = MatSolveTranspose_Python
    ops.solveadd          = MatSolveAdd_Python
    ops.solvetransposeadd = MatSolveTransposeAdd_Python
    ops.getdiagonal       = MatGetDiagonal_Python
    ops.setdiagonal       = MatSetDiagonal_Python
    ops.diagonalscale     = MatDiagonalScale_Python
    ops.missingdiagonal   = MatMissingDiagonal_Python
    ops.norm              = MatNorm_Python
    ops.realpart          = MatRealPart_Python
    ops.imagpart          = MatImagPart_Python
    ops.conjugate         = MatConjugate_Python
    ops.hasoperation      = MatHasOperation_Python
    ops.getdiagonalblock  = MatGetDiagonalBlock_Python

    ops.productsetfromoptions = MatProductSetFromOptions_Python

    #
    mat.assembled    = PETSC_TRUE  # XXX
    mat.preallocated = PETSC_FALSE # XXX
    #
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>mat, b"MatPythonSetType_C",
            <PetscVoidFunction>MatPythonSetType_PYTHON) )
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>mat, b"MatPythonGetType_C",
            <PetscVoidFunction>MatPythonGetType_PYTHON) )
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>mat, b"MatProductSetFromOptions_anytype_C",
            <PetscVoidFunction>MatProductSetFromOptions_Python) )
    CHKERR( PetscObjectChangeTypeName(
            <PetscObject>mat, MATPYTHON) )
    #
    cdef ctx = PyMat(NULL)
    mat.data = <void*> ctx
    Py_INCREF(<PyObject*>mat.data)
    return FunctionEnd()

cdef inline PetscErrorCode MatDestroy_Python_inner(
    PetscMat mat,
    ) \
    except PETSC_ERR_PYTHON with gil:
    try:
        addRef(mat)
        MatPythonSetContext(mat, NULL)
    finally:
        delRef(mat)
        Py_DECREF(<PyObject*>mat.data)
        mat.data = NULL
    return PETSC_SUCCESS

cdef PetscErrorCode MatDestroy_Python(
    PetscMat mat,
    ) \
    nogil except PETSC_ERR_PYTHON:

    FunctionBegin(b"MatDestroy_Python")
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>mat, b"MatPythonSetType_C",
            <PetscVoidFunction>NULL) )
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>mat, b"MatPythonGetType_C",
            <PetscVoidFunction>NULL) )
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>mat, b"MatProductSetFromOptions_anytype_C",
            <PetscVoidFunction>NULL) )
    CHKERR( PetscObjectChangeTypeName(
            <PetscObject>mat, NULL) )

    if Py_IsInitialized(): MatDestroy_Python_inner(mat)
    return FunctionEnd()

cdef PetscErrorCode MatSetFromOptions_Python(
    PetscMat mat,
    PetscOptionItems *PetscOptionsObject,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatSetFromOptions_Python")
    #
    cdef char name[2048], *defval = PyMat(mat).getname()
    cdef PetscBool found = PETSC_FALSE
    cdef PetscOptionItems *opts "PetscOptionsObject" = PetscOptionsObject
    CHKERR( PetscOptionsString(
            b"-mat_python_type", b"Python [package.]module[.{class|function}]",
            b"MatPythonSetType", defval, name, sizeof(name), &found) ); <void>opts;
    if found and name[0]:
        CHKERR( MatPythonSetType_PYTHON(mat, name) )
    #
    cdef setFromOptions = PyMat(mat).setFromOptions
    if setFromOptions is not None:
        setFromOptions(Mat_(mat))
    return FunctionEnd()

cdef PetscErrorCode MatView_Python(
    PetscMat mat,
    PetscViewer vwr,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatView_Python")
    viewcontext(PyMat(mat), vwr)
    cdef view = PyMat(mat).view
    if view is not None:
        view(Mat_(mat), Viewer_(vwr))
    return FunctionEnd()

cdef PetscErrorCode MatDuplicate_Python(
    PetscMat mat,
    PetscMatDuplicateOption op,
    PetscMat* out,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatDuplicate_Python")
    cdef duplicate = PyMat(mat).duplicate
    if duplicate is None: return UNSUPPORTED(b"duplicate")
    cdef Mat m = duplicate(Mat_(mat), <long>op)
    out[0] = m.mat; m.mat = NULL
    return FunctionEnd()

cdef PetscErrorCode MatCopy_Python(
    PetscMat mat,
    PetscMat out,
    PetscMatStructure op,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatCopy_Python")
    cdef copy = PyMat(mat).copy
    if copy is None: return UNSUPPORTED(b"copy")
    copy(Mat_(mat), Mat_(out), <long>op)
    return FunctionEnd()

cdef PetscErrorCode MatGetDiagonalBlock_Python(
    PetscMat  mat,
    PetscMat  *out
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatGetDiagonalBlock_Python")
    cdef getDiagonalBlock = PyMat(mat).getDiagonalBlock
    if getDiagonalBlock is None:
        try:
            mat.ops.getdiagonalblock = NULL
            CHKERR( MatGetDiagonalBlock(mat, out) )
        finally:
            mat.ops.getdiagonalblock = MatGetDiagonalBlock_Python
        return FunctionEnd()
    cdef Mat sub = getDiagonalBlock(Mat_(mat))
    if sub is not None: out[0] = sub.mat
    return FunctionEnd()

cdef PetscErrorCode MatCreateSubMatrix_Python(
    PetscMat mat,
    PetscIS  row,
    PetscIS  col,
    PetscMatReuse op,
    PetscMat *out,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatCreateSubMatrix_Python")
    cdef createSubMatrix = PyMat(mat).createSubMatrix
    if createSubMatrix is None:
       try:
           mat.ops.createsubmatrix = NULL
           CHKERR( MatCreateSubMatrix(mat, row, col, op, out) )
       finally:
           mat.ops.createsubmatrix = MatCreateSubMatrix_Python
       return FunctionEnd()
    cdef Mat sub = None
    if op == MAT_IGNORE_MATRIX:
        sub = None
    elif op == MAT_INITIAL_MATRIX:
        sub = createSubMatrix(Mat_(mat), IS_(row), IS_(col), None)
        if sub is not None:
            addRef(sub.mat)
    elif op == MAT_REUSE_MATRIX:
        sub = createSubMatrix(Mat_(mat), IS_(row), IS_(col), Mat_(out[0]))
    if sub is not None:
        out[0] = sub.mat
    return FunctionEnd()

cdef PetscErrorCode MatSetOption_Python(
    PetscMat mat,
    PetscMatOption op,
    PetscBool flag,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatSetOption_Python")
    cdef setOption = PyMat(mat).setOption
    if setOption is not None:
        setOption(Mat_(mat), <long>op, <bint>(<int>flag))
    return FunctionEnd()

cdef PetscErrorCode MatSetUp_Python(
    PetscMat mat,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatSetUp_Python")
    cdef PetscInt rbs = -1, cbs = -1
    CHKERR( PetscLayoutGetBlockSize(mat.rmap, &rbs) )
    CHKERR( PetscLayoutGetBlockSize(mat.cmap, &cbs) )
    if rbs == -1: rbs = 1
    if cbs == -1: cbs = rbs
    CHKERR( PetscLayoutSetBlockSize(mat.rmap, rbs) )
    CHKERR( PetscLayoutSetBlockSize(mat.cmap, cbs) )
    CHKERR( PetscLayoutSetUp(mat.rmap) )
    CHKERR( PetscLayoutSetUp(mat.cmap) )
    mat.preallocated = PETSC_TRUE
    #
    cdef char name[2048]
    cdef PetscBool found = PETSC_FALSE
    if PyMat(mat).self is None:
        CHKERR( PetscOptionsGetString(NULL,
                getPrefix(mat), b"-mat_python_type",
                name, sizeof(name), &found) )
        if found and name[0]:
            CHKERR( MatPythonSetType_PYTHON(mat, name) )
    if PyMat(mat).self is None:
        return PetscSETERR(PETSC_ERR_USER,
            "Python context not set, call one of \n"
            " * MatPythonSetType(mat, \"[package.]module.class\")\n"
            " * MatSetFromOptions(mat) and pass option "
            "-mat_python_type [package.]module.class")
    #
    cdef setUp = PyMat(mat).setUp
    if setUp is not None:
        setUp(Mat_(mat))
    return FunctionEnd()

cdef PetscErrorCode MatAssemblyBegin_Python(
    PetscMat mat,
    PetscMatAssemblyType at,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatAssemblyBegin_Python")
    cdef assembly = PyMat(mat).assemblyBegin
    if assembly is not None:
        assembly(Mat_(mat), <long>at)
    return FunctionEnd()

cdef PetscErrorCode MatAssemblyEnd_Python(
    PetscMat mat,
    PetscMatAssemblyType at,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatAssemblyEnd_Python")
    cdef assembly = PyMat(mat).assemblyEnd
    if assembly is None:
        assembly = PyMat(mat).assembly
    if assembly is not None:
        assembly(Mat_(mat), <long>at)
    return FunctionEnd()

cdef PetscErrorCode MatZeroEntries_Python(
    PetscMat mat,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatZeroEntries_Python")
    cdef zeroEntries = PyMat(mat).zeroEntries
    if zeroEntries is None: return UNSUPPORTED(b"zeroEntries")
    zeroEntries(Mat_(mat))
    return FunctionEnd()

cdef PetscErrorCode MatZeroRowsColumns_Python(
    PetscMat mat,
    PetscInt numRows,
    const PetscInt* rows,
    PetscScalar diag,
    PetscVec x,
    PetscVec b,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatZeroRowsColumns_Python")
    cdef zeroRowsColumns = PyMat(mat).zeroRowsColumns
    if zeroRowsColumns is None: return UNSUPPORTED(b"zeroRowsColumns")
    cdef ndarray pyrows = array_i(numRows, rows)
    zeroRowsColumns(Mat_(mat), pyrows, toScalar(diag), Vec_(x), Vec_(b))
    return FunctionEnd()

cdef PetscErrorCode MatScale_Python(
    PetscMat mat,
    PetscScalar s,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatScale_Python")
    cdef scale = PyMat(mat).scale
    if scale is None: return UNSUPPORTED(b"scale")
    scale(Mat_(mat), toScalar(s))
    return FunctionEnd()

cdef PetscErrorCode MatShift_Python(
    PetscMat mat,
    PetscScalar s,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatShift_Python")
    cdef shift = PyMat(mat).shift
    if shift is None: return UNSUPPORTED(b"shift")
    shift(Mat_(mat), toScalar(s))
    return FunctionEnd()

cdef PetscErrorCode MatCreateVecs_Python(
    PetscMat mat,
    PetscVec *x,
    PetscVec *y,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatCreateVecs_Python")
    cdef createVecs = PyMat(mat).createVecs
    if createVecs is None:
        try:
            mat.ops.getvecs = NULL
            CHKERR( MatCreateVecs(mat, x, y) )
        finally:
            mat.ops.getvecs = MatCreateVecs_Python
        return FunctionEnd()
    cdef Vec u, v
    u, v = createVecs(Mat_(mat))
    if x != NULL:
        x[0] = u.vec
        u.vec = NULL
    if y != NULL:
        y[0] = v.vec
        v.vec = NULL
    return FunctionEnd()

cdef PetscErrorCode MatMult_Python(
    PetscMat mat,
    PetscVec x,
    PetscVec y,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatMult_Python")
    cdef mult = PyMat(mat).mult
    if mult is None: return UNSUPPORTED(b"mult")
    mult(Mat_(mat), Vec_(x), Vec_(y))
    return FunctionEnd()

cdef PetscErrorCode MatMultTranspose_Python(
    PetscMat mat,
    PetscVec x,
    PetscVec y,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatMultTranspose_Python")
    cdef multTranspose = PyMat(mat).multTranspose
    if multTranspose is None:
        try:
            mat.ops.multtranspose = NULL
            CHKERR( MatMultTranspose(mat, x, y) )
        finally:
            mat.ops.multtranspose = MatMultTranspose_Python
        return FunctionEnd()
    multTranspose(Mat_(mat), Vec_(x), Vec_(y))
    return FunctionEnd()

cdef PetscErrorCode MatMultHermitian_Python(
    PetscMat mat,
    PetscVec x,
    PetscVec y,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatMultHermitian_Python")
    cdef multHermitian = PyMat(mat).multHermitian
    if multHermitian is None:
        try:
            mat.ops.multhermitian = NULL
            CHKERR( MatMultHermitian(mat, x, y) )
        finally:
            mat.ops.multhermitian = MatMultHermitian_Python
        return FunctionEnd()
    multHermitian(Mat_(mat), Vec_(x), Vec_(y))
    return FunctionEnd()

cdef PetscErrorCode MatMultAdd_Python(
    PetscMat mat,
    PetscVec x,
    PetscVec v,
    PetscVec y,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatMultAdd_Python")
    cdef multAdd = PyMat(mat).multAdd
    cdef PetscVec t = NULL
    if multAdd is None:
        if v == y:
            CHKERR( VecDuplicate(y, &t) )
            CHKERR( MatMult(mat, x, t) )
            CHKERR( VecAXPY(y, 1.0, t) )
            CHKERR( VecDestroy(&t) )
        else:
            CHKERR( MatMult(mat, x, y) )
            CHKERR( VecAXPY(y, 1.0, v) )
        return FunctionEnd()
    if multAdd is None: return UNSUPPORTED(b"multAdd")
    multAdd(Mat_(mat), Vec_(x), Vec_(v), Vec_(y))
    return FunctionEnd()

cdef PetscErrorCode MatMultTransposeAdd_Python(
    PetscMat mat,
    PetscVec x,
    PetscVec v,
    PetscVec y,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatMultTransposeAdd_Python")
    cdef multTransposeAdd = PyMat(mat).multTransposeAdd
    cdef PetscVec t = NULL
    if multTransposeAdd is None:
        if v == y:
            CHKERR( VecDuplicate(y, &t) )
            CHKERR( MatMultTranspose(mat, x, t) )
            CHKERR( VecAXPY(y, 1.0, t) )
            CHKERR( VecDestroy(&t) )
        else:
            CHKERR( MatMultTranspose(mat, x, y) )
            CHKERR( VecAXPY(y, 1.0, v) )
        return FunctionEnd()
    if multTransposeAdd is None: return UNSUPPORTED(b"multTransposeAdd")
    multTransposeAdd(Mat_(mat), Vec_(x), Vec_(v), Vec_(y))
    return FunctionEnd()

cdef PetscErrorCode MatMultHermitianAdd_Python(
    PetscMat mat,
    PetscVec x,
    PetscVec v,
    PetscVec y,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatMultHermitianAdd_Python")
    cdef multHermitianAdd = PyMat(mat).multHermitianAdd
    if multHermitianAdd is None:
        try:
            mat.ops.multhermitianadd = NULL
            CHKERR( MatMultHermitianAdd(mat, x, v, y) )
        finally:
            mat.ops.multhermitianadd = MatMultHermitianAdd_Python
        return FunctionEnd()
    multHermitianAdd(Mat_(mat), Vec_(x), Vec_(v), Vec_(y))
    return FunctionEnd()

cdef PetscErrorCode MatMultDiagonalBlock_Python(
    PetscMat mat,
    PetscVec x,
    PetscVec y,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatMultDiagonalBlock_Python")
    cdef multDiagonalBlock = PyMat(mat).multDiagonalBlock
    if multDiagonalBlock is None: return UNSUPPORTED(b"multDiagonalBlock")
    multDiagonalBlock(Mat_(mat), Vec_(x), Vec_(y))
    return FunctionEnd()

cdef PetscErrorCode MatSolve_Python(
    PetscMat mat,
    PetscVec b,
    PetscVec x,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatSolve_Python")
    cdef solve = PyMat(mat).solve
    if solve is None: return UNSUPPORTED(b"solve")
    solve(Mat_(mat), Vec_(b), Vec_(x))
    return FunctionEnd()

cdef PetscErrorCode MatSolveTranspose_Python(
    PetscMat mat,
    PetscVec b,
    PetscVec x,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatSolveTranspose_Python")
    cdef solveTranspose = PyMat(mat).solveTranspose
    if solveTranspose is None:
        try:
            mat.ops.solvetranspose = NULL
            CHKERR( MatSolveTranspose(mat, b, x) )
        finally:
            mat.ops.solvetranspose = MatSolveTranspose_Python
    solveTranspose(Mat_(mat), Vec_(b), Vec_(x))
    return FunctionEnd()

cdef PetscErrorCode MatSolveAdd_Python(
    PetscMat mat,
    PetscVec b,
    PetscVec y,
    PetscVec x,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatSolveAdd_Python")
    cdef solveAdd = PyMat(mat).solveAdd
    if solveAdd is None:
        try:
            mat.ops.solveadd = NULL
            CHKERR( MatSolveAdd(mat, b, y, x) )
        finally:
            mat.ops.solveadd = MatSolveAdd_Python
        return FunctionEnd()
    solveAdd(Mat_(mat), Vec_(b), Vec_(y), Vec_(x))
    return FunctionEnd()

cdef PetscErrorCode MatSolveTransposeAdd_Python(
    PetscMat mat,
    PetscVec b,
    PetscVec y,
    PetscVec x,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatSolveTransposeAdd_Python")
    cdef solveTransposeAdd = PyMat(mat).solveTransposeAdd
    if solveTransposeAdd is None:
        try:
            mat.ops.solvetransposeadd = NULL
            CHKERR( MatSolveTransposeAdd(mat, b, y, x) )
        finally:
            mat.ops.solvetransposeadd = MatSolveTransposeAdd_Python
        return FunctionEnd()
    solveTransposeAdd(Mat_(mat), Vec_(b), Vec_(y), Vec_(x))
    return FunctionEnd()

cdef PetscErrorCode MatSOR_Python(
    PetscMat mat,
    PetscVec b,
    PetscReal omega,
    PetscMatSORType sortype,
    PetscReal shift,
    PetscInt its,
    PetscInt lits,
    PetscVec x
    )\
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatSOR_Python")
    cdef SOR = PyMat(mat).SOR
    if SOR is None: return UNSUPPORTED(b"SOR")
    SOR(Mat_(mat), Vec_(b), asReal(omega), asInt(sortype), asReal(shift), asInt(its), asInt(lits), Vec_(x))
    return FunctionEnd()

cdef PetscErrorCode MatGetDiagonal_Python(
    PetscMat mat,
    PetscVec v,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatGetDiagonal_Python")
    cdef getDiagonal = PyMat(mat).getDiagonal
    if getDiagonal is None: return UNSUPPORTED(b"getDiagonal")
    getDiagonal(Mat_(mat), Vec_(v))
    return FunctionEnd()

cdef PetscErrorCode MatSetDiagonal_Python(
    PetscMat mat,
    PetscVec v,
    PetscInsertMode im,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatSetDiagonal_Python")
    cdef setDiagonal = PyMat(mat).setDiagonal
    cdef bint addv = True if im == PETSC_ADD_VALUES else False
    if setDiagonal is None: return UNSUPPORTED(b"setDiagonal")
    setDiagonal(Mat_(mat), Vec_(v), addv)
    return FunctionEnd()

cdef PetscErrorCode MatDiagonalScale_Python(
    PetscMat mat,
    PetscVec l,
    PetscVec r,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatDiagonalScale_Python")
    cdef diagonalScale = PyMat(mat).diagonalScale
    if diagonalScale is None: return UNSUPPORTED(b"diagonalScale")
    diagonalScale(Mat_(mat), Vec_(l), Vec_(r))
    return FunctionEnd()

cdef PetscErrorCode MatMissingDiagonal_Python(
    PetscMat mat,
    PetscBool *missing,
    PetscInt *loc
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatMissingDiagonal_Python")
    cdef missingDiagonal = PyMat(mat).missingDiagonal
    if missingDiagonal is None: return UNSUPPORTED(b"missingDiagonal")
    pymissing, pyloc = missingDiagonal(Mat_(mat))
    missing[0] = <PetscBool>pymissing
    if loc:
        loc[0] = asInt(pyloc)
    return FunctionEnd()

cdef PetscErrorCode MatNorm_Python(
    PetscMat mat,
    PetscNormType ntype,
    PetscReal *nrm,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatNorm_Python")
    cdef norm = PyMat(mat).norm
    if norm is None: return UNSUPPORTED(b"norm")
    retval = norm(Mat_(mat), ntype)
    nrm[0] = <PetscReal>retval
    return FunctionEnd()

cdef PetscErrorCode MatRealPart_Python(
    PetscMat mat,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatRealPart_Python")
    cdef realPart = PyMat(mat).realPart
    if realPart is None: return UNSUPPORTED(b"realPart")
    realPart(Mat_(mat))
    return FunctionEnd()

cdef PetscErrorCode MatImagPart_Python(
    PetscMat mat,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatImagPart_Python")
    cdef imagPart = PyMat(mat).imagPart
    if imagPart is None: return UNSUPPORTED(b"imagPart")
    imagPart(Mat_(mat))
    return FunctionEnd()

cdef PetscErrorCode MatConjugate_Python(
    PetscMat mat,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatConjugate_Python")
    cdef conjugate = PyMat(mat).conjugate
    if conjugate is None: return UNSUPPORTED(b"conjugate")
    conjugate(Mat_(mat))
    return FunctionEnd()

cdef PetscErrorCode MatHasOperation_Python(
    PetscMat mat,
    PetscMatOperation op,
    PetscBool *flag
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatHasOperation_Python")
    flag[0] = PETSC_FALSE
    cdef long i  = <long> op
    global dMatOps
    name = dMatOps.get(i, None)
    cdef void** ops = NULL
    if name is None:
        ops = <void**> mat.ops
        if ops and ops[i]: flag[0] = PETSC_TRUE
    else:
        flag[0] = PETSC_TRUE if getattr(PyMat(mat), name) is not None else PETSC_FALSE
    return FunctionEnd()

cdef PetscErrorCode MatProductNumeric_Python(
    PetscMat mat
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatProductNumeric_Python")
    cdef PetscMat A = NULL
    cdef PetscMat B = NULL
    cdef PetscMat C = NULL
    cdef PetscMatProductType mtype = MATPRODUCT_UNSPECIFIED
    CHKERR( MatProductGetMats(mat, &A, &B, &C) )
    CHKERR( MatProductGetType(mat, &mtype) )

    mtypes = {MATPRODUCT_AB : 'AB', MATPRODUCT_ABt : 'ABt', MATPRODUCT_AtB : 'AtB', MATPRODUCT_PtAP : 'PtAP', MATPRODUCT_RARt: 'RARt', MATPRODUCT_ABC: 'ABC'}

    cdef Mat_Product *product = mat.product
    cdef PetscInt i = <PetscInt> <Py_uintptr_t> product.data
    if i < 0 or i > 2:
      return PetscSETERR(PETSC_ERR_PLIB,
            "Corrupted composed id")
    cdef PetscMat pM = C if i == 2 else B if i == 1 else A

    cdef Mat PyA = Mat_(A)
    cdef Mat PyB = Mat_(B)
    cdef Mat PyC = Mat_(C)
    if mtype == MATPRODUCT_ABC:
      mats = (PyA, PyB, PyC)
    else:
      mats = (PyA, PyB, None)

    cdef productNumeric = PyMat(pM).productNumeric
    if productNumeric is None: return UNSUPPORTED(b"productNumeric")
    productNumeric(PyC if C == pM else PyB if B == pM else PyA, Mat_(mat), mtypes[mtype], *mats)

    return FunctionEnd()

cdef PetscInt matmatid = -1

cdef PetscErrorCode MatProductSymbolic_Python(
    PetscMat mat
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatProductSymbolic_Python")
    cdef PetscMat A = NULL
    cdef PetscMat B = NULL
    cdef PetscMat C = NULL
    cdef PetscMatProductType mtype = MATPRODUCT_UNSPECIFIED
    CHKERR( MatProductGetMats(mat, &A, &B, &C) )
    CHKERR( MatProductGetType(mat, &mtype) )

    mtypes = {MATPRODUCT_AB : 'AB', MATPRODUCT_ABt : 'ABt', MATPRODUCT_AtB : 'AtB', MATPRODUCT_PtAP : 'PtAP', MATPRODUCT_RARt: 'RARt', MATPRODUCT_ABC: 'ABC'}

    global matmatid
    cdef PetscInt i = -1
    cdef PetscBool flg = PETSC_FALSE
    CHKERR( PetscObjectComposedDataGetIntPy(<PetscObject>mat, matmatid, &i, &flg) )
    if flg is not PETSC_TRUE:
      return PetscSETERR(PETSC_ERR_PLIB,
            "Missing composed id")
    if i < 0 or i > 2:
      return PetscSETERR(PETSC_ERR_PLIB,
            "Corrupted composed id")
    cdef PetscMat pM = C if i == 2 else B if i == 1 else A

    cdef Mat PyA = Mat_(A)
    cdef Mat PyB = Mat_(B)
    cdef Mat PyC = Mat_(C)
    if mtype == MATPRODUCT_ABC:
      mats = (PyA, PyB, PyC)
    else:
      mats = (PyA, PyB, None)

    cdef productSymbolic = PyMat(pM).productSymbolic
    if productSymbolic is None: return UNSUPPORTED(b"productSymbolic")
    productSymbolic(PyC if C == pM else PyB if B == pM else PyA, Mat_(mat), mtypes[mtype], *mats)

    # Store id in matrix product
    cdef Mat_Product *product = mat.product
    product.data = <void*> <Py_uintptr_t> i

    cdef MatOps ops = mat.ops
    ops.productnumeric = MatProductNumeric_Python
    return FunctionEnd()

cdef PetscErrorCode MatProductSetFromOptions_Python(
    PetscMat mat
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"MatProductSetFromOptions_Python")
    cdef PetscMat A = NULL
    cdef PetscMat B = NULL
    cdef PetscMat C = NULL
    CHKERR( MatProductGetMats(mat, &A, &B, &C) )
    if A == NULL or B == NULL:
      return PetscSETERR(PETSC_ERR_PLIB,
            "Missing matrices")

    cdef PetscMatProductType mtype = MATPRODUCT_UNSPECIFIED
    CHKERR( MatProductGetType(mat, &mtype) )
    if mtype == MATPRODUCT_UNSPECIFIED:
      return PetscSETERR(PETSC_ERR_PLIB,
            "Unknown product type")

    mtypes = {MATPRODUCT_AB : 'AB', MATPRODUCT_ABt : 'ABt', MATPRODUCT_AtB : 'AtB', MATPRODUCT_PtAP : 'PtAP', MATPRODUCT_RARt: 'RARt', MATPRODUCT_ABC: 'ABC'}

    cdef Mat PyA = Mat_(A)
    cdef Mat PyB = Mat_(B)
    cdef Mat PyC = Mat_(C)
    if mtype == MATPRODUCT_ABC:
      mats = (PyA, PyB, PyC)
    else:
      mats = (PyA, PyB, None)

    # Find Python matrix in mats able to perform the product
    found = False
    cdef PetscBool mispy = PETSC_FALSE
    cdef PetscMat pM = NULL
    cdef Mat mm
    cdef PetscInt i = -1
    for i in range(len(mats)):
      if mats[i] is None: continue
      mm = mats[i]
      pM = <PetscMat>mm.mat
      CHKERR( PetscObjectTypeCompare(<PetscObject>pM, MATPYTHON,  &mispy)  )
      if mispy:
        if PyMat(pM).productSetFromOptions is not None:
          found = PyMat(pM).productSetFromOptions(PyC if C == pM else PyB if B == pM else PyA, mtypes[mtype], *mats)
          if found: break
    if not found:
      return FunctionEnd()

    cdef MatOps ops = mat.ops
    ops.productsymbolic = MatProductSymbolic_Python

    # Store index (within the product) of the Python matrix which is capable of performing the operation
    # Cannot be stored in mat.product.data at this stage
    # Symbolic operation will get this index and store it in the product data
    global matmatid
    if matmatid < 0:
      CHKERR( PetscObjectComposedDataRegisterPy(&matmatid) )
    CHKERR( PetscObjectComposedDataSetIntPy(<PetscObject>mat, matmatid, i) )

    return FunctionEnd()

# --------------------------------------------------------------------

cdef extern from * nogil:
    struct _PCOps:
      PetscErrorCode (*destroy)(PetscPC) except PETSC_ERR_PYTHON
      PetscErrorCode (*setup)(PetscPC) except PETSC_ERR_PYTHON
      PetscErrorCode (*reset)(PetscPC) except PETSC_ERR_PYTHON
      PetscErrorCode (*setfromoptions)(PetscPC,PetscOptionItems*) except PETSC_ERR_PYTHON
      PetscErrorCode (*view)(PetscPC,PetscViewer) except PETSC_ERR_PYTHON
      PetscErrorCode (*presolve)(PetscPC,PetscKSP,PetscVec,PetscVec) except PETSC_ERR_PYTHON
      PetscErrorCode (*postsolve)(PetscPC,PetscKSP,PetscVec,PetscVec) except PETSC_ERR_PYTHON
      PetscErrorCode (*apply)(PetscPC,PetscVec,PetscVec) except PETSC_ERR_PYTHON
      PetscErrorCode (*matapply)(PetscPC,PetscMat,PetscMat) except PETSC_ERR_PYTHON
      PetscErrorCode (*applytranspose)(PetscPC,PetscVec,PetscVec) except PETSC_ERR_PYTHON
      PetscErrorCode (*applysymmetricleft)(PetscPC,PetscVec,PetscVec) except PETSC_ERR_PYTHON
      PetscErrorCode (*applysymmetricright)(PetscPC,PetscVec,PetscVec) except PETSC_ERR_PYTHON
    ctypedef _PCOps *PCOps
    struct _p_PC:
        void *data
        PCOps ops


@cython.internal
cdef class _PyPC(_PyObj): pass
cdef inline _PyPC PyPC(PetscPC pc):
    if pc != NULL and pc.data != NULL:
        return <_PyPC>pc.data
    else:
        return _PyPC.__new__(_PyPC)

cdef public PetscErrorCode PCPythonGetContext(PetscPC pc, void **ctx) \
    except PETSC_ERR_PYTHON:
    FunctionBegin(b"PCPythonGetContext")
    PyPC(pc).getcontext(ctx)
    return FunctionEnd()

cdef public PetscErrorCode PCPythonSetContext(PetscPC pc, void *ctx) \
    except PETSC_ERR_PYTHON:
    FunctionBegin(b"PCPythonSetContext")
    PyPC(pc).setcontext(ctx, PC_(pc))
    return FunctionEnd()

cdef PetscErrorCode PCPythonSetType_PYTHON(PetscPC pc, char name[]) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"PCPythonSetType_PYTHON")
    if name == NULL: return FunctionEnd() # XXX
    cdef object ctx = createcontext(name)
    PCPythonSetContext(pc, <void*>ctx)
    PyPC(pc).setname(name)
    return FunctionEnd()

cdef PetscErrorCode PCPythonGetType_PYTHON(PetscPC pc, const char *name[]) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"PCPythonGetType_PYTHON")
    name[0] = PyPC(pc).getname()
    return FunctionEnd()

cdef PetscErrorCode PCCreate_Python(
    PetscPC pc,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"PCCreate_Python")
    #
    cdef PCOps ops          = pc.ops
    ops.reset               = PCReset_Python
    ops.destroy             = PCDestroy_Python
    ops.setup               = PCSetUp_Python
    ops.setfromoptions      = PCSetFromOptions_Python
    ops.view                = PCView_Python
    ops.presolve            = PCPreSolve_Python
    ops.postsolve           = PCPostSolve_Python
    ops.apply               = PCApply_Python
    ops.matapply            = PCMatApply_Python
    ops.applytranspose      = PCApplyTranspose_Python
    ops.applysymmetricleft  = PCApplySymmetricLeft_Python
    ops.applysymmetricright = PCApplySymmetricRight_Python
    #
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>pc, b"PCPythonSetType_C",
            <PetscVoidFunction>PCPythonSetType_PYTHON) )
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>pc, b"PCPythonGetType_C",
            <PetscVoidFunction>PCPythonGetType_PYTHON) )
    #
    cdef ctx = PyPC(NULL)
    pc.data = <void*> ctx
    Py_INCREF(<PyObject*>pc.data)
    return FunctionEnd()

cdef inline PetscErrorCode PCDestroy_Python_inner(
    PetscPC pc,
    ) \
    except PETSC_ERR_PYTHON with gil:
    try:
        addRef(pc)
        PCPythonSetContext(pc, NULL)
    finally:
        delRef(pc)
        Py_DECREF(<PyObject*>pc.data)
        pc.data = NULL
    return PETSC_SUCCESS

cdef PetscErrorCode PCDestroy_Python(
    PetscPC pc,
    ) \
    nogil except PETSC_ERR_PYTHON:
    FunctionBegin(b"PCDestroy_Python")
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>pc, b"PCPythonSetType_C",
            <PetscVoidFunction>NULL) )
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>pc, b"PCPythonGetType_C",
            <PetscVoidFunction>NULL) )
    #
    if Py_IsInitialized(): PCDestroy_Python_inner(pc)
    return FunctionEnd()

cdef PetscErrorCode PCSetUp_Python(
    PetscPC pc,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"PCSetUp_Python")
    #
    cdef char name[2048]
    cdef PetscBool found = PETSC_FALSE
    if PyPC(pc).self is None:
        CHKERR( PetscOptionsGetString(NULL,
                getPrefix(pc), b"-pc_python_type",
                name, sizeof(name), &found) )
        if found and name[0]:
            CHKERR( PCPythonSetType_PYTHON(pc, name) )
    if PyPC(pc).self is None:
        return PetscSETERR(PETSC_ERR_USER,
            "Python context not set, call one of \n"
            " * PCPythonSetType(pc, \"[package.]module.class\")\n"
            " * PCSetFromOptions(pc) and pass option "
            "-pc_python_type [package.]module.class")
    #
    cdef setUp = PyPC(pc).setUp
    if setUp is not None:
        setUp(PC_(pc))
    #
    cdef o = PyPC(pc)
    cdef PCOps ops = pc.ops
    if o.applyTranspose is None:
        ops.applytranspose = NULL
    if o.applySymmetricLeft is None:
        ops.applysymmetricleft = NULL
    if o.applySymmetricRight is None:
        ops.applysymmetricright = NULL
    #
    return FunctionEnd()

cdef inline PetscErrorCode PCReset_Python_inner(
    PetscPC pc,
    ) \
    except PETSC_ERR_PYTHON with gil:
    cdef reset = PyPC(pc).reset
    if reset is not None:
        reset(PC_(pc))
    return PETSC_SUCCESS

cdef PetscErrorCode PCReset_Python(
    PetscPC pc,
    ) \
    nogil except PETSC_ERR_PYTHON:
    if getRef(pc) == 0: return PETSC_SUCCESS
    FunctionBegin(b"PCReset_Python")
    if Py_IsInitialized(): PCReset_Python_inner(pc)
    return FunctionEnd()

cdef PetscErrorCode PCSetFromOptions_Python(
    PetscPC pc,
    PetscOptionItems *PetscOptionsObject,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"PCSetFromOptions_Python")
    #
    cdef char name[2048], *defval = PyPC(pc).getname()
    cdef PetscBool found = PETSC_FALSE
    cdef PetscOptionItems *opts "PetscOptionsObject" = PetscOptionsObject
    CHKERR( PetscOptionsString(
            b"-pc_python_type", b"Python [package.]module[.{class|function}]",
            b"PCPythonSetType", defval, name, sizeof(name), &found) ); <void>opts;
    if found and name[0]:
        CHKERR( PCPythonSetType_PYTHON(pc, name) )
    #
    cdef setFromOptions = PyPC(pc).setFromOptions
    if setFromOptions is not None:
        setFromOptions(PC_(pc))
    return FunctionEnd()

cdef PetscErrorCode PCView_Python(
    PetscPC     pc,
    PetscViewer vwr,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"PCView_Python")
    viewcontext(PyPC(pc), vwr)
    cdef view = PyPC(pc).view
    if view is not None:
        view(PC_(pc), Viewer_(vwr))
    return FunctionEnd()

cdef PetscErrorCode PCPreSolve_Python(
    PetscPC  pc,
    PetscKSP ksp,
    PetscVec b,
    PetscVec x,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"PCPreSolve_Python")
    cdef preSolve = PyPC(pc).preSolve
    if preSolve is not None:
        preSolve(PC_(pc), KSP_(ksp), Vec_(b), Vec_(x))
    return FunctionEnd()

cdef PetscErrorCode PCPostSolve_Python(
    PetscPC  pc,
    PetscKSP ksp,
    PetscVec b,
    PetscVec x,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"PCPostSolve_Python")
    cdef postSolve = PyPC(pc).postSolve
    if postSolve is not None:
        postSolve(PC_(pc), KSP_(ksp), Vec_(b), Vec_(x))
    return FunctionEnd()

cdef PetscErrorCode PCApply_Python(
    PetscPC  pc,
    PetscVec x,
    PetscVec y,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"PCApply_Python")
    cdef apply = PyPC(pc).apply
    apply(PC_(pc), Vec_(x), Vec_(y))
    return FunctionEnd()

cdef PetscErrorCode PCApplyTranspose_Python(
    PetscPC  pc,
    PetscVec x,
    PetscVec y,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"PCApplyTranspose_Python")
    cdef applyTranspose = PyPC(pc).applyTranspose
    applyTranspose(PC_(pc), Vec_(x), Vec_(y))
    return FunctionEnd()

cdef PetscErrorCode PCApplySymmetricLeft_Python(
    PetscPC  pc,
    PetscVec x,
    PetscVec y,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"PCApplySymmetricLeft_Python")
    cdef applySymmetricLeft = PyPC(pc).applySymmetricLeft
    applySymmetricLeft(PC_(pc), Vec_(x), Vec_(y))
    return FunctionEnd()

cdef PetscErrorCode PCApplySymmetricRight_Python(
    PetscPC  pc,
    PetscVec x,
    PetscVec y,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"PCApplySymmetricRight_Python")
    cdef applySymmetricRight = PyPC(pc).applySymmetricRight
    applySymmetricRight(PC_(pc), Vec_(x), Vec_(y))
    return FunctionEnd()

cdef PetscErrorCode PCMatApply_Python(
    PetscPC  pc,
    PetscMat X,
    PetscMat Y,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"PCMatApply_Python")
    cdef matApply = PyPC(pc).matApply
    if matApply is None:
        try:
            pc.ops.matapply = NULL
            CHKERR( PCMatApply(pc, X, Y) )
        finally:
            pc.ops.matapply = PCMatApply_Python
        return FunctionEnd()
    matApply(PC_(pc), Mat_(X), Mat_(Y))
    return FunctionEnd()


# --------------------------------------------------------------------

cdef extern from * nogil:
    struct _KSPOps:
      PetscErrorCode (*destroy)(PetscKSP) except PETSC_ERR_PYTHON
      PetscErrorCode (*setup)(PetscKSP) except PETSC_ERR_PYTHON
      PetscErrorCode (*reset)(PetscKSP) except PETSC_ERR_PYTHON
      PetscErrorCode (*setfromoptions)(PetscKSP,PetscOptionItems*) except PETSC_ERR_PYTHON
      PetscErrorCode (*view)(PetscKSP,PetscViewer) except PETSC_ERR_PYTHON
      PetscErrorCode (*solve)(PetscKSP) except PETSC_ERR_PYTHON
      PetscErrorCode (*buildsolution)(PetscKSP,PetscVec,PetscVec*) except PETSC_ERR_PYTHON
      PetscErrorCode (*buildresidual)(PetscKSP,PetscVec,PetscVec,PetscVec*) except PETSC_ERR_PYTHON
    ctypedef _KSPOps *KSPOps
    struct _p_KSP:
        void *data
        KSPOps ops
        PetscBool transpose_solve
        PetscInt iter"its",max_its"max_it"
        PetscReal norm"rnorm"
        PetscKSPConvergedReason reason

cdef extern from * nogil: # custom.h
    PetscErrorCode KSPConverged(PetscKSP,PetscInt,PetscReal,PetscKSPConvergedReason*)
    PetscErrorCode KSPLogHistory(PetscKSP,PetscReal)


@cython.internal
cdef class _PyKSP(_PyObj): pass
cdef inline _PyKSP PyKSP(PetscKSP ksp):
    if ksp != NULL and ksp.data != NULL:
        return <_PyKSP>ksp.data
    else:
        return _PyKSP.__new__(_PyKSP)

cdef public PetscErrorCode KSPPythonGetContext(PetscKSP ksp, void **ctx) \
    except PETSC_ERR_PYTHON:
    FunctionBegin(b"KSPPythonGetContext")
    PyKSP(ksp).getcontext(ctx)
    return FunctionEnd()

cdef public PetscErrorCode KSPPythonSetContext(PetscKSP ksp, void *ctx) \
    except PETSC_ERR_PYTHON:
    FunctionBegin(b"KSPPythonSetContext")
    PyKSP(ksp).setcontext(ctx, KSP_(ksp))
    return FunctionEnd()

cdef PetscErrorCode KSPPythonSetType_PYTHON(PetscKSP ksp, char name[]) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"KSPPythonSetType_PYTHON")
    if name == NULL: return FunctionEnd() # XXX
    cdef object ctx = createcontext(name)
    KSPPythonSetContext(ksp, <void*>ctx)
    PyKSP(ksp).setname(name)
    return FunctionEnd()

cdef PetscErrorCode KSPPythonGetType_PYTHON(PetscKSP ksp, const char *name[]) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"KSPPythonGetType_PYTHON")
    name[0] = PyKSP(ksp).getname()
    return FunctionEnd()

cdef PetscErrorCode KSPCreate_Python(
    PetscKSP ksp,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"KSPCreate_Python")
    #
    cdef KSPOps ops    = ksp.ops
    ops.reset          = KSPReset_Python
    ops.destroy        = KSPDestroy_Python
    ops.setup          = KSPSetUp_Python
    ops.setfromoptions = KSPSetFromOptions_Python
    ops.view           = KSPView_Python
    ops.solve          = KSPSolve_Python
    ops.buildsolution  = KSPBuildSolution_Python
    ops.buildresidual  = KSPBuildResidual_Python
    #
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>ksp, b"KSPPythonSetType_C",
            <PetscVoidFunction>KSPPythonSetType_PYTHON) )
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>ksp, b"KSPPythonGetType_C",
            <PetscVoidFunction>KSPPythonGetType_PYTHON) )
    #
    cdef ctx = PyKSP(NULL)
    ksp.data = <void*> ctx
    Py_INCREF(<PyObject*>ksp.data)
    #
    CHKERR( KSPSetSupportedNorm(
            ksp, KSP_NORM_PRECONDITIONED,   PC_LEFT,      3) )
    CHKERR( KSPSetSupportedNorm(
            ksp, KSP_NORM_UNPRECONDITIONED, PC_RIGHT,     3) )
    CHKERR( KSPSetSupportedNorm(
            ksp, KSP_NORM_UNPRECONDITIONED, PC_LEFT,      2) )
    CHKERR( KSPSetSupportedNorm(
            ksp, KSP_NORM_PRECONDITIONED,   PC_RIGHT,     2) )
    CHKERR( KSPSetSupportedNorm(
            ksp, KSP_NORM_PRECONDITIONED,   PC_SYMMETRIC, 1) )
    CHKERR( KSPSetSupportedNorm(
            ksp, KSP_NORM_UNPRECONDITIONED, PC_SYMMETRIC, 1) )
    return FunctionEnd()

cdef inline PetscErrorCode KSPDestroy_Python_inner(
    PetscKSP ksp,
    ) \
    except PETSC_ERR_PYTHON with gil:
    try:
        addRef(ksp)
        KSPPythonSetContext(ksp, NULL)
    finally:
        delRef(ksp)
        Py_DECREF(<PyObject*>ksp.data)
        ksp.data = NULL
    return PETSC_SUCCESS

cdef PetscErrorCode KSPDestroy_Python(
    PetscKSP ksp,
    ) \
    nogil except PETSC_ERR_PYTHON:
    FunctionBegin(b"KSPDestroy_Python")
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>ksp, b"KSPPythonSetType_C",
            <PetscVoidFunction>NULL))
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>ksp, b"KSPPythonGetType_C",
            <PetscVoidFunction>NULL))
    #
    if Py_IsInitialized(): KSPDestroy_Python_inner(ksp)
    return FunctionEnd()

cdef PetscErrorCode KSPSetUp_Python(
    PetscKSP ksp,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"KSPSetUp_Python")
    #
    cdef char name[2048]
    cdef PetscBool found = PETSC_FALSE
    if PyKSP(ksp).self is None:
        CHKERR( PetscOptionsGetString(NULL,
                getPrefix(ksp), b"-ksp_python_type",
                name, sizeof(name), &found) )
        if found and name[0]:
            CHKERR( KSPPythonSetType_PYTHON(ksp, name) )
    if PyKSP(ksp).self is None:
        return PetscSETERR(PETSC_ERR_USER,
            "Python context not set, call one of \n"
            " * KSPPythonSetType(ksp, \"[package.]module.class\")\n"
            " * KSPSetFromOptions(ksp) and pass option "
            "-ksp_python_type [package.]module.class")
    #
    cdef setUp = PyKSP(ksp).setUp
    if setUp is not None:
        setUp(KSP_(ksp))
    return FunctionEnd()

cdef inline PetscErrorCode KSPReset_Python_inner(
    PetscKSP ksp,
    ) \
    except PETSC_ERR_PYTHON with gil:
    cdef reset = PyKSP(ksp).reset
    if reset is not None:
        reset(KSP_(ksp))
    return PETSC_SUCCESS

cdef PetscErrorCode KSPReset_Python(
    PetscKSP ksp,
    ) \
    nogil except PETSC_ERR_PYTHON:
    if getRef(ksp) == 0: return PETSC_SUCCESS
    FunctionBegin(b"KSPReset_Python")
    CHKERR( PetscObjectCompose(<PetscObject>ksp, b"@ksp.vec_work_sol", NULL) )
    CHKERR( PetscObjectCompose(<PetscObject>ksp, b"@ksp.vec_work_res", NULL) )
    if Py_IsInitialized(): KSPReset_Python_inner(ksp)
    return FunctionEnd()

cdef PetscErrorCode KSPSetFromOptions_Python(
    PetscKSP ksp,
    PetscOptionItems *PetscOptionsObject
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"KSPSetFromOptions_Python")
    #
    cdef char name[2048], *defval = PyKSP(ksp).getname()
    cdef PetscBool found = PETSC_FALSE
    cdef PetscOptionItems *opts "PetscOptionsObject" = PetscOptionsObject
    CHKERR( PetscOptionsString(
            b"-ksp_python_type", b"Python [package.]module[.{class|function}]",
            b"KSPPythonSetType", defval, name, sizeof(name), &found) ); <void>opts;
    if found and name[0]:
        CHKERR( KSPPythonSetType_PYTHON(ksp, name) )
    #
    cdef setFromOptions = PyKSP(ksp).setFromOptions
    if setFromOptions is not None:
        setFromOptions(KSP_(ksp))
    return FunctionEnd()

cdef PetscErrorCode KSPView_Python(
    PetscKSP    ksp,
    PetscViewer vwr,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"KSPView_Python")
    viewcontext(PyKSP(ksp), vwr)
    cdef view = PyKSP(ksp).view
    if view is not None:
        view(KSP_(ksp), Viewer_(vwr))
    return FunctionEnd()

cdef PetscErrorCode KSPBuildSolution_Python(
    PetscKSP ksp,
    PetscVec v,
    PetscVec *V,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"KSPBuildSolution_Python")
    cdef PetscVec x = v
    cdef buildSolution = PyKSP(ksp).buildSolution
    if buildSolution is not None:
        if x == NULL: pass # XXX
        buildSolution(KSP_(ksp), Vec_(x))
        if V != NULL: V[0] = x
    else:
        CHKERR( KSPBuildSolutionDefault(ksp, v, V) )
    return FunctionEnd()

cdef PetscErrorCode KSPBuildResidual_Python(
    PetscKSP ksp,
    PetscVec t,
    PetscVec v,
    PetscVec *V,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"KSPBuildResidual_Python")
    cdef buildResidual = PyKSP(ksp).buildResidual
    if buildResidual is not None:
        buildResidual(KSP_(ksp), Vec_(t), Vec_(v))
        if V != NULL: V[0] = v
    else:
        CHKERR( KSPBuildResidualDefault(ksp, t, v, V) )
    return FunctionEnd()

cdef PetscErrorCode KSPSolve_Python(
    PetscKSP ksp,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"KSPSolve_Python")
    cdef PetscVec B = NULL, X = NULL
    CHKERR( KSPGetRhs(ksp, &B) )
    CHKERR( KSPGetSolution(ksp, &X) )
    #
    ksp.iter = 0
    ksp.reason = KSP_CONVERGED_ITERATING
    #
    cdef solve = None
    if ksp.transpose_solve:
        solve = PyKSP(ksp).solveTranspose
    else:
        solve = PyKSP(ksp).solve
    if solve is not None:
        solve(KSP_(ksp), Vec_(B), Vec_(X))
    else:
        KSPSolve_Python_default(ksp, B, X)
    return FunctionEnd()

cdef PetscErrorCode KSPSolve_Python_default(
    PetscKSP ksp,
    PetscVec B,
    PetscVec X,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"KSPSolve_Python_default")
    #
    cdef PetscVec t = NULL
    CHKERR( PetscObjectQuery(
            <PetscObject>ksp,
             b"@ksp.vec_work_sol",
             <PetscObject*>&t) )
    if t == NULL:
        CHKERR( VecDuplicate(X, &t) )
        CHKERR( PetscObjectCompose(
                <PetscObject>ksp,
                 b"@ksp.vec_work_sol",
                 <PetscObject>t) )
    cdef PetscVec v = NULL
    CHKERR( PetscObjectQuery(
            <PetscObject>ksp,
             b"@ksp.vec_work_res",
             <PetscObject*>&v) )
    if v == NULL:
        CHKERR( VecDuplicate(B, &v) )
        CHKERR( PetscObjectCompose(
                <PetscObject>ksp,
                 b"@ksp.vec_work_res",
                 <PetscObject>v) )
    #
    cdef PetscInt its = 0
    cdef PetscVec R = NULL
    cdef PetscReal rnorm = 0
    #
    CHKERR( KSPBuildResidual(ksp, t, v, &R) )
    CHKERR( VecNorm(R, PETSC_NORM_2, &rnorm) )
    #
    CHKERR( KSPConverged(ksp, ksp.iter, rnorm, &ksp.reason) )
    CHKERR( KSPLogHistory(ksp, ksp.norm) )
    CHKERR( KSPMonitor(ksp, ksp.iter, ksp.norm) )
    for its from 0 <= its < ksp.max_its:
        if ksp.reason: break
        KSPPreStep_Python(ksp)
        #
        KSPStep_Python(ksp, B, X)
        CHKERR( KSPBuildResidual(ksp, t, v, &R) )
        CHKERR( VecNorm(R, PETSC_NORM_2, &rnorm) )
        ksp.iter += 1
        #
        KSPPostStep_Python(ksp)
        CHKERR( KSPConverged(ksp, ksp.iter, rnorm, &ksp.reason) )
        CHKERR( KSPLogHistory(ksp, ksp.norm) )
        CHKERR( KSPMonitor(ksp, ksp.iter, ksp.norm) )
    if ksp.iter == ksp.max_its:
        if ksp.reason == KSP_CONVERGED_ITERATING:
            ksp.reason = KSP_DIVERGED_MAX_IT
    #
    return FunctionEnd()

cdef PetscErrorCode KSPPreStep_Python(
    PetscKSP ksp,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"KSPPreStep_Python")
    cdef preStep = PyKSP(ksp).preStep
    if preStep is not None:
        preStep(KSP_(ksp))
    return FunctionEnd()

cdef PetscErrorCode KSPPostStep_Python(
    PetscKSP ksp,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"KSPPostStep_Python")
    cdef postStep = PyKSP(ksp).postStep
    if postStep is not None:
        postStep(KSP_(ksp))
    return FunctionEnd()

cdef PetscErrorCode KSPStep_Python(
    PetscKSP ksp,
    PetscVec B,
    PetscVec X,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"KSPStep_Python")
    cdef step = None
    if ksp.transpose_solve:
        step = PyKSP(ksp).stepTranspose
        if step is None: return UNSUPPORTED(b"stepTranspose")
    else:
        step = PyKSP(ksp).step
        if step is None: return UNSUPPORTED(b"step")
    step(KSP_(ksp), Vec_(B), Vec_(X))
    return FunctionEnd()

# --------------------------------------------------------------------

cdef extern from * nogil:
    struct _SNESOps:
      PetscErrorCode (*destroy)(PetscSNES) except PETSC_ERR_PYTHON
      PetscErrorCode (*setup)(PetscSNES) except PETSC_ERR_PYTHON
      PetscErrorCode (*reset)(PetscSNES) except PETSC_ERR_PYTHON
      PetscErrorCode (*setfromoptions)(PetscSNES,PetscOptionItems*) except PETSC_ERR_PYTHON
      PetscErrorCode (*view)(PetscSNES,PetscViewer) except PETSC_ERR_PYTHON
      PetscErrorCode (*solve)(PetscSNES) except PETSC_ERR_PYTHON
    ctypedef _SNESOps *SNESOps
    struct _p_SNES:
        void *data
        SNESOps ops
        PetscInt  iter,max_its,linear_its
        PetscReal norm,rtol,ttol
        PetscSNESConvergedReason reason
        PetscVec vec_sol,vec_sol_update,vec_func
        PetscMat jacobian,jacobian_pre
        PetscKSP ksp

cdef extern from * nogil: # custom.h
    PetscErrorCode SNESConverged(PetscSNES,PetscInt,PetscReal,PetscReal,PetscReal,PetscSNESConvergedReason*)
    PetscErrorCode SNESLogHistory(PetscSNES,PetscReal,PetscInt)


@cython.internal
cdef class _PySNES(_PyObj): pass
cdef inline _PySNES PySNES(PetscSNES snes):
    if snes != NULL and snes.data != NULL:
        return <_PySNES>snes.data
    else:
        return _PySNES.__new__(_PySNES)

cdef public PetscErrorCode SNESPythonGetContext(PetscSNES snes, void **ctx) \
    except PETSC_ERR_PYTHON:
    FunctionBegin(b"SNESPythonGetContext ")
    PySNES(snes).getcontext(ctx)
    return FunctionEnd()

cdef public PetscErrorCode SNESPythonSetContext(PetscSNES snes, void *ctx) \
    except PETSC_ERR_PYTHON:
    FunctionBegin(b"SNESPythonSetContext ")
    PySNES(snes).setcontext(ctx, SNES_(snes))
    return FunctionEnd()

cdef PetscErrorCode SNESPythonSetType_PYTHON(PetscSNES snes, char name[]) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"SNESPythonSetType_PYTHON")
    if name == NULL: return FunctionEnd() # XXX
    cdef object ctx = createcontext(name)
    SNESPythonSetContext(snes, <void*>ctx)
    PySNES(snes).setname(name)
    return FunctionEnd()

cdef PetscErrorCode SNESPythonGetType_PYTHON(PetscSNES snes, const char *name[]) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"SNESPythonGetType_PYTHON")
    name[0] = PySNES(snes).getname()
    return FunctionEnd()

cdef PetscErrorCode SNESCreate_Python(
    PetscSNES snes,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"SNESCreate_Python")
    #
    cdef SNESOps ops   = snes.ops
    cdef PetscSNESLineSearch ls = NULL
    ops.reset          = SNESReset_Python
    ops.destroy        = SNESDestroy_Python
    ops.setup          = SNESSetUp_Python
    ops.setfromoptions = SNESSetFromOptions_Python
    ops.view           = SNESView_Python
    ops.solve          = SNESSolve_Python
    #
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>snes, b"SNESPythonSetType_C",
            <PetscVoidFunction>SNESPythonSetType_PYTHON) )
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>snes, b"SNESPythonGetType_C",
            <PetscVoidFunction>SNESPythonGetType_PYTHON) )
    #
    cdef ctx = PySNES(NULL)
    snes.data = <void*> ctx

    # Ensure that the SNES has a linesearch object early enough that
    # it gets setFromOptions.
    CHKERR( SNESGetLineSearch(snes, &ls) )
    Py_INCREF(<PyObject*>snes.data)
    return FunctionEnd()

cdef inline PetscErrorCode SNESDestroy_Python_inner(
    PetscSNES snes,
    ) \
    except PETSC_ERR_PYTHON with gil:
    try:
        addRef(snes)
        SNESPythonSetContext(snes, NULL)
    finally:
        delRef(snes)
        Py_DECREF(<PyObject*>snes.data)
        snes.data = NULL
    return PETSC_SUCCESS

cdef PetscErrorCode SNESDestroy_Python(
    PetscSNES snes,
    ) \
    nogil except PETSC_ERR_PYTHON:
    FunctionBegin(b"SNESDestroy_Python")
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>snes, b"SNESPythonSetType_C",
            <PetscVoidFunction>NULL) )
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>snes, b"SNESPythonGetType_C",
            <PetscVoidFunction>NULL) )
    #
    if Py_IsInitialized(): SNESDestroy_Python_inner(snes)
    return FunctionEnd()

cdef PetscErrorCode SNESSetUp_Python(
    PetscSNES snes,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"SNESSetUp_Python")
    #
    #SNESGetKSP(snes,&snes.ksp)
    #
    cdef char name[2048]
    cdef PetscBool found = PETSC_FALSE
    if PySNES(snes).self is None:
        CHKERR( PetscOptionsGetString(NULL,
                getPrefix(snes), b"-snes_python_type",
                name, sizeof(name), &found) )
        if found and name[0]:
            CHKERR( SNESPythonSetType_PYTHON(snes, name) )
    if PySNES(snes).self is None:
        return PetscSETERR(PETSC_ERR_USER,
            "Python context not set, call one of \n"
            " * SNESPythonSetType(snes, \"[package.]module.class\")\n"
            " * SNESSetFromOptions(snes) and pass option "
            "-snes_python_type [package.]module.class")
    #
    cdef setUp = PySNES(snes).setUp
    if setUp is not None:
        setUp(SNES_(snes))
    return FunctionEnd()

cdef inline PetscErrorCode SNESReset_Python_inner(
    PetscSNES snes,
    ) \
    except PETSC_ERR_PYTHON with gil:
    cdef reset = PySNES(snes).reset
    if reset is not None:
        reset(SNES_(snes))
    return PETSC_SUCCESS

cdef PetscErrorCode SNESReset_Python(
    PetscSNES snes,
    ) \
    nogil except PETSC_ERR_PYTHON:
    if getRef(snes) == 0: return PETSC_SUCCESS
    FunctionBegin(b"SNESReset_Python")
    if Py_IsInitialized(): SNESReset_Python_inner(snes)
    return FunctionEnd()

cdef PetscErrorCode SNESSetFromOptions_Python(
    PetscSNES snes,
    PetscOptionItems *PetscOptionsObject,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"SNESSetFromOptions_Python")
    #
    cdef char name[2048], *defval = PySNES(snes).getname()
    cdef PetscBool found = PETSC_FALSE
    cdef PetscOptionItems *opts "PetscOptionsObject" = PetscOptionsObject
    CHKERR( PetscOptionsString(
            b"-snes_python_type", b"Python [package.]module[.{class|function}]",
            b"SNESPythonSetType", defval, name, sizeof(name), &found) ); <void>opts;
    if found and name[0]:
        CHKERR( SNESPythonSetType_PYTHON(snes, name) )
    #
    cdef setFromOptions = PySNES(snes).setFromOptions
    if setFromOptions is not None:
        setFromOptions(SNES_(snes))
    return FunctionEnd()

cdef PetscErrorCode SNESView_Python(
    PetscSNES   snes,
    PetscViewer vwr,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"SNESView_Python")
    viewcontext(PySNES(snes), vwr)
    cdef view = PySNES(snes).view
    if view is not None:
        view(SNES_(snes), Viewer_(vwr))
    return FunctionEnd()

cdef PetscErrorCode SNESSolve_Python(
    PetscSNES snes,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"SNESSolve_Python")
    cdef PetscVec b = NULL, x = NULL
    CHKERR( SNESGetRhs(snes, &b) )
    CHKERR( SNESGetSolution(snes, &x) )
    #
    snes.iter = 0
    snes.reason = SNES_CONVERGED_ITERATING
    #
    cdef solve = PySNES(snes).solve
    if solve is not None:
        solve(SNES_(snes), Vec_(b) if b != NULL else None, Vec_(x))
    else:
        SNESSolve_Python_default(snes)
    #
    return FunctionEnd()

cdef PetscErrorCode SNESSolve_Python_default(
    PetscSNES snes,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"SNESSolve_Python_default")
    #
    cdef PetscVec X=NULL, F=NULL, Y=NULL
    cdef PetscSNESLineSearch ls
    CHKERR( SNESGetSolution(snes, &X) )
    CHKERR( SNESGetFunction(snes, &F, NULL, NULL) )
    CHKERR( SNESGetSolutionUpdate(snes, &Y) )
    CHKERR( SNESGetLineSearch(snes, &ls) )
    cdef PetscInt  its=0, lits=0
    cdef PetscReal xnorm = 0.0
    cdef PetscReal fnorm = 0.0
    cdef PetscReal ynorm = 0.0
    #
    CHKERR( VecSet(Y, 0.0) )
    CHKERR( SNESComputeFunction(snes, X, F) )
    CHKERR( VecNorm(X, PETSC_NORM_2, &xnorm) )
    CHKERR( VecNorm(F, PETSC_NORM_2, &fnorm) )
    #
    CHKERR( SNESConverged(snes, snes.iter, xnorm, ynorm, fnorm, &snes.reason) )
    CHKERR( SNESLogHistory(snes, snes.norm, lits) )
    CHKERR( SNESMonitor(snes, snes.iter, snes.norm) )

    cdef PetscObjectState ostate = -1
    cdef PetscObjectState nstate = -1
    for its from 0 <= its < snes.max_its:
        if snes.reason: break
        CHKERR( PetscObjectStateGet(<PetscObject>X, &ostate) )
        SNESPreStep_Python(snes)
        CHKERR( PetscObjectStateGet(<PetscObject>X, &nstate) )
        if ostate != nstate:
            CHKERR( SNESComputeFunction(snes, X, F) )
            CHKERR( VecNorm(F, PETSC_NORM_2, &fnorm) )
        #
        lits = -snes.linear_its
        SNESStep_Python(snes, X, F, Y)
        lits += snes.linear_its
        #
        CHKERR( SNESLineSearchApply(ls, X, F, NULL, Y) )
        CHKERR( SNESLineSearchGetNorms(ls, &xnorm, &fnorm, &ynorm) )
        snes.iter += 1
        #
        SNESPostStep_Python(snes)
        CHKERR( SNESConverged(snes, snes.iter, xnorm, ynorm, fnorm, &snes.reason) )
        CHKERR( SNESLogHistory(snes, snes.norm, lits) )
        CHKERR( SNESMonitor(snes, snes.iter, snes.norm) )
    if snes.iter == snes.max_its:
        if snes.reason == SNES_CONVERGED_ITERATING:
            snes.reason = SNES_DIVERGED_MAX_IT
    #
    return FunctionEnd()

cdef PetscErrorCode SNESPreStep_Python(
    PetscSNES snes,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"SNESPreStep_Python")
    cdef preStep = PySNES(snes).preStep
    if preStep is not None:
        preStep(SNES_(snes))
    return FunctionEnd()

cdef PetscErrorCode SNESPostStep_Python(
    PetscSNES snes,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"SNESPostStep_Python")
    cdef postStep = PySNES(snes).postStep
    if postStep is not None:
        postStep(SNES_(snes))
    return FunctionEnd()

cdef PetscErrorCode SNESStep_Python(
    PetscSNES snes,
    PetscVec  X,
    PetscVec  F,
    PetscVec  Y,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"SNESStep_Python")
    cdef step = PySNES(snes).step
    if step is not None:
        step(SNES_(snes), Vec_(X), Vec_(F), Vec_(Y))
    else:
        SNESStep_Python_default(snes, X, F, Y)
    return FunctionEnd()

cdef PetscErrorCode SNESStep_Python_default(
    PetscSNES snes,
    PetscVec  X,
    PetscVec  F,
    PetscVec  Y,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"SNESStep_Python_default")
    cdef PetscMat J = NULL, P = NULL
    cdef PetscInt lits = 0
    CHKERR( SNESGetJacobian(snes, &J, &P, NULL, NULL) )
    CHKERR( SNESComputeJacobian(snes, X, J, P) )
    CHKERR( KSPSetOperators(snes.ksp, J, P) )
    CHKERR( KSPSolve(snes.ksp, F, Y) )
    CHKERR( KSPGetIterationNumber(snes.ksp, &lits) )
    snes.linear_its += lits
    return FunctionEnd()

# --------------------------------------------------------------------


cdef extern from * nogil:
    struct _TSOps:
      PetscErrorCode (*destroy)(PetscTS) except PETSC_ERR_PYTHON
      PetscErrorCode (*setup)(PetscTS) except PETSC_ERR_PYTHON
      PetscErrorCode (*reset)(PetscTS) except PETSC_ERR_PYTHON
      PetscErrorCode (*setfromoptions)(PetscTS,PetscOptionItems*) except PETSC_ERR_PYTHON
      PetscErrorCode (*view)(PetscTS,PetscViewer) except PETSC_ERR_PYTHON
      PetscErrorCode (*step)(PetscTS) except PETSC_ERR_PYTHON
      PetscErrorCode (*rollback)(PetscTS) except PETSC_ERR_PYTHON
      PetscErrorCode (*interpolate)(PetscTS,PetscReal,PetscVec) except PETSC_ERR_PYTHON
      PetscErrorCode (*evaluatestep)(PetscTS,PetscInt,PetscVec,PetscBool*) except PETSC_ERR_PYTHON
      PetscErrorCode (*solve)(PetscTS) except PETSC_ERR_PYTHON
      PetscErrorCode (*snesfunction)(PetscSNES,PetscVec,PetscVec,PetscTS) except PETSC_ERR_PYTHON
      PetscErrorCode (*snesjacobian)(PetscSNES,PetscVec,PetscMat,PetscMat,PetscTS) except PETSC_ERR_PYTHON
    ctypedef _TSOps *TSOps
    struct _TSUserOps:
      PetscErrorCode (*prestep)(PetscTS) except PETSC_ERR_PYTHON
      PetscErrorCode (*prestage)(PetscTS,PetscReal) except PETSC_ERR_PYTHON
      PetscErrorCode (*poststage)(PetscTS,PetscReal,PetscInt,PetscVec*) except PETSC_ERR_PYTHON
      PetscErrorCode (*poststep)(PetscTS) except PETSC_ERR_PYTHON
      PetscErrorCode (*rhsfunction)(PetscTS,PetscReal,PetscVec,PetscVec,void*) except PETSC_ERR_PYTHON
      PetscErrorCode (*ifunction)  (PetscTS,PetscReal,PetscVec,PetscVec,PetscVec,void*) except PETSC_ERR_PYTHON
      PetscErrorCode (*rhsjacobian)(PetscTS,PetscReal,PetscVec,PetscMat,PetscMat,void*) except PETSC_ERR_PYTHON
      PetscErrorCode (*ijacobian)  (PetscTS,PetscReal,PetscVec,PetscVec,PetscReal,PetscMat,PetscMat,void*) except PETSC_ERR_PYTHON
    ctypedef _TSUserOps *TSUserOps
    struct _p_TS:
        void *data
        PetscDM dm
        PetscTSAdapt adapt
        TSOps ops
        TSUserOps userops
        PetscTSProblemType problem_type
        PetscInt  snes_its
        PetscInt  ksp_its
        PetscInt  reject
        PetscInt  max_reject
        PetscInt  steps
        PetscReal ptime
        PetscVec  vec_sol
        PetscReal time_step
        PetscInt  max_steps
        PetscReal max_time
        PetscTSConvergedReason reason
        PetscSNES snes
        PetscBool usessnes


@cython.internal
cdef class _PyTS(_PyObj): pass
cdef inline _PyTS PyTS(PetscTS ts):
    if ts != NULL and ts.data != NULL:
        return <_PyTS>ts.data
    else:
        return _PyTS.__new__(_PyTS)

cdef public PetscErrorCode TSPythonGetContext(PetscTS ts, void **ctx) \
    except PETSC_ERR_PYTHON:
    FunctionBegin(b"TSPythonGetContext")
    PyTS(ts).getcontext(ctx)
    return FunctionEnd()

cdef public PetscErrorCode TSPythonSetContext(PetscTS ts, void *ctx) \
    except PETSC_ERR_PYTHON:
    FunctionBegin(b"TSPythonSetContext")
    PyTS(ts).setcontext(ctx, TS_(ts))
    return FunctionEnd()

cdef PetscErrorCode TSPythonSetType_PYTHON(PetscTS ts, char name[]) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TSPythonSetType_PYTHON")
    if name == NULL: return FunctionEnd() # XXX
    cdef object ctx = createcontext(name)
    TSPythonSetContext(ts, <void*>ctx)
    PyTS(ts).setname(name)
    return FunctionEnd()

cdef PetscErrorCode TSPythonGetType_PYTHON(PetscTS ts, const char *name[]) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TSPythonGetType_PYTHON")
    name[0] = PyTS(ts).getname()
    return FunctionEnd()

cdef PetscErrorCode TSCreate_Python(
    PetscTS ts,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TSCreate_Python")
    #
    cdef TSOps ops     = ts.ops
    ops.reset          = TSReset_Python
    ops.destroy        = TSDestroy_Python
    ops.setup          = TSSetUp_Python
    ops.setfromoptions = TSSetFromOptions_Python
    ops.view           = TSView_Python
    ops.step           = TSStep_Python
    ops.rollback       = TSRollBack_Python
    ops.interpolate    = TSInterpolate_Python
    ops.evaluatestep   = TSEvaluateStep_Python
    ops.snesfunction   = SNESTSFormFunction_Python
    ops.snesjacobian   = SNESTSFormJacobian_Python
    #
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>ts, b"TSPythonSetType_C",
            <PetscVoidFunction>TSPythonSetType_PYTHON) )
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>ts, b"TSPythonGetType_C",
            <PetscVoidFunction>TSPythonGetType_PYTHON) )
    #
    ts.usessnes = PETSC_TRUE
    #
    cdef ctx = PyTS(NULL)
    ts.data = <void*> ctx
    Py_INCREF(<PyObject*>ts.data)
    return FunctionEnd()

cdef inline PetscErrorCode TSDestroy_Python_inner(
    PetscTS ts,
    ) \
    except PETSC_ERR_PYTHON with gil:
    try:
        addRef(ts)
        TSPythonSetContext(ts, NULL)
    finally:
        delRef(ts)
        Py_DECREF(<PyObject*>ts.data)
        ts.data = NULL
    return PETSC_SUCCESS

cdef PetscErrorCode TSDestroy_Python(
    PetscTS ts,
    ) \
    nogil except PETSC_ERR_PYTHON:
    FunctionBegin(b"TSDestroy_Python")
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>ts, b"TSPythonSetType_C",
            <PetscVoidFunction>NULL) )
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>ts, b"TSPythonGetType_C",
            <PetscVoidFunction>NULL) )
    #
    if Py_IsInitialized(): TSDestroy_Python_inner(ts)
    return FunctionEnd()

cdef PetscErrorCode TSSetUp_Python(
    PetscTS ts,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TSSetUp_Python")
    #
    cdef PetscVec vec_update = NULL
    CHKERR( VecDuplicate(ts.vec_sol, &vec_update) )
    CHKERR( PetscObjectCompose(<PetscObject>ts,
                                b"@ts.vec_update",
                                <PetscObject>vec_update) )
    CHKERR( VecDestroy(&vec_update) )
    cdef PetscVec vec_dot = NULL
    CHKERR( VecDuplicate(ts.vec_sol, &vec_dot) )
    CHKERR( PetscObjectCompose(<PetscObject>ts,
                                b"@ts.vec_dot",
                                <PetscObject>vec_dot) )
    CHKERR( VecDestroy(&vec_dot) )
    #
    cdef char name[2048]
    cdef PetscBool found = PETSC_FALSE
    if PyTS(ts).self is None:
        CHKERR( PetscOptionsGetString(NULL,
                getPrefix(ts), b"-ts_python_type",
                name, sizeof(name), &found) )
        if found and name[0]:
            CHKERR( TSPythonSetType_PYTHON(ts, name) )
    if PyTS(ts).self is None:
        return PetscSETERR(PETSC_ERR_USER,
            "Python context not set, call one of \n"
            " * TSPythonSetType(ts, \"[package.]module.class\")\n"
            " * TSSetFromOptions(ts) and pass option "
            "-ts_python_type [package.]module.class")
    #
    cdef setUp = PyTS(ts).setUp
    if setUp is not None:
        setUp(TS_(ts))
    return FunctionEnd()

cdef inline PetscErrorCode TSReset_Python_inner(
    PetscTS ts,
    ) \
    except PETSC_ERR_PYTHON with gil:
    cdef reset = PyTS(ts).reset
    if reset is not None:
        reset(TS_(ts))
    return PETSC_SUCCESS

cdef PetscErrorCode TSReset_Python(
    PetscTS ts,
    ) \
    nogil except PETSC_ERR_PYTHON:
    if getRef(ts) == 0: return PETSC_SUCCESS
    FunctionBegin(b"TSReset_Python")
    CHKERR( PetscObjectCompose(<PetscObject>ts, b"@ts.vec_update", NULL) )
    CHKERR( PetscObjectCompose(<PetscObject>ts, b"@ts.vec_dot",    NULL) )
    if Py_IsInitialized(): TSReset_Python_inner(ts)
    return FunctionEnd()

cdef PetscErrorCode TSSetFromOptions_Python(
    PetscTS ts,
    PetscOptionItems *PetscOptionsObject,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TSSetFromOptions_Python")
    cdef char name[2048], *defval = PyTS(ts).getname()
    cdef PetscBool found = PETSC_FALSE
    cdef PetscOptionItems *opts "PetscOptionsObject" = PetscOptionsObject
    CHKERR( PetscOptionsString(
            b"-ts_python_type", b"Python [package.]module[.{class|function}]",
            b"TSPythonSetType", defval, name, sizeof(name), &found) ); <void>opts;
    if found and name[0]:
        CHKERR( TSPythonSetType_PYTHON(ts, name) )
    #
    cdef setFromOptions = PyTS(ts).setFromOptions
    if setFromOptions is not None:
        setFromOptions(TS_(ts))
    CHKERR( SNESSetFromOptions(ts.snes) )
    return FunctionEnd()

cdef PetscErrorCode TSView_Python(
    PetscTS ts,
    PetscViewer vwr,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TSView_Python")
    viewcontext(PyTS(ts), vwr)
    cdef view = PyTS(ts).view
    if view is not None:
        view(TS_(ts), Viewer_(vwr))
    return FunctionEnd()

cdef PetscErrorCode TSStep_Python(
    PetscTS   ts,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TSStep_Python")
    cdef step = PyTS(ts).step
    if step is not None:
        step(TS_(ts))
    else:
        TSStep_Python_default(ts)
    return FunctionEnd()

cdef PetscErrorCode TSRollBack_Python(
    PetscTS   ts,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TSRollBack_Python")
    cdef rollback = PyTS(ts).rollback
    if rollback is None: return UNSUPPORTED(b"rollback")
    rollback(TS_(ts))
    return FunctionEnd()

cdef PetscErrorCode TSInterpolate_Python(
    PetscTS   ts,
    PetscReal t,
    PetscVec  x,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TSInterpolate _Python")
    cdef interpolate = PyTS(ts).interpolate
    if interpolate is None: return UNSUPPORTED(b"interpolate")
    interpolate(TS_(ts), toReal(t), Vec_(x))
    return FunctionEnd()

cdef PetscErrorCode TSEvaluateStep_Python(
    PetscTS   ts,
    PetscInt  o,
    PetscVec  x,
    PetscBool *flag,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TSEvaluateStep _Python")
    cdef evaluatestep = PyTS(ts).evaluatestep
    if evaluatestep is None: return UNSUPPORTED(b"evaluatestep")
    cdef done = evaluatestep(TS_(ts), toInt(o), Vec_(x))
    if flag != NULL:
        flag[0] = PETSC_TRUE if done else PETSC_FALSE
    elif not done:
        return PetscSETERR(PETSC_ERR_USER, "Cannot evaluate step")
    return FunctionEnd()

cdef PetscErrorCode SNESTSFormFunction_Python(
    PetscSNES snes,
    PetscVec  x,
    PetscVec  f,
    PetscTS   ts,
    ) \
    except PETSC_ERR_PYTHON with gil:
    #
    cdef formSNESFunction = PyTS(ts).formSNESFunction
    if formSNESFunction is not None:
        args = (SNES_(snes), Vec_(x), Vec_(f), TS_(ts))
        formSNESFunction(args)
        return FunctionEnd()
    #
    cdef PetscVec dx = NULL
    CHKERR( PetscObjectQuery(
            <PetscObject>ts,
             b"@ts.vec_dot",
             <PetscObject*>&dx) )
    #
    cdef PetscReal t = ts.ptime + ts.time_step
    cdef PetscReal a = 1.0/ts.time_step
    CHKERR( VecCopy(ts.vec_sol, dx) )
    CHKERR( VecAXPBY(dx, +a, -a, x) )
    CHKERR( TSComputeIFunction(ts, t, x, dx, f, PETSC_FALSE) )
    return FunctionEnd()

cdef PetscErrorCode SNESTSFormJacobian_Python(
    PetscSNES snes,
    PetscVec  x,
    PetscMat  A,
    PetscMat  B,
    PetscTS   ts,
    ) \
    except PETSC_ERR_PYTHON with gil:
    #
    cdef formSNESJacobian = PyTS(ts).formSNESJacobian
    if formSNESJacobian is not None:
        args = (SNES_(snes), Vec_(x), Mat_(A), Mat_(B), TS_(ts))
        formSNESJacobian(*args)
        return FunctionEnd()
    #
    cdef PetscVec dx = NULL
    CHKERR( PetscObjectQuery(
            <PetscObject>ts,
             b"@ts.vec_dot",
             <PetscObject*>&dx) )
    #
    cdef PetscReal t = ts.ptime + ts.time_step
    cdef PetscReal a = 1.0/ts.time_step
    CHKERR( VecCopy(ts.vec_sol, dx) )
    CHKERR( VecAXPBY(dx, +a, -a, x) )
    CHKERR( TSComputeIJacobian(ts, t, x, dx, a, A, B, PETSC_FALSE) )
    return FunctionEnd()

cdef PetscErrorCode TSSolveStep_Python(
    PetscTS   ts,
    PetscReal t,
    PetscVec  x,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TSSolveStep_Python")
    #
    cdef solveStep = PyTS(ts).solveStep
    if solveStep is not None:
        solveStep(TS_(ts), <double>t, Vec_(x))
        return FunctionEnd()
    #
    cdef PetscInt nits = 0, lits = 0
    CHKERR( SNESSolve(ts.snes, NULL, x) )
    CHKERR( SNESGetIterationNumber(ts.snes, &nits) )
    CHKERR( SNESGetLinearSolveIterations(ts.snes, &lits) )
    ts.snes_its += nits
    ts.ksp_its  += lits
    return FunctionEnd()

cdef PetscErrorCode TSAdaptStep_Python(
    PetscTS   ts,
    PetscReal t,
    PetscVec  x,
    PetscReal *nextdt,
    PetscBool *stepok,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TSAdaptStep_Python")
    nextdt[0] = ts.time_step
    stepok[0] = PETSC_TRUE
    #
    cdef adaptStep = PyTS(ts).adaptStep
    if adaptStep is None: return FunctionEnd()
    cdef object retval
    cdef double dt
    cdef bint   ok
    retval = adaptStep(TS_(ts), <double>t, Vec_(x))
    if retval is None: pass
    elif isinstance(retval, float):
        dt = retval
        nextdt[0] = <PetscReal>dt
        stepok[0] = PETSC_TRUE
    elif isinstance(retval, bool):
        ok = retval
        nextdt[0] = ts.time_step
        stepok[0] = PETSC_TRUE if ok else PETSC_FALSE
    else:
        dt, ok = retval
        nextdt[0] = <PetscReal>dt
        stepok[0] = PETSC_TRUE if ok else PETSC_FALSE
    return FunctionEnd()

cdef PetscErrorCode TSStep_Python_default(
    PetscTS ts,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TSStep_Python_default")
    cdef PetscVec vec_update = NULL
    CHKERR( PetscObjectQuery(
            <PetscObject>ts,
             b"@ts.vec_update",
             <PetscObject*>&vec_update) )
    #
    cdef PetscInt  r = 0
    cdef PetscReal tt = ts.ptime
    cdef PetscReal dt = ts.time_step
    cdef PetscBool accept  = PETSC_TRUE
    cdef PetscBool stageok = PETSC_TRUE
    for r from 0 <= r < ts.max_reject:
        tt = ts.ptime + ts.time_step
        CHKERR( VecCopy(ts.vec_sol, vec_update) )
        CHKERR( TSPreStage(ts, tt+dt) )
        TSSolveStep_Python(ts, tt, vec_update)
        CHKERR( TSPostStage(ts, tt+dt, 0, &vec_update) );
        CHKERR( TSAdaptCheckStage(ts.adapt, ts, tt+dt, vec_update, &stageok) );
        if not stageok:
            ts.reject += 1
            continue
        TSAdaptStep_Python(ts, tt, vec_update, &dt, &accept)
        if not accept:
            ts.time_step = dt
            ts.reject += 1
            continue
        CHKERR( VecCopy(vec_update, ts.vec_sol) )
        ts.ptime += ts.time_step
        ts.time_step = dt
        break
    if (not stageok or not accept) and ts.reason == 0:
        ts.reason = TS_DIVERGED_STEP_REJECTED
    return FunctionEnd()

# --------------------------------------------------------------------


cdef extern from * nogil:
    struct _TaoOps:
      PetscErrorCode (*destroy)(PetscTAO) except PETSC_ERR_PYTHON
      PetscErrorCode (*setup)(PetscTAO) except PETSC_ERR_PYTHON
      PetscErrorCode (*solve)(PetscTAO) except PETSC_ERR_PYTHON
      PetscErrorCode (*setfromoptions)(PetscTAO,PetscOptionItems*) except PETSC_ERR_PYTHON
      PetscErrorCode (*view)(PetscTAO,PetscViewer) except PETSC_ERR_PYTHON
    ctypedef _TaoOps *TaoOps
    struct _p_TAO:
        void *data
        TaoOps ops
        PetscInt niter, max_it
        PetscTAOConvergedReason reason
        PetscInt ksp_its, ksp_tot_its
        PetscKSP ksp
        PetscVec gradient
        PetscVec stepdirection
        PetscTAOLineSearch linesearch

cdef extern from * nogil: # custom.h
    PetscErrorCode TaoConverged(PetscTAO,PetscTAOConvergedReason*)

cdef extern from * nogil: # custom.h
    PetscErrorCode TaoGetVecs(PetscTAO,PetscVec*,PetscVec*,PetscVec*)
    PetscErrorCode TaoCheckReals(PetscTAO,PetscReal,PetscReal)
    PetscErrorCode TaoComputeUpdate(PetscTAO)
    PetscErrorCode TaoCreateDefaultLineSearch(PetscTAO)
    PetscErrorCode TaoCreateDefaultKSP(PetscTAO)
    PetscErrorCode TaoApplyLineSearch(PetscTAO,PetscReal*,PetscReal*,PetscTAOLineSearchConvergedReason*)


@cython.internal
cdef class _PyTao(_PyObj): pass
cdef inline _PyTao PyTao(PetscTAO tao):
    if tao != NULL and tao.data != NULL:
        return <_PyTao>tao.data
    else:
        return _PyTao.__new__(_PyTao)

cdef public PetscErrorCode TaoPythonGetContext(PetscTAO tao, void **ctx) \
    except PETSC_ERR_PYTHON:
    FunctionBegin(b"TaoPythonGetContext")
    PyTao(tao).getcontext(ctx)
    return FunctionEnd()

cdef public PetscErrorCode TaoPythonSetContext(PetscTAO tao, void *ctx) \
    except PETSC_ERR_PYTHON:
    FunctionBegin(b"TaoPythonSetContext")
    PyTao(tao).setcontext(ctx, TAO_(tao))
    return FunctionEnd()

cdef PetscErrorCode TaoPythonSetType_PYTHON(PetscTAO tao, char name[]) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TaoPythonSetType_PYTHON")
    if name == NULL: return FunctionEnd() # XXX
    cdef object ctx = createcontext(name)
    TaoPythonSetContext(tao, <void*>ctx)
    PyTao(tao).setname(name)
    return FunctionEnd()

cdef PetscErrorCode TaoPythonGetType_PYTHON(PetscTAO tao, const char *name[]) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TaoPythonGetType_PYTHON")
    name[0] = PyTao(tao).getname()
    return FunctionEnd()

cdef PetscErrorCode TaoCreate_Python(
    PetscTAO tao,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TaoCreate_Python")
    #
    cdef TaoOps ops    = tao.ops
    ops.destroy        = TaoDestroy_Python
    ops.view           = TaoView_Python
    ops.solve          = TaoSolve_Python
    ops.setup          = TaoSetUp_Python
    ops.setfromoptions = TaoSetFromOptions_Python
    #
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>tao, b"TaoPythonSetType_C",
            <PetscVoidFunction>TaoPythonSetType_PYTHON) )
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>tao, b"TaoPythonGetType_C",
            <PetscVoidFunction>TaoPythonGetType_PYTHON) )
    #
    CHKERR( TaoCreateDefaultLineSearch(tao) )
    CHKERR( TaoCreateDefaultKSP(tao) )
    #
    cdef ctx = PyTao(NULL)
    tao.data = <void*> ctx
    Py_INCREF(<PyObject*>tao.data)
    return FunctionEnd()

cdef inline PetscErrorCode TaoDestroy_Python_inner(
    PetscTAO tao,
    ) \
    except PETSC_ERR_PYTHON with gil:
    try:
        addRef(tao)
        TaoPythonSetContext(tao, NULL)
    finally:
        delRef(tao)
        Py_DECREF(<PyObject*>tao.data)
        tao.data = NULL
    return PETSC_SUCCESS

cdef PetscErrorCode TaoDestroy_Python(
    PetscTAO tao,
    ) \
    nogil except PETSC_ERR_PYTHON:
    FunctionBegin(b"TaoDestroy_Python")
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>tao, b"TaoPythonSetType_C",
            <PetscVoidFunction>NULL) )
    CHKERR( PetscObjectComposeFunction(
            <PetscObject>tao, b"TaoPythonGetType_C",
            <PetscVoidFunction>NULL) )
    #
    if Py_IsInitialized(): TaoDestroy_Python_inner(tao)
    return FunctionEnd()

cdef PetscErrorCode TaoSetUp_Python(
    PetscTAO tao,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TaoSetUp_Python")
    cdef char name[2048]
    cdef PetscBool found = PETSC_FALSE
    if PyTao(tao).self is None:
        CHKERR( PetscOptionsGetString(NULL,
                getPrefix(tao), b"-tao_python_type",
                name, sizeof(name), &found) )
        if found and name[0]:
            CHKERR( TaoPythonSetType_PYTHON(tao, name) )
    if PyTao(tao).self is None:
        return PetscSETERR(PETSC_ERR_USER,
            "Python context not set, call one of \n"
            " * TaoPythonSetType(tao, \"[package.]module.class\")\n"
            " * TaoSetFromOptions(tao) and pass option "
            "-tao_python_type [package.]module.class")
    #
    cdef setUp = PyTao(tao).setUp
    if setUp is not None:
        setUp(TAO_(tao))
    return FunctionEnd()

cdef PetscErrorCode TaoSetFromOptions_Python(
    PetscTAO tao,
    PetscOptionItems *PetscOptionsObject,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TaoSetFromOptions_Python")
    #
    cdef char name[2048], *defval = PyTao(tao).getname()
    cdef PetscBool found = PETSC_FALSE
    cdef PetscOptionItems *opts "PetscOptionsObject" = PetscOptionsObject
    CHKERR( PetscOptionsString(
            b"-tao_python_type", b"Python [package.]module[.{class|function}]",
            b"TaoPythonSetType", defval, name, sizeof(name), &found) ); <void>opts;
    if found and name[0]:
        CHKERR( TaoPythonSetType_PYTHON(tao, name) )
    #
    cdef setFromOptions = PyTao(tao).setFromOptions
    if setFromOptions is not None:
        setFromOptions(TAO_(tao))
    CHKERR( KSPSetFromOptions(tao.ksp) )
    return FunctionEnd()

cdef PetscErrorCode TaoView_Python(
    PetscTAO tao,
    PetscViewer vwr,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TaoView_Python")
    viewcontext(PyTao(tao), vwr)
    cdef view = PyTao(tao).view
    if view is not None:
        view(TAO_(tao), Viewer_(vwr))
    return FunctionEnd()

cdef PetscErrorCode TaoSolve_Python(
    PetscTAO tao,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TaoSolve_Python")
    #
    tao.niter = 0
    tao.ksp_its = 0
    tao.reason = TAO_CONTINUE_ITERATING
    #
    cdef solve = PyTao(tao).solve
    if solve is not None:
        solve(TAO_(tao))
    else:
        TaoSolve_Python_default(tao)
    #
    return FunctionEnd()

cdef PetscErrorCode TaoSolve_Python_default(
    PetscTAO tao,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TaoSolve_Python_default")
    #
    cdef PetscVec X = NULL, G = NULL, S = NULL
    CHKERR( TaoGetVecs(tao, &X, &G, &S) )
    #
    cdef PetscReal f = 0.0
    cdef PetscReal gnorm = 0.0
    cdef PetscReal step = 1.0
    #
    if G != NULL:
        CHKERR( TaoComputeObjectiveAndGradient(tao, X, &f, G) )
        CHKERR( VecNorm(G, PETSC_NORM_2, &gnorm) )
    else:
        CHKERR( TaoComputeObjective(tao, X, &f) )
    CHKERR( TaoCheckReals(tao, f, gnorm) )

    CHKERR( TaoLogConvergenceHistory(tao, f, gnorm, 0.0, tao.ksp_its) )
    CHKERR( TaoMonitor(tao, tao.niter, f, gnorm, 0.0, step) )
    CHKERR( TaoConverged(tao, &tao.reason) )

    cdef PetscObjectState ostate = -1
    cdef PetscObjectState nstate = -1
    cdef PetscInt its = 0
    cdef PetscTAOLineSearchConvergedReason lsr = TAOLINESEARCH_SUCCESS
    for its from 0 <= its < tao.max_it:
        if tao.reason: break
        CHKERR( PetscObjectStateGet(<PetscObject>X, &ostate) )
        CHKERR( TaoComputeUpdate(tao) )

        TaoPreStep_Python(tao)
        CHKERR( PetscObjectStateGet(<PetscObject>X, &nstate) )
        if ostate != nstate:
            if G != NULL:
                CHKERR( TaoComputeObjectiveAndGradient(tao, X, &f, G) )
                CHKERR( VecNorm(G, PETSC_NORM_2, &gnorm) )
            else:
                CHKERR( TaoComputeObjective(tao, X, &f) )
        #
        tao.ksp_its = 0
        TaoStep_Python(tao, X, G, S)
        CHKERR( KSPGetIterationNumber(tao.ksp, &tao.ksp_its) )
        tao.ksp_tot_its += tao.ksp_its
        #
        if G != NULL:
            CHKERR( TaoApplyLineSearch(tao, &f, &step, &lsr) )
            CHKERR( VecNorm(G, PETSC_NORM_2, &gnorm) )
            if lsr < TAOLINESEARCH_CONTINUE_ITERATING:
                tao.reason = TAO_DIVERGED_LS_FAILURE
        else:
            CHKERR( TaoComputeObjective(tao, X, &f) )
        CHKERR( TaoCheckReals(tao, f, gnorm) )

        tao.niter += 1
        #
        TaoPostStep_Python(tao)
        CHKERR( TaoLogConvergenceHistory(tao, f, gnorm, 0.0, tao.ksp_its) )
        CHKERR( TaoMonitor(tao, tao.niter, f, gnorm, 0.0, step) )
        CHKERR( TaoConverged(tao, &tao.reason) )

    if tao.niter == tao.max_it:
        if tao.reason <= 0:
            tao.reason = TAO_DIVERGED_MAXITS
    #
    return FunctionEnd()

cdef PetscErrorCode TaoStep_Python(
    PetscTAO tao,
    PetscVec X,
    PetscVec G,
    PetscVec S,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TaoStep_Python")
    cdef step = PyTao(tao).step
    if step is not None:
        step(TAO_(tao), Vec_(X), Vec_(G) if G != NULL else None, Vec_(S) if S != NULL else None)
    else:
        # TaoStep_Python_default(tao,X,G,S)
        CHKERR( TaoComputeGradient(tao, X, S) )
        CHKERR( VecCopy(G, S) )
        CHKERR( VecScale(S, -1.0) )
    return FunctionEnd()

cdef PetscErrorCode TaoPreStep_Python(
    PetscTAO tao,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TaoPreStep_Python")
    cdef preStep = PyTao(tao).preStep
    if preStep is not None:
        preStep(TAO_(tao))
    return FunctionEnd()

cdef PetscErrorCode TaoPostStep_Python(
    PetscTAO tao,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"TaoPostStep_Python")
    cdef postStep = PyTao(tao).postStep
    if postStep is not None:
        postStep(TAO_(tao))
    return FunctionEnd()

# --------------------------------------------------------------------

cdef PetscErrorCode PetscPythonMonitorSet_Python(
    PetscObject obj_p,
    const char *url_p,
    ) \
    except PETSC_ERR_PYTHON with gil:
    FunctionBegin(b"PetscPythonMonitorSet_Python")
    assert obj_p != NULL
    assert url_p != NULL
    assert url_p[0] != 0
    #
    cdef PetscClassId classid = 0
    CHKERR( PetscObjectGetClassId(obj_p, &classid) )
    cdef type klass = PyPetscType_Lookup(classid)
    cdef Object ob = klass()
    ob.obj[0] = newRef(obj_p)
    #
    cdef url = bytes2str(url_p)
    if ':' in url:
        path, names = parse_url(url)
    else:
        path, names = url, 'monitor'
    module = load_module(path)
    for attr in names.split(','):
        monitor = getattr(module, attr)
        if isinstance(monitor, type):
            monitor = monitor(ob)
        ob.setMonitor(monitor)
    #
    return FunctionEnd()

# --------------------------------------------------------------------

cdef extern from * nogil:

  ctypedef PetscErrorCode MatCreateFunction  (PetscMat)  except PETSC_ERR_PYTHON
  ctypedef PetscErrorCode PCCreateFunction   (PetscPC)   except PETSC_ERR_PYTHON
  ctypedef PetscErrorCode KSPCreateFunction  (PetscKSP)  except PETSC_ERR_PYTHON
  ctypedef PetscErrorCode SNESCreateFunction (PetscSNES) except PETSC_ERR_PYTHON
  ctypedef PetscErrorCode TSCreateFunction   (PetscTS)   except PETSC_ERR_PYTHON
  ctypedef PetscErrorCode TaoCreateFunction  (PetscTAO)  except PETSC_ERR_PYTHON

  PetscErrorCode MatRegister  (const char[],MatCreateFunction* )
  PetscErrorCode PCRegister   (const char[],PCCreateFunction*  )
  PetscErrorCode KSPRegister  (const char[],KSPCreateFunction* )
  PetscErrorCode SNESRegister (const char[],SNESCreateFunction*)
  PetscErrorCode TSRegister   (const char[],TSCreateFunction*  )
  PetscErrorCode TaoRegister  (const char[],TaoCreateFunction* )

  PetscErrorCode (*PetscPythonMonitorSet_C) \
      (PetscObject, const char[]) except PETSC_ERR_PYTHON


cdef public PetscErrorCode PetscPythonRegisterAll() except PETSC_ERR_PYTHON:
    FunctionBegin(b"PetscPythonRegisterAll")

    # Python subtypes
    CHKERR( MatRegister ( MATPYTHON,  MatCreate_Python  ) )
    CHKERR( PCRegister  ( PCPYTHON,   PCCreate_Python   ) )
    CHKERR( KSPRegister ( KSPPYTHON,  KSPCreate_Python  ) )
    CHKERR( SNESRegister( SNESPYTHON, SNESCreate_Python ) )
    CHKERR( TSRegister  ( TSPYTHON,   TSCreate_Python   ) )
    CHKERR( TaoRegister ( TAOPYTHON,  TaoCreate_Python  ) )

    # Python monitors
    global PetscPythonMonitorSet_C
    PetscPythonMonitorSet_C = PetscPythonMonitorSet_Python

    return FunctionEnd()

# --------------------------------------------------------------------
