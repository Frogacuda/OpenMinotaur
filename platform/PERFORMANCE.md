# Neon presentation performance

Android spaces frame starts at least 1/60 second apart while an available Neon bezel is visible, after its loading assessment has completed. The wait occurs before input polling and works alongside display synchronization. A delayed frame advances the next start from its actual time; the host does not submit a burst of catch-up frames. Pausing, hiding the bezel, or selecting another letterboxing method resets this schedule. Desktop presentation retains its display-synchronized behavior.

The submission schedule is independent of bezel update cadence. The existing loading presentation makes one full-rate or half-rate choice, then locks it for that game run. The first four timing samples are discarded. At least 12 samples are required, with at most 240 retained. A 90th-percentile CPU frame-work time above 14.5 ms, or more than roughly 20 percent of intervals above 22 ms, selects half rate. Insufficient evidence retains full rate. Ordinary gameplay and orientation changes do not restart assessment.

Full-rate Neon updates once per presented game frame. Half-rate Neon updates every other frame and presents the completed output between updates. Both advance animation clocks by elapsed time and render each effect graph once per update. There is no GPU catch-up rendering. Feedback trails can differ at reduced cadence because intermediate feedback steps are not replayed.

Android and iOS default to low-resolution intermediates; desktop defaults to high quality. The LiveSynth Quality option remains player controlled. The initial desktop title-screen check can select its first default only while no explicit quality preference exists.

The collection submits offscreen effect work before touching the window buffer. Its exclusive presentation pass uses known state rather than querying and restoring the caller's state. The host establishes its game-image composition state afterward. The completed window is cleared once, including alpha, before the bezel strips and opaque game image are drawn. These choices allocate no additional history or render targets.

Uniform locations are cached for each linked presentation program and refreshed when graphics resources are recreated. Generated static textures and vector objects retain their source caches. Fixed scenes are prepared offline and include only their required resource closure in the player bundle.

The general Neon SDK retains state preservation and exclusion-area preservation by default. See [the runtime SDK](../modules/neon_background/SDK.md) for its explicit exclusive-pass and whole-output-clear options.
