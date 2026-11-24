Historical shader reference fixtures

The two expanded material shaders retain the exact includes from before the
2026-10-23 optimization pass. The SSAO fixture retains the original 25-tap box
filter. These are comparison data, never runtime assets. Keep them frozen when
changing production shaders; deliberate visual changes need reviewed references.

tests/shader_equivalence.c compares these against live material shaders and the
actual concatenated ssao_blur_fs string extracted from main.c. Run its executable
from the engine root. It requests an offscreen OpenGL 3.3 context and verifies
rendered floating-point images; it does not measure native GPU performance.

The SSAO equivalence requires GL_LINEAR, GL_CLAMP_TO_EDGE and matching source /
destination sizes, as configured by the runtime. It is not claimed for arbitrary
subpixel UV offsets or resampling between different image dimensions.

The box kernels are mathematically equivalent. Real normalized GL_R8 filtering
may round intermediate paired samples; the test allows one R8 code value there.
GL_R32F source cases verify the mathematical equivalence within float tolerance.
