"""Relabel the original quality button with its original embedded Copperplate font."""
from build_menu_assets import ArtCompiler,ROOT,FLASH
from build_archidon_assets import table


def build():
    c=ArtCompiler(ROOT/'assets/pc-button')
    field=c.swf.getCharacter(1102)
    field.fontHeight=160
    field.align=2
    field.wordWrap=False
    field.bounds=c.J(FLASH+'types.RECT')(-400,400,-160,160)
    field.initialText='<p align="center"><font face="CopprplGoth Bd BT" size="8" color="#d7d3b1" letterSpacing="0.000000" kerning="1">PC</font></p>'
    # Keep the quality button's authored up/over/down graphics and replace its
    # bitmap label with the existing embedded Copperplate text field.
    label=c.timeline(9807).getFrame(0).layers
    original=label.get(c.J('java.lang.Integer')(1))
    original.characterId=1102
    original.matrix.translateX=0
    original.matrix.translateY=35
    c.render(10102,[1,2,3,4],scale=1,frame_offset=60000)
    # Rebuild the same native HUD plate while removing the original HIGH label.
    depths={1,2,10,17,18,20,22,24,27,30,33,36,39,42,45,48,73,75,77,132,145}
    hud=c.timeline(10118).getFrame(0).layers
    for depth in list(hud.keySet()):
        if int(depth) not in depths:hud.remove(depth)
    c.render(10118,[1],scale=1.5,frame_offset=10000)
    table(c.directory/'textures.tsv',c.rows)


if __name__=='__main__':build()
