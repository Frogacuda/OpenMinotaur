#include "controls.h"
#include <algorithm>
namespace superoxwars {
namespace {
constexpr float Exponents[] = {1, 1.5f, 2, 3};
constexpr const char *Names[] = {"LINEAR", "GENTLE", "QUADRATIC", "CUBIC"};
} // namespace
Vec2 AnalogProfile::apply(Vec2 stick) const {
    const double length = stick.length();
    if (length == 0)
        return {};
    const double magnitude =
        std::pow(std::min(length, 1.), Exponents[std::clamp(curve, 0, 3)]) * gain;
    // Equal pixel speed along both axes in the native 320x480 game surface.
    return {stick.x / length * magnitude * .02, stick.y / length * magnitude * (.02 * 320. / 480.)};
}
const char *AnalogProfile::curveName() const {
    return Names[std::clamp(curve, 0, 3)];
}
double AnalogControls::worldInfluence(float rightX) {
    // The source has no tilt threshold or clamp. Full stick travel covers
    // the +/-1 g static-gravity range; the platform already applies deadzone.
    return std::isfinite(rightX) ? std::clamp(double(rightX), -1., 1.) : 0.;
}
} // namespace superoxwars
