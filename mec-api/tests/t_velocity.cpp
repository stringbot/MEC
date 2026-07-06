#include <cassert>
#include <iostream>

#include <devices/mec_velocity.h>
#include <mec_log.h>

int main(int argc, char **argv) {
    LOG_0("velocity test started");

    static constexpr float SCALE = 50.0f;
    static constexpr float CURVE = 4.0f;

    // typical tracker dz values: slow press ~0.002, medium ~0.01, fast strike ~0.05
    float slow = mec::velocityFromDz(0.002f, SCALE, CURVE);
    float medium = mec::velocityFromDz(0.01f, SCALE, CURVE);
    float fast = mec::velocityFromDz(0.05f, SCALE, CURVE);

    // in range
    assert(slow >= 0.01f && slow <= 1.0f);
    assert(medium >= 0.01f && medium <= 1.0f);
    assert(fast >= 0.01f && fast <= 1.0f);

    // monotonic in dz
    assert(slow < medium);
    assert(medium < fast);

    // sensible spread: a fast strike should be near max, a slow press well below it
    assert(fast > 0.9f);
    assert(slow < 0.5f);

    // edge cases: negative and zero dz floor at the minimum velocity
    assert(mec::velocityFromDz(0.0f, SCALE, CURVE) == 0.01f);
    assert(mec::velocityFromDz(-1.0f, SCALE, CURVE) == 0.01f);

    // huge dz clamps to max
    assert(mec::velocityFromDz(100.0f, SCALE, CURVE) == 1.0f);

    // linear curve passes dz*scale through unshaped
    float lin = mec::velocityFromDz(0.01f, SCALE, 1.0f);
    assert(lin > 0.49f && lin < 0.51f);

    // curve > 1 boosts soft touches relative to linear
    assert(mec::velocityFromDz(0.005f, SCALE, 4.0f) > mec::velocityFromDz(0.005f, SCALE, 1.0f));

    LOG_0("velocity test completed");
    return 0;
}
