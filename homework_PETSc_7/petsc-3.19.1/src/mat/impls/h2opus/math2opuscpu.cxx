#include <h2opusconf.h>
#if !defined(H2OPUS_USE_GPU)
  #include "../src/mat/impls/h2opus/cuda/math2opus.cu"
#endif
