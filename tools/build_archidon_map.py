"""Compile the original second-to-third mission travel and labels."""
from xml.etree import ElementTree as ET
from math import ceil, floor
import argparse

import jpype
import numpy as np
from PIL import Image

from build_menu_assets import ArtCompiler,ROOT,WORK
from build_archidon_assets import table
from build_campaign_assets import set_field


def build(level=3):
    folders={3:"archidon-map",4:"shadow-map",5:"magic-map"}
    c=ArtCompiler(ROOT/"assets"/folders.get(level,f"campaign-map-{level}"))
    stops=[e["frame"] for e in c.index["timelines"]["499"] if e["op"]=="label"]
    start,end=stops[level-2:level]
    movements=[]
    current={}
    for owner,depth in ((499,1),(436,2297)):
        changes={event["frame"]:event["matrix"] for event in c.index["timelines"][str(owner)]
                 if event.get("depth")==depth and "matrix" in event}
        last=changes[1]
        poses=[]
        for frame in range(1,end+1):
            last=changes.get(frame,last)
            poses.append(last)
        current[owner]=poses
    for index,(map_,flag) in enumerate(zip(current[499],current[436]),1):
        movements.append((index,map_[0],map_[4]-657.7,map_[5]-584.9,flag[4],flag[5]))
    for frame in range(start+1,end+1):
        c.timeline(436).getFrame(frame-1).layers.remove(jpype.JInt(2297))
        _,scale,x,y,*_=movements[frame-1]
        left=min(-140,floor(((850-700*16/9)/2-x)/scale)-2)
        right=max(897,ceil(((850+700*16/9)/2-x)/scale)+2)
        top=min(13,floor(-y/scale)-2)
        bottom=max(701,ceil((700-y)/scale)+2)
        # Later territories move original geography above y=13. Render that
        # actual region instead of stretching the old crop's first scanline.
        c.render(436,[frame],scale=1,box=(left,897,top,bottom))
        row=c.rows[-1]
        with Image.open(c.directory/row[2]) as source: image=np.array(source.convert("RGBA"))
        water=image[:,-105:]
        sea=np.tile(np.concatenate((water[:,::-1],water),axis=1),(1,ceil((right-897)/210),1))[:,:right-897]
        image=np.concatenate((image,sea),axis=1)
        image[:,:,3]=255
        Image.fromarray(image).save(c.directory/row[2])
        c.rows[-1]=(*row[:4],top,image.shape[1],image.shape[0])
    destination=ET.parse(next((WORK/"extract/binaryData").glob("29_*"))).findall("level")[level-1]
    for symbol,value in ((486,destination.get("story")),(488,destination.get("title")),(491,"Game saved at "+destination.get("title"))):
        set_field(c,symbol,value)
        c.render(symbol,[1],scale=1,frame_offset=level-2)
    table(c.directory/"textures.tsv",c.rows)
    table(c.directory/"map-motion.tsv",movements)


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--shadow",action="store_true")
    parser.add_argument("--magic",action="store_true")
    parser.add_argument("--level",type=int,choices=range(3,15),default=3)
    args=parser.parse_args()
    build(5 if args.magic else 4 if args.shadow else args.level)
