#ifndef MEC_VELOCITY_H_
#define MEC_VELOCITY_H_

#include <cmath>

namespace mec {

// map an instantaneous pressure rise (dz, in tracker curvature units per
// frame) to a note-on velocity in (0.0,1.0].
// scale : gain applied to dz before clamping; dz for typical strikes is
//         roughly 0.005 (slow press) to 0.05 (fast strike), so useful scales
//         are in the tens.
// curve : pow shaping, matching Voices velocity. 1.0 = linear,
//         > 1.0 boosts soft touches, < 1.0 needs firmer strikes.
inline float velocityFromDz(float dz, float scale, float curve) {
    float v = dz * scale;
    if (v < 0.0f) v = 0.0f;
    else if (v > 1.0f) v = 1.0f;
    v = 1.0f - powf(1.0f - v, curve);
    if (v > 1.0f) v = 1.0f;
    if (v < 0.01f) v = 0.01f;
    return v;
}

}  // namespace mec

#endif  // MEC_VELOCITY_H_
