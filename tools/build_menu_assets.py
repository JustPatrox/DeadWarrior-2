"""Compile the original campaign menu art and authored keyframes for the native renderer."""
import csv
import json
import os
import shutil
from pathlib import Path
from xml.etree import ElementTree

import jpype
import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "work"
OUT = ROOT / "assets/menu"
FLASH = "com.jpexs.decompiler.flash."


def extend_plate(image):
    """Join two new landscape mattes to the unchanged 850 x 700 source plate."""
    center = np.asarray(image.convert("RGB"))

    def join(side, source_edge):
        rgb = np.asarray(Image.open(ROOT/f"art/menu/{side}.png"), dtype=float)
        if side == "left":
            rgb = rgb[:,::-1]
        distance = np.arange(rgb.shape[1])
        correction = (source_edge-rgb[:,0:1]) * np.exp(-distance/13)[None,:,None]
        rgb = np.uint8(np.clip(np.round(rgb+correction),0,255))
        return rgb[:,::-1] if side=="left" else rgb

    return Image.fromarray(np.concatenate([join("left",center[:,:1]),center,
                                           join("right",center[:,-1:])],axis=1))


class ArtCompiler:
    def __init__(self, directory=OUT, *, loadout_overrides=None):
        self.directory = directory
        directory.mkdir(parents=True, exist_ok=True)
        java_home = os.environ.get("DW2_JAVA_HOME")
        java = (Path(java_home)/"bin/server/jvm.dll" if java_home else
                next((WORK / "tools").glob("*/bin/server/jvm.dll")))
        ffdec = Path(os.environ.get("DW2_FFDEC_HOME", WORK / "tools/ffdec"))
        jars = list((ffdec / "lib").glob("*.jar"))
        jpype.startJVM(str(java), "-Xmx3g", "-Djava.awt.headless=true", classpath=jars)
        self.J = jpype.JClass
        self.swf_class = self.J(FLASH + "SWF")
        source = Path(os.environ.get("DW2_SWF", ROOT / "stickwar2.swf"))
        with self.J("java.io.FileInputStream")(str(source)) as stream:
            self.swf = self.swf_class(stream, False)
        self.matrix = self.J(FLASH + "exporters.commonshape.Matrix")
        self.index = json.loads((ROOT/"evidence/inventory.json").read_text())
        self.rows = []
        from loadout_assets import freeze_default_loadouts
        self.loadouts=freeze_default_loadouts(self,loadout_overrides)

    def select_loadout(self, symbol, label):
        frames = self.timeline(symbol).getFrames()
        frames.set(0, self.loadout_variants[symbol][label])
        frames.subList(1, frames.size()).clear()

    def timeline(self, symbol):
        tag = self.swf.getCharacter(symbol)
        if hasattr(tag, "getTimeline"):
            return tag.getTimeline()
        # Isolate an original shape/text in a temporary one-frame display list.
        clip = self.J(FLASH + "tags.DefineSpriteTag")(self.swf)
        place = self.J(FLASH + "tags.PlaceObject2Tag")(self.swf)
        place.placeFlagHasCharacter = True
        place.characterId, place.depth = symbol, 1
        clip.addTag(place)
        clip.addTag(self.J(FLASH + "tags.ShowFrameTag")(self.swf))
        return clip.getTimeline()

    def render(self, symbol, frames=None, scale=1.5, *, child_ticks=None, box=None, frame_offset=0):
        timeline = self.timeline(symbol)
        rect = (timeline.getDisplayRectWithFilters() if box is None else
                self.J(FLASH+"types.RECT")(*(round(coordinate*20) for coordinate in box)))
        frames = list(frames or range(1,timeline.getFrameCount()+1))
        for i,frame in enumerate(frames):
            tick = child_ticks[i] if child_ticks is not None else 0
            index = frame_offset+(i+1 if child_ticks is not None else frame)
            file = f"{symbol}-{index}.png"
            surface = self.swf_class.frameToImageGet(timeline,frame-1,tick,None,0,rect,
                                                    self.matrix(),None,None,scale,True,1)
            self.J("javax.imageio.ImageIO").write(surface.getBufferedImage(),"png",
                                                 self.J("java.io.File")(str(self.directory/file)))
            with Image.open(self.directory/file) as image:
                crop = image.convert("RGBA").getchannel("A").getbbox() or (0,0,1,1)
                trimmed = image.crop(crop)
            trimmed.save(self.directory/file)
            self.rows.append((symbol,index,file,rect.Xmin/20+crop[0]/scale,
                              rect.Ymin/20+crop[1]/scale,trimmed.width/scale,trimmed.height/scale))

    def build(self):
        OUT.mkdir(parents=True, exist_ok=True)
        # Menu equipment has addFrameScript(0, frame1) with stop(). Freeze only
        # these in-memory copies while baking the independently moving fighters.
        for symbol in (747,769,796,814,833,851):
            frames = self.timeline(symbol).getFrames()
            frames.subList(1,frames.size()).clear()
        constants = ElementTree.parse(next((WORK/"extract/binaryData").glob("10208_*")))
        for symbol, field in ((1033,"campaignVersion"),(1038,"credits")):
            tag = self.swf.getCharacter(symbol)
            tag.initialText = constants.findtext(field).strip()
            tag.hasText = True
            tag.html = False
        for symbol in (699,1044,1002,1005,708,1009,1010,1015,1018,1021,1027,
                       1032,1033,1036,1039,1043,987,990,993,1012,1023,998):
            self.render(symbol)
        self.render(979,range(1,98),scale=1)
        plate = extend_plate(Image.open(WORK/"extract/images/976.jpg"))
        plate.save(OUT/"976-wide.png")
        self.rows.append((976,1,"976-wide.png",-198,0,plate.width,plate.height))
        with (OUT/"textures.tsv").open("w", newline="", encoding="utf-8") as out:
            csv.writer(out,delimiter="\t",lineterminator="\n").writerows(self.rows)

        timelines = self.index["timelines"]
        with (OUT/"alpha.tsv").open("w") as out:
            for symbol in (979,982,983):
                alpha = 0
                events = iter(timelines[str(symbol)])
                event = next(events, None)
                for frame in range(1,self.index["symbols"][str(symbol)]["frames"]+1):
                    while event and event["frame"] == frame:
                        if event["op"] == "remove":
                            alpha = 0
                        if "color" in event:
                            alpha = event["color"][3] / 256
                        event = next(events,None)
                    out.write(f"{symbol}\t{frame}\t{alpha}\n")
        with (OUT/"placements.tsv").open("w", newline="") as out:
            write = csv.writer(out,delimiter="\t",lineterminator="\n").writerow
            for owner in (1045,1006,1011,1024,1028,994):
                for row in timelines[str(owner)]:
                    if row["op"] != "place":
                        continue
                    symbol = row["id"]
                    bounds = self.swf.getCharacter(symbol).getRect()
                    a, b, c, d, x, y = row["matrix"]
                    assert b == c == 0
                    write((owner,symbol,row["frame"],row["depth"],row.get("name","-"),
                           a,d,x,y,bounds.Xmin/20,bounds.Ymin/20,bounds.Xmax/20,bounds.Ymax/20))
        for key in ("77_LoginMusic","193_clickButton"):
            sound = next((WORK/"extract/sounds").glob(key+"*.mp3"))
            shutil.copyfile(sound,OUT/(key+".mp3"))


if __name__ == "__main__":
    ArtCompiler().build()
