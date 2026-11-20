"""Prepare native art for the first campaign map and Swamp Lands tutorial."""
import argparse
import csv
from html import escape
import re
import shutil
import subprocess
import wave
from xml.etree import ElementTree

import jpype
import numpy as np
from PIL import Image
from build_menu_assets import ArtCompiler, ROOT, WORK

DEFENCE_ICONS=(8804,10123,10126,10130)


def outcome_art(c,level,data):
    # Team.updateStatue pins the nested timeline to the team's skin even while
    # UserInterface advances the 89-frame masked collapse of the parent.
    skins=c.timeline(1904).getFrames()
    enemy=skins.get(9)
    skins.subList(1,skins.size()).clear()
    count=c.timeline(1922).getFrameCount()
    c.render(1922,scale=.8)
    skins.set(0,enemy)
    c.render(1922,scale=.8,frame_offset=count)

    def omit(owner,depths):
        layers=c.timeline(owner).getFrame(0).layers
        for depth in depths:
            layers.remove(jpype.JInt(depth))
    # Dynamic graph, timer, modal panels and buttons retain individual origins.
    omit(343,(2,3,19,22,25,30,31,32,34,36,40,41,57,68))
    set_field(c,292,"Order")
    set_field(c,293,level.findtext("oponent/raceName"))
    omit(301,(2,))
    set_field(c,326,data.findtext("Order/Units/spearton/info"))
    set_field(c,334,data.findtext("Order/Units/spearton/name"))
    set_field(c,337,level.findtext("tip"))
    omit(335,(4,))
    omit(338,(4,))
    for symbol in (286,301,320,335,338,343,1366):
        c.render(symbol,[1],scale=1)
        if symbol==286:
            # Keep all four graph-frame corners and the central ornament fixed.
            # The flanks use the empty earth inside that same illustration.
            path=c.directory/c.rows[-1][2]
            with Image.open(path) as original:
                core=np.array(original.convert("RGB"),dtype=float)
            earth=core[337:505,270:468]
            band=np.concatenate((earth,earth[::-1],earth,earth[::-1],earth),axis=0)[:700]
            def flank(edge,texture):
                fade=np.exp(-np.arange(198)/22)[None,:,None]
                return texture+(edge-texture[:,0:1])*fade
            wide=np.concatenate((flank(core[:,0:1],band)[:,::-1],core,
                                 flank(core[:,-1:],band[:,::-1])),axis=1)
            Image.fromarray(np.clip(wide,0,255).astype("uint8")).save(path)
            c.rows[-1]=(*c.rows[-1][:3],-198,0,wide.shape[1],wide.shape[0])
    c.render(312,[3,4],scale=1)
    for name in ("StatueDestroyed","OrderVictory","OrderDefeat","UnitUnlock"):
        src=next((WORK/"extract/sounds").glob(f"*_{name}_*.mp3"))
        shutil.copyfile(src,c.directory/(name+".mp3"))


def defence_art(c):
    # CastleArchers.addUnit selects the Basic Helmet on Archer.head and the
    # Default bag. The bow is an independent display object rotated in Archer.aim.
    c.select_loadout(2004,"Basic Helmet")
    for symbol,frame in ((2149,1),(4164,1)):
        frames=c.timeline(symbol).getFrames()
        frames.set(0,frames.get(frame-1))
        frames.subList(1,frames.size()).clear()
    body=c.timeline(4176)
    for frame in body.getFrames():
        frame.layers.remove(jpype.JInt(66))
    c.render(4176,scale=1)
    c.render(4168,scale=1)
    c.render(3988,[1,3],scale=1)
    # Input uses Flash hit states for the global orders and the original
    # building hit sprite. Record spans at their authored origin in the HUD/base.
    maskdir=WORK/"compiled/input-masks"
    maskdir.mkdir(parents=True,exist_ok=True)
    original=c.directory
    out=original/"control-masks.tsv"
    with out.open("w",newline="") as f:
        writer=csv.writer(f,delimiter="\t",lineterminator="\n")
        c.directory=maskdir
        try:
            for owner,field in ((10118,"garrisonButton"),(10118,"defendButton"),(10118,"attackButton"),(1866,"fletcherHitArea")):
                e=next(x for x in c.index["timelines"][str(owner)] if x.get("name")==field)
                symbol=e["id"]
                state=1 if owner==1866 else 4
                c.render(symbol,[state],scale=1)
                _,_,name,x,y,_,_=c.rows.pop()
                with Image.open(maskdir/name) as im:
                    mask=im.convert("RGBA").getchannel("A")
                x+=e["matrix"][4]; y+=e["matrix"][5]
                for row in range(mask.height):
                    pixels=list(mask.crop((0,row,mask.width,row+1)).tobytes())+[0]
                    start=None
                    for col,alpha in enumerate(pixels):
                        if alpha and start is None: start=col
                        if not alpha and start is not None:
                            writer.writerow((field,y+row,x+start,x+col))
                            start=None
        finally:
            c.directory=original


def set_field(compiler,symbol,value):
    field = compiler.swf.getCharacter(symbol)
    field.initialText = re.sub(r"(?<=>)[^<>]+(?=</font>)",
                               lambda _:escape(value), str(field.initialText), count=1)
    field.hasText = True


def positions(compiler,owners):
    with (compiler.directory/"placements.tsv").open("w",newline="") as f:
        write=csv.writer(f,delimiter="\t",lineterminator="\n").writerow
        for key in owners:
            for event in compiler.index["timelines"][str(key)]:
                if event["frame"]!=1 or event["op"]!="place":
                    continue
                symbol=event["id"]
                a,b,c,d,x,y=event["matrix"]
                if b or c:
                    raise ValueError(f"Rotated placement {key}/{symbol} needs affine rendering")
                bounds=compiler.swf.getCharacter(symbol).getRect()
                write((key,symbol,1,event["depth"],event.get("name","-"),a,d,x,y,
                       bounds.Xmin/20,bounds.Ymin/20,bounds.Xmax/20,bounds.Ymax/20))


def map_art(compiler,level):
    # CampaignScreen.enter() stops campaignMap at frame 1. Its selected flag
    # has an independently looping child. Keep it separate for input and animation.
    frame=compiler.timeline(436).getFrame(0)
    frame.layers.remove(jpype.JInt(2297))
    compiler.render(436,[1],scale=1,box=(-130,902,22,700))
    # Continue the chart's ocean marks out to the right of the authored map.
    # Use the ocean at its eastern edge; no flag or text is in this band.
    with Image.open(compiler.directory/"436-1.png") as original:
        base=original.convert("RGBA")
    x0=round(compiler.rows[-1][3])
    ocean_start=892-x0
    core=base.crop((0,0,ocean_start,base.height))
    sea=core.crop((core.width-180,0,core.width,core.height)).transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    chart=Image.new("RGBA",(core.width+sea.width,core.height))
    chart.paste(core)
    chart.paste(sea,(core.width,0))
    chart.save(compiler.directory/"436-1.png")
    compiler.rows[-1]=(*compiler.rows[-1][:5],chart.width,chart.height)
    compiler.render(393,[1]*15,child_ticks=range(15),scale=1)
    for symbol,value in [(486,level.get("story")),(488,level.get("title")),
                         (491,"Game saved at "+level.get("title"))]:
        set_field(compiler,symbol,value)
    for symbol in (439,442,454,463,472,476,485,486,488,490,491,497):
        compiler.render(symbol)
    positions(compiler,(499,478,477,498))


def tutorial_art(c,level,refresh_ui=False):
    import imageio_ffmpeg
    data=ElementTree.parse(next((WORK/"extract/binaryData").glob("10208_*")))
    timelines=c.index["timelines"]
    # The market loadout of the initial campaign is Default. Otherwise FFDec
    # cycles through every weapon in the library as if it were an animation.
    for symbol in (2262,2035,2126,1983,2030,2115):
        frames=c.timeline(symbol).getFrames()
        frames.subList(1,frames.size()).clear()
    if refresh_ui:
        with (c.directory/"textures.tsv").open() as f:
            for row in csv.reader(f,delimiter="\t"):
                if int(row[0]) not in (10118,9726,10139,10138,4098,1621,1872,1866,1904,1928,
                                        1244,1447,2311,6457,6458,8780,8808,10039,1595,
                                        4176,4168,3988,10036,1874,10094,1922,286,301,312,320,335,338,343,1366,*DEFENCE_ICONS):
                    c.rows.append((int(row[0]),int(row[1]),row[2],*map(float,row[3:])))
    else:
        for symbol in (4102,1051,1078):
            c.render(symbol,[1],scale=1)
        c.render(1924,scale=1)
    c.render(4098,[1],scale=1)
    # The old HUD concealed the bottom 84 pixels of the stage. Continue only
    # the soil texture below the battlefield at the two newly visible flanks.
    file=c.directory/"4098-1.png"
    with Image.open(file) as image:
        soil=image.convert("RGBA")
    # The final antialiased boundary is nearly transparent (alpha 0..4).
    # Join the extension to the last interior texel instead.
    soil=soil.crop((0,0,soil.width,soil.height-1))
    origin_y=c.rows[-1][4]
    missing=round(700-origin_y-soil.height)
    extended=Image.new("RGBA",(soil.width,soil.height+missing))
    extended.paste(soil)
    extended.paste(soil.crop((0,soil.height-missing,soil.width,soil.height)).transpose(
        Image.Transpose.FLIP_TOP_BOTTOM),(0,soil.height))
    extended.save(file)
    c.rows[-1]=(*c.rows[-1][:6],extended.height)
    for symbol in (1621,1872,1866,1928,1244,1447,10036,10039,1874,1595):
        c.render(symbol,[1],scale=1)
    # FogOfWar.setTint(mc,0,.9) multiplies all RGB channels by .1.
    with Image.open(c.directory/"1595-1.png") as source:
        fog=source.convert("RGBA")
    rgb=fog.convert("RGB").point(lambda v: round(v*.1))
    rgb.putalpha(fog.getchannel("A"))
    rgb.save(c.directory/"1595-1.png")
    defence_art(c)
    for symbol in (2311,6457,6458,8780,8808,*DEFENCE_ICONS):
        file=next((WORK/"extract/images").glob(f"{symbol}_*.png"))
        name=f"{symbol}-1.png"
        shutil.copyfile(file,c.directory/name)
        with Image.open(file) as icon:
            c.rows.append((symbol,1,name,-icon.width/2,-icon.height/2,icon.width,icon.height))
    c.render(1904,[1,10],scale=1)
    c.render(10094,[1,2],scale=1)
    outcome_art(c,level,data)
    animations=[]
    for owner,labels in [(4922,{"stand","stand_breath","run","attack_1","attack_2","death_1","death_2","death_3"}),
                         (3104,{"stand","run","mine","bendDownToPray","pray","death_1","attack_1"}),
                         (6456,{"stand","run","attack_1","death_1"})]:
        label=""
        for event in timelines[str(owner)]:
            if event["op"]=="label":
                label=event["name"]
            if label not in labels or event.get("name")!="mc" or "id" not in event:
                continue
            child=event["id"]
            if not refresh_ui or not any(row[0]==child for row in c.rows):
                c.render(child,scale=1)
            a,b,cc,d,x,y=event["matrix"]
            assert (a,b,cc,d)==(1,0,0,1)
            animations.append((owner,label,child,x,y,c.timeline(child).getFrameCount()))
            labels.remove(label)
    with (c.directory/"animations.tsv").open("w",newline="") as f:
        csv.writer(f,delimiter="\t",lineterminator="\n").writerows(animations)

    # Keep the authored order HUD structure. Buttons and counters whose state
    # comes from AS3 are separate from the static empty training/action slots.
    static_depths={1,2,10,17,18,20,22,24,27,30,33,36,39,42,45,48,73,75,77,103,132,145}
    base=c.timeline(10118).getFrame(0)
    for depth in list(base.layers.keySet()):
        if int(depth) not in static_depths:
            base.layers.remove(depth)
    c.render(10118,[1],scale=1.5)
    for name in ("gold","population","mana"):
        event=next(e for e in timelines["9726"] if e.get("name")==name)
        c.timeline(9726).getFrame(0).layers.remove(jpype.JInt(event["depth"]))
    c.render(9726,[1])
    for depth in (10,11,12):
        c.timeline(10139).getFrame(0).layers.remove(jpype.JInt(depth))
    c.render(10139,[1])
    c.render(10138)
    with (c.directory/"markers.tsv").open("w",newline="") as f:
        writer=csv.writer(f,delimiter="\t",lineterminator="\n")
        for child in (4422,4449,6144,3087):
            events=iter(timelines[str(child)])
            event=next(events,None)
            layers={}
            for frame in range(1,c.timeline(child).getFrameCount()+1):
                while event and event["frame"]==frame:
                    depth=event.get("depth")
                    if event["op"]=="remove":
                        layers.pop(depth,None)
                    elif event["op"] in ("place","move"):
                        item={} if "id" in event else layers.get(depth,{})
                        item.update(event)
                        layers[depth]=item
                    event=next(events,None)
                for item in layers.values():
                    if item.get("name") in ("tip","swing"):
                        writer.writerow((child,frame,item["name"],*item["matrix"][4:]))
    positions(c,(9726,))

    script=(WORK/"extract/scripts/com/brockw/stickwar/campaign/controllers/CampaignTutorial.as").read_text()
    messages=re.findall(r'this\.message\.setMessage\("([^"]*)","([^"]*)",[^,]+,"(voiceTutorial\d+)"',script)
    sounds={}
    with (c.directory/"messages.tsv").open("w",encoding="utf-8",newline="") as f:
        writer=csv.writer(f,delimiter="\t",lineterminator="\n")
        for message,step,voice in messages:
            if voice not in sounds:
                src=next((WORK/"extract/sounds").glob(f"*_{voice}_*"))
                output=c.directory/(voice+".wav")
                subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(),"-v","error","-y","-i",src,
                                "-c:a","pcm_s16le",output],check=True)
                with wave.open(str(output)) as wav:
                    sounds[voice]=wav.getnframes()/wav.getframerate()
            writer.writerow((voice,sounds[voice],step,message.replace("\\'","'")))
    with (c.directory/"rules.tsv").open("w") as f:
        f.write(f"campaign.points\t{level.get('points')}\n")
        for name in ("frontScale","backScale","battlefieldHeight","battlefieldY","goldAtStartOfMap",
                     "screenScrollSpeed","screenScrollGain","visionSize","maxWorshipers","gravity","populationLimit",
                     "garrisonHealRate","passiveIncome","passiveMana","passiveIncomeUpgraded1","passiveIncomeUpgraded2",
                     "passiveIncomeUpgraded3","passiveManaUpgraded1","passiveManaUpgraded2","passiveManaUpgraded3"):
            f.write(f"{name}\t{data.findtext(name)}\n")
        for unit in ("swordwrath","spearton","miner","archer"):
            for field in data.find("Order/Units/"+unit):
                if not list(field) and re.fullmatch(r"[\d.\-]+",(field.text or "").strip()):
                    f.write(f"{unit}.{field.tag}\t{field.text.strip()}\n")
        for who in ("player","oponent"):
            f.write(f"{who}.statueHealth\t{level.find(who).get('statueHealth')}\n")
        f.write(f"normalHealthScale\t{level.findtext('normalHealthScale')}\n")
        f.write(f"enemy.handicap\t{level.findtext('insane')}\n")
        campaign=ElementTree.parse(next((WORK/"extract/binaryData").glob("29_*")))
        for unit in ("Miner","Spearton"):
            f.write(f"composition.{unit}\t{campaign.findtext('Order/UnitComposition/'+unit)}\n")
        for field in ("cost","mana","time"):
            f.write(f"castleArchers1.{field}\t{data.findtext('Order/Tech/castleArchers1/'+field)}\n")
        for name in ("body","bow"):
            key="4236" if name=="body" else "4176"
            child=next(e for e in timelines[key] if e.get("name")==("mc" if name=="body" else "bow"))
            f.write(f"archer.{name}X\t{child['matrix'][4]}\narcher.{name}Y\t{child['matrix'][5]}\n")
            f.write(f"archer.{name}Frames\t{c.timeline(child['id']).getFrameCount()}\n")
        barracks=next(e for e in timelines["1866"] if e.get("name")=="barracksHitArea")
        hit=c.swf.getCharacter(barracks["id"]).getRect()
        f.write(f"barracks.spawnX\t{barracks['matrix'][4]+(hit.Xmax-hit.Xmin)/40}\n")
    file=next((WORK/"extract/sounds").glob("*_OrderMainLoop*.mp3"))
    shutil.copyfile(file,c.directory/"orderInGame.mp3")


def swamp_edges(directory,rows):
    # Keep frame 1 at its authored width: that width drives the map and parallax.
    # Each independent side plate overlaps two pixels underneath the source.
    pad=198
    overlap=2
    for symbol in (4102,4098):
        source=next(row for row in rows if int(row[0])==symbol and int(row[1])==1)
        with Image.open(directory/source[2]) as image:
            original=image.convert("RGBA")
        rows[:]=[row for row in rows if int(row[0])!=symbol or int(row[1])==1]
        for frame in (2,3):
            # Work outwards from the edge. Use the empty soil or tree line of
            # this layer farther in, joined to the adjacent edge over 32 pixels.
            strip=original if frame==2 else original.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            near=strip.crop((1,0,pad+1,strip.height))
            far=strip.crop((335,0,335+pad,strip.height))
            weight=np.minimum(255,np.arange(pad)*8).astype(np.uint8)
            mask=Image.fromarray(np.broadcast_to(weight,(strip.height,pad)).copy())
            flank=Image.composite(far,near,mask)
            plate=Image.new("RGBA",(pad+overlap,strip.height))
            seam=strip.crop((1,0,1+overlap,strip.height)).transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            plate.paste(seam)
            plate.paste(flank,(overlap,0))
            if frame==2:
                plate=plate.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            name=f"{symbol}-edge-{frame}.png"
            plate.save(directory/name)
            x=-pad if frame==2 else original.width-overlap
            rows.append((symbol,frame,name,x,source[4],plate.width,plate.height))


def write_assets(module,refresh_ui=False,edges_only=False):
    directory=(WORK/"compiled"/module if module=="campaign-map" else ROOT/"assets"/module)
    if edges_only:
        with (directory/"textures.tsv").open() as f:
            rows=list(csv.reader(f,delimiter="\t"))
    else:
        compiler=ArtCompiler(directory)
        campaign=ElementTree.parse(next((WORK/"extract/binaryData").glob("29_*")))
        level=campaign.find("level")
        if module=="campaign-map":
            map_art(compiler,level)
        else:
            tutorial_art(compiler,level,refresh_ui)
        rows=compiler.rows
    if module=="tutorial":
        swamp_edges(directory,rows)
    with (directory/"textures.tsv").open("w",newline="") as f:
        csv.writer(f,delimiter="\t",lineterminator="\n").writerows(rows)


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("module",choices=["campaign-map","tutorial"])
    parser.add_argument("--refresh-ui",action="store_true")
    parser.add_argument("--refresh-edges",action="store_true")
    args=parser.parse_args()
    if args.refresh_edges and (args.module!="tutorial" or args.refresh_ui):
        parser.error("--refresh-edges requires tutorial and runs separately from --refresh-ui")
    write_assets(args.module,args.refresh_ui,args.refresh_edges)
