struct LegacyDeflexLine : ProportionalLine {
    void oldUpdate() {
        phase += phaseRate;
        if (state == 1) {
            if (hold > 0 && --hold == 0) {
                state = 2;
                progress = 1;
            }
            return;
        }
        if (state != 0 && state != 2)
            return;
        const auto factor = state == 0 ? inScale : outScale;
        pos = (state == 0 ? from : away).lerp(to, progress);
        currentScale = (scale * factor).lerp(scale, progress);
        const float curve = state == 0 ? inCurve : outCurve;
        currentSpacing =
            float(double(curve) + (double(spacing) - double(curve)) * double(progress));
        if (state == 0) {
            progress += inRate;
            if (progress >= 1) {
                progress = 1;
                state = 1;
            }
        } else {
            progress -= outRate;
            if (progress <= 0) {
                progress = 0;
                text.clear();
            }
        }
        alpha = progress;
    }
};
