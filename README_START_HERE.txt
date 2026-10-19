WARLOCK GLOVE STUDIO — ARCANE UI + CODE GENERATOR

Open a Terminal in this folder and run:
  python3 -m http.server 8000
Open http://localhost:8000 in Safari.

Click an effect icon to preview it. Adjust settings, then open Code Generator and click Generate Arduino Code. Review and download the .ino sketch. The code is based on your original uploaded ThreeModeV1.ino with parameter changes and extra modes. Do not flash without checking pins, wiring, library compatibility, and behavior. The color picker affects the charge-only generated mode; rainbow uses its full spectrum, and organic fire retains its thermal palette. Individual LED overrides are not yet exported. The display font loads online when available, with a local serif fallback.
