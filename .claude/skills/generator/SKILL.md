---
name: generator
description: Build a new custom Source (generator) plugin for Resolume Arena as an FFGL plugin, with presets, animation, seamless color palettes and lots of dials, then compile and install it so it appears in Arena's Sources tab. Use when the user asks for a new Resolume generator/source/visual (e.g. "/generator spherical portal").
---

# /generator — make a Resolume Arena Source plugin

The user describes a visual ("spherical portal", "hex grid tunnel", "lissajous ribbons"). You deliver a
working FFGL **Source** plugin that shows up in Resolume Arena's Sources tab within a minute, with the
same level of polish as the reference `Phyllotaxis` plugin: presets, an Animation block, a Color block
with seamless palettes, a Background picker, and plenty of shape dials grouped sensibly.

Do the whole thing yourself: design, code, compile, install, verify in the Arena log, then report.
Only the user can look at the output visually, so tell them exactly what to drag where.

## Where everything lives

| Thing | Path |
|---|---|
| SDK + all plugins | the repo root, i.e. the directory containing this `.claude/` folder (a fork of Resolume's FFGL SDK; all paths below are relative to it and every command runs from it) |
| Shared base class, palettes, GLSL helpers | `source/plugins/_common/GeneratorCommon.h` |
| Minimal skeleton to copy | `source/plugins/Template/` (`FFGLTemplate.h/.cpp`) |
| Full worked example | `source/plugins/Phyllotaxis/` (standalone; predates the base class) |
| Build + install | `build_generator.sh <Name> [--no-install]` |
| Offscreen GLSL check (run by the build) | `tests/shader_compile_test.cpp` |
| Where Arena loads plugins from | `~/Documents/Resolume Arena/Extra Effects/<Name>.bundle` |
| Arena log (proves load / shows errors) | `~/Library/Logs/Resolume Arena/Resolume Arena log.txt` |

Toolchain is only the Command Line Tools `clang++`. No Xcode, no CMake, no Homebrew. Don't install
anything.

## Workflow

1. **Design the dials first** (in your head or a short list), then code. Aim for 15–30 parameters in
   3–5 groups. A generator with three sliders is a failure; the user wants "a lot of dials".
2. **Copy the template**: `cp -R source/plugins/Template source/plugins/<Name>` and rename files,
   class, `PluginInfo` name, and the **4-character plugin ID** (must be unique; `PHYL`, `TMPL` taken).
3. **Write the shader** in the `fragmentBody` string. Keep `GetFragmentShaderSource()` and
   `vertexShaderCode` exactly as in the template; the build harness includes the cpp and calls them.
4. **Build**: `./build_generator.sh <Name>` from the repo root. Step 1 compiles the GLSL offscreen and
   prints numbered source on failure. Fix and rebuild until clean. Warnings inside `source/lib` are the
   SDK's, ignore them.
5. **Verify** in the Arena log: expect `load: Loading plugin ...<Name>` followed by
   `FFGL: Created <Name> generator` (your `LogToHost` line) and `registered extension: '<Name>' uid: <ID>`.
   Arena watches the Extra Effects folder and reloads without a restart. If `shader compile failed`
   appears, the runtime GL differs from the test; read the log line and fix.
6. **Report**: name in the Sources tab, drag into a *fresh* clip slot (old clips keep old instances),
   then a grouped list of the parameters and what each does. Mention what's not possible if relevant.

## Design checklist (what "cool like Phyllotaxis" means)

- **Shape group** (plugin-specific): the geometry dials. Always include `Rotation` (deg) + `Spin`
  (deg/s) for anything radial, `Edge Softness` (px) for anti-aliasing, a size/`Radius` in frame units.
  Add a **Preset** dropdown (option param) whose index 0 is `Custom`; presets set several shape values
  at once and hide the sliders they override via `SetParamVisibility(..., true)`.
- **Animation group**: use `AddAnimationParams()`. It gives Animate / Progress / Speed / Loop / Reset
  and a per-frame `phase` in 0..1. Design the motion so `phase` wrapping 1 → 0 is seamless (use
  `fract`, periodic functions, integer cycle counts). Manual `Progress` doubles as a scrubber so the
  user can drive it from Resolume's BPM sync or envelopes. Add extra motion dials in Shape when they
  are independent (spin, pulse amount, wobble).
- **Color group**: use `AddColorParams()`, colour pixels with `shade(t)` where `t` is a meaningful
  0..1 coordinate (angle, radius, index, depth). Give the user a **Color By** option param when there
  is more than one natural choice. Everything in `shade()` is periodic, so any hue rotation is
  seam-free; never write your own hue formula that can tear.
- **Background group**: `AddBackgroundParams()`. Output premultiplied via `background()` and
  `over()`; alpha 0 gives a transparent layer for compositing in Arena.
- Real units in the UI: always `SetParamRange` (degrees 0–360, counts 1–N, px 0–4). Defaults are the
  member initialisers; pass them explicitly to `SetParamInfo(..., default)`.
- Extra polish that costs little: a `Glow`/`Bloom` falloff, `Line Width`, `Density`, `Warp`/`Twist`,
  `Mirror`/`Symmetry` (int), `Depth`/`Perspective` for 3D-ish looks, `Jitter` seeded by index.

## FFGL / GLSL gotchas (all learned the hard way)

- Parameter names ≤ 16 characters. Group names free.
- Option params: `SetOptionParamInfo(idx, name, count, default)` then `SetParamElementInfo` per item;
  the host sends the element *value* to `SetFloatParameter`.
- Bool = `FF_TYPE_BOOLEAN` (0/1 float), Event = `FF_TYPE_EVENT` (fires with value ≠ 0),
  Int = `FF_TYPE_INTEGER`. Colour pickers = four consecutive params typed HUE/SATURATION/BRIGHTNESS/ALPHA;
  Resolume shows them as one picker named after the hue param, and visibility is toggled via the hue param only.
- Call `UpdateCommonVisibility(false)` / `UpdateVisibility(false)` at the end of the constructor;
  pass `true` when reacting to a change at runtime or Resolume won't refresh.
- GLSL is `#version 410 core`. Reserved words that bite: `half`, `sample`, `filter`, `input`, `output`,
  `common`, `active`, `partition`. No `RENDERSIZE`/`TIME`; use `resolution` uniform and `phase`/`time`.
- Frame coords: `p = (uv - 0.5) * resolution / min(resolution.x, resolution.y)` puts the shorter side
  at −0.5..0.5, aspect-correct. One pixel = `1.0 / min(res.x, res.y)`.
- Per-pixel loops must have a constant upper bound and `break`; cap at ~1500 iterations. Prefer inverse
  lookups (compute which few elements can touch this pixel) over brute force when N can be large.
- Animation timing comes from `Tick()` (wall clock), not the host's `SetTime`, so it never stalls.
- Rebuilding overwrites the bundle while Arena has it loaded; Arena reloads it. Existing clips keep
  the old instance, so tell the user to drag a fresh one.
- Never restart Arena for the user (unsaved compositions). Ask them to if a restart is truly needed.

## Reporting style

Lead with "built and loaded" or the failure. Then: how to find it, a grouped parameter list with one
short line each, what the presets are, and anything omitted. Keep code out of the message.
