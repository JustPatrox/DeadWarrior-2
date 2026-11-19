"""Compile the second campaign mission from the existing SWF extraction."""
import csv
from xml.etree import ElementTree as ET

import jpype
import numpy as np
from PIL import Image

from build_menu_assets import ArtCompiler, ROOT, WORK
from build_campaign_assets import set_field


def table(path,rows):
    with path.open("w",encoding="utf-8",newline="") as out:
        csv.writer(out,delimiter="\t",lineterminator="\n").writerows(rows)


def extend(c,symbol,ground=False):
    c.render(symbol,[1],scale=1)
    row=c.rows[-1]
    with Image.open(c.directory/row[2]) as source:
        core=source.convert("RGBA")
    if ground:
        # Terrain below the authored HUD can be empty even when its bounds
        # reach 700. Continue the last opaque soil above that hidden area.
        pixels=np.array(core)
        cutoff=min(core.height,round(616-row[4]))
        opaque=np.where(np.all(pixels[:cutoff,2:-2,3]>=254,axis=1))[0]
        if not len(opaque): raise ValueError(f"No solid terrain row in {symbol}")
        bottom=int(opaque[-1])+1
        height=round(700-row[4])
        pixels=np.pad(pixels[:bottom],((0,max(0,height-bottom)),(0,0),(0,0)),mode="reflect")
        core=Image.fromarray(pixels)
        core.save(c.directory/row[2])
        row=(*row[:6],core.height)
        c.rows[-1]=row
    # Continue the layer at its endpoints. Its original frame and width still
    # determine both the terrain bounds and the parallax.
    for frame,left in ((2,True),(3,False)):
        edge=core.crop((1,0,200,core.height)) if left else core.crop((core.width-200,0,core.width-1,core.height))
        edge=edge.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        name=f"{symbol}-{frame}.png"
        edge.save(c.directory/name)
        x=row[3]-198 if left else row[3]+row[5]-1
        c.rows.append((symbol,frame,name,x,row[4],199,row[6]))


def build():
    c=ArtCompiler(ROOT/"assets/archidon")
    data=ET.parse(next((WORK/"extract/binaryData").glob("10208_*")))
    campaign=ET.parse(next((WORK/"extract/binaryData").glob("29_*")))
    level=campaign.findall("level")[1]
    rules=[]
    for symbol in (4119,4117,4114,4124):
        bounds=c.swf.getCharacter(symbol).getRect()
        rules.append((f"layer.{symbol}.width",(bounds.Xmax-bounds.Xmin)/20))
        extend(c,symbol,ground=symbol==4124)
    rules.append(("map.length",int(dict(rules)["layer.4124.width"])))
    rules.extend((("player.statueHealth",int(level.find("player").get("statueHealth"))),
                  ("oponent.statueHealth",int(level.find("oponent").get("statueHealth"))),
                  ("enemy.handicap",float(level.findtext("insane"))),
                  ("normalHealthScale",float(level.findtext("normalHealthScale")))))
    for who in ("player","oponent"):
        rules.extend(((f"{who}.gold",int(level.findtext(who+"/gold","500"))),
                      (f"{who}.mana",int(level.findtext(who+"/mana","0")))))
    for name in ("goldCollectionRate","frequency"):
        rules.append((f"hill.{name}",data.findtext("hill/"+name)))
    rules.append(("towerCaptureTime",data.findtext("towerCaptureTime")))
    for field in ("effect","cooldown","healthLoss","rageMaxVelocity"):
        rules.append(("rage."+field,data.findtext("Order/Units/swordwrath/rage/"+field)))
    for unit in ("Miner","Swordwrath","Archidon"):
        rules.append((f"composition.{unit}",level.findtext("oponent/UnitComposition/"+unit,
                      campaign.findtext("Order/UnitComposition/"+unit))))
    # Default equipment in campaign, independent of the basic castle helmet.
    for symbol in (2004,2149):
        frames=c.timeline(symbol).getFrames()
        frames.subList(1,frames.size()).clear()
    poses=[]
    bows=[]
    for label,symbol,origin,base in (
        ("stand",4176,(-70.5,-85.8),1000),("run",4221,(18.75,-102.2),0),
        ("death_1",4232,(-70.35,-85.8),0)):
        for number,frame in enumerate(c.timeline(symbol).getFrames(),1):
            for depth in frame.layers.keySet():
                state=frame.layers.get(depth)
                if int(state.characterId)==4168:
                    matrix=state.matrix
                    bows.append((symbol,number+base,matrix.translateX/20,matrix.translateY/20))
        if label!="death_1":
            for frame in c.timeline(symbol).getFrames():
                for depth in list(frame.layers.keySet()):
                    if int(frame.layers.get(depth).characterId)==4168: frame.layers.remove(depth)
        c.render(symbol,scale=1,frame_offset=base)
        poses.append((4236,label,symbol,*origin,c.timeline(symbol).getFrameCount(),base))
    c.render(1904,[6],scale=1)
    c.timeline(1904).getFrames().set(0,c.timeline(1904).getFrames().get(5))
    frames=c.timeline(1904).getFrames()
    frames.subList(1,frames.size()).clear()
    c.render(1922,scale=.8,frame_offset=178)
    for symbol in (10042,10051,8806,8788):
        c.render(symbol,[1],scale=1)
    for name in ("profileArchidon","profileSpearton"):
        symbol=int(next(key for key,names in c.index["linkage"].items() if name in names))
        frame=c.timeline(symbol).getFrameCount() if name=="wallSpike" else 1
        c.render(symbol,[frame],scale=1)
    c.timeline(1162).getFrame(0).layers.remove(jpype.JInt(9))
    c.render(1162,[1],scale=1)
    c.render(1158,scale=1)
    rules.append(("hill.flameFrames",c.timeline(1158).getFrameCount()))
    wall_frames=c.timeline(1174).getFrameCount()
    c.render(1174,[wall_frames],scale=1)
    rules.append(("wall.frame",wall_frames))
    rules.append(("wall.health",data.findtext("Order/Units/wall/health")))
    # Distinct result frames keep the first mission's cached textures valid.
    for symbol,value in ((292,"Order"),(293,"Archidons"),(326,data.findtext("Order/Units/archer/info")),
                         (334,data.findtext("Order/Units/archer/name")),(337,level.findtext("tip"))):
        set_field(c,symbol,value)
    for owner,depths in ((343,(2,3,19,22,25,30,31,32,34,36,40,41,57,68)),(335,(4,)),(338,(4,))):
        for depth in depths: c.timeline(owner).getFrame(0).layers.remove(jpype.JInt(depth))
        c.render(owner,[1],scale=1,frame_offset=1)
    c.render(1509,[1],scale=1)
    table(c.directory/"textures.tsv",c.rows)
    table(c.directory/"rules.tsv",rules)
    table(c.directory/"animations.tsv",poses)
    table(c.directory/"bows.tsv",bows)
    table(c.directory/"starting.tsv",((who,node.text) for who in ("player","oponent") for node in level.findall(who+"/startingUnit")))


if __name__=="__main__":
    build()
