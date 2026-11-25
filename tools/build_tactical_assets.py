"""Export original Spearton Block and center-tower spell/ghost presentation."""
from xml.etree import ElementTree as ET
from pathlib import Path
import csv
from PIL import Image
from build_menu_assets import ArtCompiler, ROOT, WORK, FLASH
from build_archidon_assets import table


def build():
    c=ArtCompiler(ROOT/'assets/tactical')
    data=ET.parse(next((WORK/'extract/binaryData').glob('10208_*')))
    rules=[('spearton.shieldWall.damageReduction',data.findtext('Order/Units/spearton/shieldWall/damageReduction')),
           ('towerSpawnDelay',data.findtext('towerSpawnDelay')),
           # Original Hill.init readback at midpoint perspective .65, including
           # Flash's twip-quantized height and child y assignment (both sizes).
           ('hill.barY',-206.05)]
    c.render(6208,scale=2)
    animations=[(6456,'block',6208,6.9,-103.55,c.timeline(6208).getFrameCount(),0)]
    c.render(8806,[1],scale=1)
    for symbol,child in [(3732,3731),(4027,4026)]:
        length=c.timeline(child).getFrameCount()
        c.render(symbol,[1]*length,child_ticks=range(length),scale=1.5)
        rules.append((f'effect.{symbol}.frames',length))
    # Unit.updateCommon applies a cyan GlowFilter to mc.mc and blue +255 on
    # mc. Its later poison branch resets green to zero during a normal tick.
    # Keep these authored effects out of the core
    # renderer by baking the source display hierarchy into separate frames.
    clip=c.J(FLASH+'tags.DefineSpriteTag')(c.swf)
    place=c.J(FLASH+'tags.PlaceObject2Tag')(c.swf)
    place.placeFlagHasCharacter=True;place.characterId=6124;place.depth=1
    clip.addTag(place);clip.addTag(c.J(FLASH+'tags.ShowFrameTag')(c.swf))
    timeline=clip.getTimeline()
    state=next(iter(timeline.getFrame(0).layers.values()))
    glow=c.J(FLASH+'types.filters.GLOWFILTER')()
    glow.glowColor=c.J(FLASH+'types.RGBA')(0,255,255,255)
    glow.blurX=36;glow.blurY=18;glow.strength=2.64
    glow.innerGlow=False;glow.knockout=False;glow.compositeSource=True;glow.passes=1
    state.filters=c.J('java.util.ArrayList')();state.filters.add(glow)
    color=c.J(FLASH+'types.CXFORMWITHALPHA')()
    color.hasAddTerms=True;color.blueAddTerm=255
    state.colorTransForm=color
    # Use the original body clips with their existing registration points.
    unique={}
    for folder,unit in [('tutorial',6456),('giants',7526),('bomber',7526)]:
        for row in csv.reader((ROOT/f'assets/{folder}/animations.tsv').open(),delimiter='\t'):
            if int(row[0])==unit:unique[int(row[2])]=int(row[5])
    for symbol,length in sorted(unique.items()):
        state.characterId=symbol
        # Dynamic child time is used because the wrapper itself is one frame.
        raw=c.swf.getCharacter(symbol).getRect()
        rect=c.J(FLASH+'types.RECT')(raw.Xmin-800,raw.Xmax+800,raw.Ymin-500,raw.Ymax+500)
        density=3 if symbol!=6124 and symbol!=6144 and symbol!=6145 and symbol!=6193 else 2
        for tick in range(length):
            surface=c.swf_class.frameToImageGet(timeline,0,tick,None,0,rect,c.matrix(),None,None,density,True,1)
            name=f'{symbol}-{50001+tick}.png';path=c.directory/name
            c.J('javax.imageio.ImageIO').write(surface.getBufferedImage(),'png',c.J('java.io.File')(str(path)))
            with Image.open(path) as source:
                image=source.convert('RGBA');crop=image.getchannel('A').getbbox()
                if crop is None:raise ValueError(('Empty ghost frame',symbol,tick))
                image.crop(crop).save(path)
            c.rows.append((symbol,50001+tick,name,rect.Xmin/20+crop[0]/density,rect.Ymin/20+crop[1]/density,
                           (crop[2]-crop[0])/density,(crop[3]-crop[1])/density))
    table(c.directory/'textures.tsv',c.rows)
    table(c.directory/'animations.tsv',animations)
    table(c.directory/'rules.tsv',rules)
    print('Original tactical frames:',len(c.rows),flush=True)


if __name__=='__main__':build()
