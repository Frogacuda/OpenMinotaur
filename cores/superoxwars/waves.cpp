#include "waves.h"
#include "original_tables.h"
namespace superoxwars {
void WaveController::init(int script, const std::array<uint32_t, 1024> &seeds) {
    script_ = script % int(original::Waves.size());
    seeds_ = seeds;
    active = false;
    cursor = 0;
    rowDelay = 4;
    groupCursor = 0;
    archetypeCount = {};
}
void WaveController::rowCrossed() {
    if (active || blocked || --rowDelay > 0)
        return;
    const auto *data = original::Waves[size_t(script_)];
    archetype = data[cursor];
    remaining = data[cursor + 1];
    interval = countdown = data[cursor + 2];
    rowDelay = data[cursor + 3];
    active = true;
    group = groupCursor++ & 7;
    world_.groupSize[size_t(group)] = remaining;
    world_.groupKills[size_t(group)] = 0;
    waveOrdinal = archetypeCount[size_t(archetype)]++;
    cursor += 4;
    if (cursor >= int(original::WavesLength[size_t(script_)]))
        cursor = 0;
    // The original saves one rand result, temporarily reseeds for the wave
    // parameters, then restores that result with srand. A scoped setup stream
    // is exactly that isolation; actors continue to use World's single stream.
    world_.random.next();
    neon::DarwinRandom setup(seeds_[size_t((cursor & 0xffc) / 4)]);
    podPhase_ = float(setup.range(-3.141592654, 3.141592654));
    podPhaseStep_ = float(setup.range(.02, .2));
    if (setup.next() & 1)
        podPhase_ = -podPhase_;
    phase_[0] = float(setup.range(-3.141592654, 3.141592654));
    phaseStep_[0] = float(setup.range(.02, .06));
    if (setup.next() & 1)
        phase_[0] = -phase_[0];
    amplitudePhase_ = float(setup.range(-3.141592654, 3.141592654));
    amplitudeStep_ = float(setup.range(.02, .2));
    if (setup.next() & 1)
        amplitudePhase_ = -amplitudePhase_;
    phase_[1] = float(setup.range(-3.141592654, 3.141592654));
    phaseStep_[1] = float(setup.range(.02, .06));
    if (setup.next() & 1)
        phase_[1] = -phase_[1];
    sweep_ = float(setup.range(-1, 1));
    sweepStep_ = float(setup.range(.04, .2));
    if (setup.next() & 1)
        sweepStep_ = -sweepStep_;
    int fixed = archetype == 0    ? 0
                : archetype == 8  ? 1
                : archetype == 9  ? 2
                : archetype == 27 ? 3
                : archetype == 28 ? 4
                                  : -1;
    path = fixed >= 0 ? original::FixedPodPaths[size_t(fixed)] : int((setup.next() & 4095) % 12);
    ordinal = 0;
    fraction = 0;
    fractionStep = 1.f / float(remaining);
}
void WaveController::tick() {
    if (!active || --countdown > 0)
        return;
    countdown = interval;
    if (--remaining < 1)
        active = false;
    podPhase_ += podPhaseStep_;
    phase_[0] += phaseStep_[0];
    amplitudePhase_ += amplitudeStep_;
    phase_[1] += phaseStep_[1];
    sweep_ += sweepStep_;
    if (sweep_ < -.3f || sweep_ > .3f)
        sweepStep_ = -sweepStep_;
    PodLaunch p;
    p.phase = phase_[0];
    p.phaseStep = phaseStep_[0];
    p.secondPhase = phase_[1];
    p.secondStep = phaseStep_[1];
    p.archetype = archetype;
    p.path = path;
    p.ordinal = ordinal;
    p.waveOrdinal = waveOrdinal;
    p.group = group;
    p.fraction = fraction;
    switch (path) {
    case 0:
        p.position.x = (double(fraction) - .5) * .3;
        break;
    case 3:
        p.position.x = double(std::sin(podPhase_)) * .3;
        break;
    case 4:
        p.position.x = double(float(sweep_ + std::sin(podPhase_))) * .15;
        break;
    case 5:
        p.position.x = double(float(std::sin(podPhase_) + std::sin(amplitudePhase_))) * .15;
        break;
    case 6:
        p.position.x = double(float(std::sin(float(podPhase_ + amplitudePhase_))) * .3f);
        break;
    case 7:
        p.position.x = double(float(float(std::sin(podPhase_) * std::sin(amplitudePhase_)) * .3f));
        break;
    case 8:
        p.position.x = double(float(ordinal)) * .05 - .3;
        break;
    case 9:
        p.position.x = double(float(ordinal)) * -.05 + .3;
        break;
    case 10:
        p.position.x = 0;
        break;
    case 11:
    case 12:
    case 13:
        p.position.x = world_.random.range(-.3, .3);
        break;
    default:
        p.position.x = double(sweep_) * .3;
        break;
    }
    world_.add(Domain::LaunchPods, new LaunchPod(world_, p));
    ++ordinal;
    fraction += fractionStep;
}
LaunchPod::LaunchPod(World &w, const PodLaunch &p) : GameEntity(w), launch(p) {
    frame = 2;
    radius = .02f;
    colour.a = 1;
    position = p.position;
    retained = position;
    velocity = {0, -.003};
    scale = scale * .3;
    if (p.path >= 8) {
        launch.path = 2;
        remaining = 400;
        double targetX = 0, targetY = -.6;
        if (p.path == 8)
            targetX = .35;
        else if (p.path == 9)
            targetX = -.35;
        else if (p.path == 10 || p.path == 11)
            targetX = double(p.fraction) - .5;
        else {
            targetX = p.path == 12 ? -.9 : .9;
            targetY = double(p.fraction) * .2 - .3;
        }
        velocity = {(targetX - position.x) / 400., (targetY - position.y) / 400.};
    } else if (p.path >= 3 && p.path <= 7)
        launch.path = 1;
    else
        launch.path = 0;
}
void LaunchPod::run() {
    colour = world.palette[0];
    bool completed = false;
    if (launch.path == 2) {
        position.y += velocity.y;
        retained.x += velocity.x;
        position.x = retained.x;
        if (--remaining < 0) {
            retained.x /= 1.5;
            completed = true;
        }
    } else {
        position.y += velocity.y;
        if (launch.path == 1) {
            position.x = double(float(std::sin(launch.phase) * launch.amplitude)) + retained.x;
            launch.phase += launch.phaseStep;
        } else
            position.x = retained.x;
        completed = position.y < -.7;
    }
    if (completed) {
        dead = true;
        if (world.launchEnemy)
            world.launchEnemy(*this);
        else
            world.unsupported("Launch pod combat actor dependency");
    }
}
} // namespace superoxwars
