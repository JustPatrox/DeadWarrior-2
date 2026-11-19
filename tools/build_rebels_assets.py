"""Compile the united Order armies' original terrain and mission data."""
from xml.etree import ElementTree as ET
import jpype
import shutil
from PIL import Image
from build_menu_assets import ArtCompiler, ROOT, WORK
from build_archidon_assets import extend, table
from build_campaign_assets import set_field


def build():
    c=ArtCompiler(ROOT/"assets/rebels")
    level=ET.parse(next((WORK/"extract/binaryData").glob("29_*"))).findall("level")[4]
    rules=[]
    for symbol in (4058,4056,4062,4067):
        bounds=c.swf.getCharacter(symbol).getRect()
        rules.append((f"layer.{symbol}.width",(bounds.Xmax-bounds.Xmin)/20))
        extend(c,symbol,ground=symbol==4067)
    rules.append(("map.length",int(dict(rules)["layer.4067.width"])))
    for side in ("player","oponent"):
        rules.extend(((side+".statueHealth",level.find(side).get("statueHealth")),
                      (side+".gold",level.findtext(side+"/gold","500")),
                      (side+".mana",level.findtext(side+"/mana","0"))))
    rules.extend((("normalHealthScale",level.findtext("normalHealthScale")),
                  ("normalDamageScale",level.findtext("normalDamageScale")),
                  ("enemy.handicap",level.findtext("insane")),("enemy.castleArchers",0)))
    for name in ("Swordwrath","Spearton","Archidon","Ninja","Magikill","Monk"):
        value=level.findtext("oponent/UnitComposition/"+name)
        if value is None:
            campaign=ET.parse(next((WORK/"extract/binaryData").glob("29_*")))
            value=campaign.findtext("Order/UnitComposition/"+name)
        rules.append(("composition."+name,value))
    for symbol in (10045,10048):
        c.render(symbol,[1],scale=1)
    for symbol in (8783,8787,8789,8791,8801):
        source=next((WORK/"extract/images").glob(f"{symbol}_*.png"))
        shutil.copyfile(source,c.directory/f"{symbol}-1.png")
        with Image.open(source) as image:
            c.rows.append((symbol,1,f"{symbol}-1.png",-image.width/2,-image.height/2,image.width,image.height))
    set_field(c,293,level.findtext("oponent/raceName"))
    set_field(c,292,"Order")
    set_field(c,337,level.findtext("tip"))
    for symbol,depths in ((343,(2,3,19,22,25,30,31,32,34,36,40,41,57,68)),(338,(4,))):
        for depth in depths: c.timeline(symbol).getFrame(0).layers.remove(jpype.JInt(depth))
        c.render(symbol,[1],scale=1,frame_offset=4)
    table(c.directory/"textures.tsv",c.rows)
    table(c.directory/"rules.tsv",rules)
    table(c.directory/"starting.tsv",((side,n.text) for side in ("player","oponent") for n in level.findall(side+"/startingUnit")))


if __name__=="__main__":
    build()
