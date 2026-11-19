"""Compile CampaignUpgradeScreen and the first post-victory CampaignScreen transition."""
import csv
import re
import shutil
from xml.etree import ElementTree as ET

import jpype
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from build_menu_assets import ArtCompiler, ROOT, WORK
from build_campaign_assets import positions, set_field


def write_table(path, rows):
    with path.open("w", newline="", encoding="utf-8") as out:
        csv.writer(out, delimiter="\t", lineterminator="\n").writerows(rows)


def widen_armory(c):
    # The central room is painted into bitmap 506. Extend the near wall,
    # floor and stone trim from empty material, keeping the weapon racks intact.
    path=c.directory/"507-1.png"
    center=np.asarray(Image.open(path).convert("RGB"), dtype=float)
    height,width=center.shape[:2]
    def side(right):
        edge=center[:,-1] if right else center[:,0]
        result=np.zeros((height,198,3))
        y=np.arange(height)
        for x in range(198):
            # Continue the perspective of the junctions with ceiling and floor.
            wall_top=88-.14*x
            floor_top=550+.58*x
            sample_y=np.interp(y,[0,65,max(66,wall_top),min(619,floor_top),620,699],
                               [0,76,88,550,620,699])
            strip=center[:,4:10].mean(axis=1)
            wall=np.array([np.interp(sample_y,y,strip[:,ch]) for ch in range(3)]).T
            # The broad rail below the painted room uses the original stone surface.
            trim=center[:,28+(x%95)]
            wall[620:]=trim[620:]
            wall[:68]=center[:68,240+x%60]
            blend=np.exp(-x/14)
            result[:,x]=wall*(1-blend)+edge*blend
        return np.uint8(np.clip(result,0,255))
    Image.fromarray(np.concatenate((side(False)[:,::-1],center.astype("uint8"),side(True)),axis=1)).save(path)
    c.rows[-1]=(507,1,path.name,-198,0,width+396,height)


def separate_modal(c):
    # The source shape surrounds its panel with half-opaque black. Remove that
    # matte analytically so the native view can dim the entire 16:9 stage once.
    id,frame,name,x,y,w,h=c.rows[-1]
    path=c.directory/name
    pixels=np.array(Image.open(path).convert("RGBA"),dtype=float)/255
    scale=pixels.shape[1]/w
    inner=pixels[12:-12,12:-12]
    yy,xx=np.where((inner[:,:,3]>.501)|(inner[:,:,:3].max(axis=2)>0))
    left,top,right,bottom=xx.min()+9,yy.min()+9,xx.max()+16,yy.max()+16
    panel=pixels[top:bottom,left:right].copy()
    alpha=panel[:,:,3:4]
    ink=np.clip((alpha-127/255)/(1-127/255),0,1)
    rgb=np.clip(panel[:,:,:3]*alpha/np.maximum(ink,1/255),0,1)
    Image.fromarray(np.uint8(np.round(255*np.concatenate((rgb,ink),axis=2)))).save(path)
    c.rows[-1]=(id,frame,name,x+left/scale,y+top/scale,(right-left)/scale,(bottom-top)/scale)


def confirmation_text(c):
    field=c.swf.getCharacter(684)
    html=ET.fromstring("<text>"+str(field.initialText)+"</text>")
    lines=["".join(p.itertext()) for p in html]
    # FFDec's HTML renderer piles up the glyphs of this field. Lay out the
    # original paragraphs with the advances from the SWF's own exported TTF.
    scale=3
    font=ImageFont.truetype(str(next((WORK/"extract/fonts").glob(f"{field.fontId}_*.ttf"))),
                            round(field.fontHeight/20*scale))
    bounds=field.getRect()
    width=round(max((bounds.Xmax-bounds.Xmin)/20*scale,max(font.getlength(s) for s in lines)+4*scale))
    line_height=sum(font.getmetrics())
    image=Image.new("RGBA",(width,line_height*len(lines)+scale*2))
    draw=ImageDraw.Draw(image)
    color=field.textColor
    for n,line in enumerate(lines):
        draw.text((width/2,n*line_height),line,font=font,anchor="ma",fill=(color.red,color.green,color.blue,color.alpha))
    name="684-1.png"
    image.save(c.directory/name)
    center=(bounds.Xmax+bounds.Xmin)/40
    c.rows.append((684,1,name,center-width/(2*scale),0,width/scale,image.height/scale))


def upgrades(c, data):
    scripts=WORK/"extract/scripts/com/brockw/stickwar"
    screen=(scripts/"campaign/CampaignUpgradeScreen.as").read_text()
    campaign=(scripts/"campaign/Campaign.as").read_text()
    tech=next(scripts.rglob("GoodTech.as")).read_text()
    buttons={name:int(index) for name,index in re.findall(r'setUpButton\("([^"]+)",this.mc.button(\d+)\)',screen)}
    xml_paths={key:path.replace(".","/") for key,path in re.findall(
        r'addNewUpgrade\((?:Tech\.)?(\w+),game.xml.xml\.([\w.]+)',tech)}
    nodes={name:(parents,key) for name,parents,key in re.findall(
        r'new CampaignUpgrade\("([^"]+)",\[([^\]]*)\],\[[^\]]*\],Tech\.(\w+)\)',campaign)}
    events=c.index["timelines"]["694"]
    root=c.timeline(694).getFrame(0).layers
    c.render(507,[1],scale=1)
    widen_armory(c)
    rows=[]
    for event in events:
        if event.get("op")!="place": continue
        name=event.get("name","")
        if name or event["id"]==507: root.remove(jpype.JInt(event["depth"]))
        if name.startswith("button"):
            index=int(name[6:])
            label=next(label for label,button in buttons.items() if button==index)
            parents,key=nodes[label]
            prerequisite=[buttons[parent] for parent in re.findall(r'"([^"]+)"',parents)]
            assert len(prerequisite)<=1
            tip=data.findtext(xml_paths[key]+"/tip")
            if not tip: raise ValueError(f"Missing tip for {label}")
            rows.append((index,event["id"],prerequisite[0] if prerequisite else 0,label,tip.rstrip()))
            c.render(event["id"],[1,2,3],scale=1)
    write_table(c.directory/"upgrades.tsv", sorted(rows))
    confirmation_text(c)
    layers=c.timeline(693).getFrame(0).layers
    for depth in (3,4,8): layers.remove(jpype.JInt(depth))
    for symbol in (671,681,689,692,693,694):
        c.render(symbol,scale=1)
        if symbol==693: separate_modal(c)
    # Campaign points use the original font, size and centered alignment.
    for n in range(15):
        set_field(c,682,str(n)); c.render(682,[1],scale=1,frame_offset=n)
    shutil.copyfile(next((WORK/"extract/sounds").glob("*_ArmoryEquipSound_*.mp3")),c.directory/"ArmoryEquipSound.mp3")


def campaign_map(c,level):
    # The map moves with respect to the screen, and the flag moves over the map.
    # Draw these two independent timelines with their own registration points.
    def matrices(owner,depth):
        state={}
        for e in c.index["timelines"][str(owner)]:
            if e.get("depth")==depth and "matrix" in e: state[e["frame"]]=e["matrix"]
        m=state[1]
        for frame in range(1,51):
            m=state.get(frame,m)
            yield m
    track=list(zip(matrices(499,1),matrices(436,2297)))
    write_table(c.directory/"map-motion.tsv",
                [(i+1,mat[0],mat[4]-657.7,mat[5]-584.9,flag[4],flag[5]) for i,(mat,flag) in enumerate(track)])
    # The intermediate map timeline adds individual route markers to the
    # second-frame plate. Keep that small updating area separate on disk.
    base=None
    for frame in range(1,51):
        layers=c.timeline(436).getFrame(frame-1).layers
        layers.remove(jpype.JInt(2297))
        is_plate=frame in (1,2,50)
        c.render(436,[frame],scale=1,box=(-140,897,13,701) if is_plate else (540,600,360,515))
        path=c.directory/c.rows[-1][2]
        if is_plate:
            with Image.open(path) as im: rgb=np.array(im.convert("RGBA"))
            # The eastern edge is open water. Extend its existing surface to
            # cover the right side even at frame 50, the camera's leftmost x.
            water=rgb[:,-105:]
            sea=np.concatenate((water[:,::-1],water,water[:,::-1]),axis=1)[:,:225]
            rgb[:,:,3]=255
            sea[:,:,3]=255
            Image.fromarray(np.concatenate((rgb,sea),axis=1)).save(path)
            row=c.rows[-1]; c.rows[-1]=(*row[:5],row[5]+sea.shape[1],row[6])
            if frame==2: base=(rgb,c.rows[-1][3:5])
        else:
            with Image.open(path) as im: pixels=np.array(im.convert("RGBA"))
            row=c.rows[-1]; rgb,origin=base
            x,y=round(row[3]-origin[0]),round(row[4]-origin[1])
            changed=(pixels!=rgb[y:y+pixels.shape[0],x:x+pixels.shape[1]]).any(axis=2)
            pixels[~changed]=0
            Image.fromarray(pixels).save(path)
    c.render(393,[1]*15,child_ticks=range(15),scale=1)
    c.timeline(393).getFrame(0).layers.remove(jpype.JInt(1))
    c.render(393,[1],scale=1,frame_offset=15)
    for symbol,value in ((486,level.get("story")),(488,level.get("title")),(491,"Game saved at "+level.get("title"))):
        set_field(c,symbol,value)
    for symbol in (439,442,454,463,472,476,485,486,488,490,491,497):
        c.render(symbol)
        if symbol==490: separate_modal(c)
        if symbol==439:
            # Extend the rocky lip behind the buttons. Its higher center and
            # all interactive panel art retain their authored positions.
            id,frame,name,x,y,w,h=c.rows[-1]
            path=c.directory/name
            src=np.array(Image.open(path).convert("RGBA"))
            side=round(198*src.shape[1]/w)
            Image.fromarray(np.concatenate((src[:,:side][:,::-1],src,src[:,-side:][:,::-1]),axis=1)).save(path)
            c.rows[-1]=(id,frame,name,x-198,y,w+396,h)
    shutil.copyfile(next((WORK/"extract/sounds").glob("*_SelectRaceSound_*.mp3")),c.directory/"SelectRaceSound.mp3")


if __name__=="__main__":
    c=ArtCompiler(ROOT/"assets/intermission")
    data=ET.parse(next((WORK/"extract/binaryData").glob("10208_*")))
    campaign=ET.parse(next((WORK/"extract/binaryData").glob("29_*")))
    levels=campaign.findall("level")
    upgrades(c,data)
    campaign_map(c,levels[1])
    positions(c,(694,693,499,478,477,498))
    write_table(c.directory/"textures.tsv",c.rows)
