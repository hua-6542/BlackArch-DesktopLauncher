# Bundled Assets

These assets are shipped with the project so anyone cloning the repo can run
the launcher without sourcing them separately. The launcher reads them from
`~/.cache/blackarch-launcher/` at runtime — see `--install` in the main README
for how to copy these into place.

## Contents

| Path | Purpose |
|------|---------|
| `backgrounds/*-full.png` | Original wallpapers used by the rolling background carousel |
| `backgrounds_compose/*-full.png` | Pre-darkened/blurred glass-friendly variants (preferred at runtime) |
| `gif_corner/frame-{0..4}.png` | 5-frame ping-pong sprite for the Rick & Morty corner animation |
| `icons/blackarch-tree-rick.png` | High-res Rick avatar |
| `icons/blackarch-tree-rick-64.png` | 64×64 Rick avatar (window icon) |
| `icons/blackarch-tree.svg` | Vector launcher icon |
| `morty-bg.png` | Reference Rick & Morty background tile |
| `morty-corner.png` | Reference Morty corner figure |

## License & attribution

Wallpapers and Rick & Morty imagery are derived from publicly available fan art
and screenshots. They are bundled here for personal/educational use; if you
redistribute or repackage the launcher, replace them with assets you have
explicit rights to use.
