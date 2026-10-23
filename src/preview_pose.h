#ifndef TOMB_PREVIEW_POSE_H
#define TOMB_PREVIEW_POSE_H
#include <stdint.h>
/* Rigid orientation interpolation; original packed rotations use Y-X-Z order.
   The result is a column-major OpenGL matrix. Fraction is in [0,1]. */
void tomb_preview_rotation(const uint16_t a[3],const uint16_t b[3],double fraction,float out[16]);
#endif
