-include ../../../../../petscdir.mk
#requiresdefine   'PETSC_HAVE_ATTRIBUTEALIGNED'

OBJECTS_WATER = waterreaddata.o waterfunctions.o water.o

include ${PETSC_DIR}/lib/petsc/conf/variables
include ${PETSC_DIR}/lib/petsc/conf/rules


water: $(OBJECTS_WATER)
	-${CLINKER} -o water $(OBJECTS_WATER) ${PETSC_SNES_LIB}
	${RM} $(OBJECTS_WATER)

