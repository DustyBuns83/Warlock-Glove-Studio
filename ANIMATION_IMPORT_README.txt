Warlock Glove Studio — Animation Import v2

Import Animation now supports deeper Fire profiles in addition to the four visible sliders.
The included Rolling_Fire_v1.json changes the simulated fire algorithm: palette, base depth motion,
flame ignition/height distribution, brightness jitter, and intermittent dim periods.

This is a SIMULATOR profile. Correct_V4_MASTER.ino remains the known-working firmware reference.
When the visual effect is approved, the corresponding fire logic should be ported deliberately to
Correct V4 rather than assuming the JSON itself programs the glove.

UI note: Play/Pause and Cast are now floating inside the lower-right corner of the 3D glove viewer. The Off control was removed.
