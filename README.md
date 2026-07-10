# SOL Spout Layout

Windows-only Geode 5.7.1 mod for Geometry Dash 2.2081.

- The game window renders a semantic Layout pass.
- OBS receives the original level through the Spout sender **Geometry Dash - Original Level**.
- Pause UI, menus, editor chrome and indicators are not individually hidden; layout filtering is scoped globally to `GameObject::visit`, so non-world overlays remain intact.
- The original compressed `m_levelString` is cached before level initialization.
- Capture uses a GPU FBO and Spout `SendTexture`; there is no `glReadPixels` or CPU frame copy.
- Capture FPS is independently limited (default 60) so high-refresh gameplay does not duplicate every frame.

## Build

```powershell
git clone https://github.com/gigasteeven/Sol-Mod
cd Sol-Mod
cmake -B build -A x64
cmake --build build --config Release
```

The GitHub Actions workflow builds and uploads the `.geode` automatically.

## OBS

Install the OBS Spout2 source plugin, add a Spout2 Capture source, and select `Geometry Dash - Original Level`. Spout2 itself is statically linked into the mod; no Spout SDK or runtime DLL is required by the user.

## Performance

The original scene is rendered at the configured capture rate and the layout scene at game FPS. This is GPU-only but not free: shader-heavy levels still require a second world draw on capture frames. Keep Geometry Dash and OBS on the RTX 3090 adapter and use 60 capture FPS for 240+ FPS gameplay.

## Third-party notice

Spout2 is fetched from `leadedge/Spout2` and statically linked. Its license remains in the submodule. XDBot source is not copied because that repository provides no redistribution license; the layout filtering here is independently implemented from gameplay semantics.
