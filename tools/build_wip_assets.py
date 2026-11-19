"""Reuse the original menu button's three authored states with a native label."""
from build_menu_assets import ArtCompiler,ROOT
from build_archidon_assets import table
import jpype

c=ArtCompiler(ROOT/"assets/wip")
for frame in c.timeline(1002).getFrames():
    for depth in list(frame.layers.keySet()):
        if int(frame.layers.get(depth).characterId) in (999,1000,1001):
            frame.layers.remove(jpype.JInt(depth))
c.render(1002,[1,2,3],scale=1.5,frame_offset=10000)
table(c.directory/"textures.tsv",c.rows)
