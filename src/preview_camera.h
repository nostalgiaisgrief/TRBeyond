#ifndef TOMB_PREVIEW_CAMERA_H
#define TOMB_PREVIEW_CAMERA_H
#include "step.h"
/* Column-major original-world -> OpenGL eye transform. Original Y is down;
   camera forward becomes GL -Z. Returns 0 for a degenerate camera. */
int tomb_preview_view(const double eye[3],const double target[3],double out[16]);
/* Original 80-degree horizontal projection, integer focal length. */
TOMB_EXPORT int tomb_preview_focal(int width);

/* Preserve the DOS 640x480 vertical view; wider windows reveal the sides. */
TOMB_EXPORT double tomb_preview_aspect_focal(int height);

#endif
