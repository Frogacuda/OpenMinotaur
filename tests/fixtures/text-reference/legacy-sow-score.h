struct LegacySowScore : superoxwars::FloatingScore {
    using superoxwars::FloatingScore::FloatingScore;
    void oldRun() {
        position += velocity;
        colour = world.palette[size_t(paletteSlot)];
        colour.a = double(opacity);
        opacity -= .01f;
        if (opacity < 0)
            dead = true;
    }
};
