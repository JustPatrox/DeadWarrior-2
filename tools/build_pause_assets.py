"""Compile the original campaign pause panel and independent button states."""
from build_menu_assets import ArtCompiler, ROOT
from build_archidon_assets import table
import jpype

c = ArtCompiler(ROOT / "assets/pause")
# CampaignPauseMenu hides confirmation and updates buttons/toggles separately.
for depth in (5, 7, 10, 24):
    c.timeline(9964).getFrame(0).layers.remove(jpype.JInt(depth))
c.render(9964, [1], scale=1)
c.render(9951, [1], scale=1)
for symbol in (9915, 9919, 9957, 9958, 9939, 9946):
    c.render(symbol, [1, 2, 3], scale=1.5)
c.render(9928, [1, 3], scale=1.5)
for depth in (3, 5):
    c.timeline(9963).getFrame(0).layers.remove(jpype.JInt(depth))
c.render(9963, [1], scale=1)
table(c.directory / "textures.tsv", c.rows)
