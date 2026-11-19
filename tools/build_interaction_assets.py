"""Recover original contextual pointers, music and sound effects."""
import numpy as np
from PIL import Image
import re
import subprocess
import sys
import imageio_ffmpeg
from build_menu_assets import ArtCompiler, ROOT, WORK
from build_archidon_assets import table


def build():
    c=ArtCompiler(ROOT/"assets/interaction")
    for symbol in (2310,2313,8821,8824,8785,8800):
        c.render(symbol,scale=1)
    with Image.open(c.directory/"2313-1.png") as source:
        pixels=np.array(source.convert("RGBA"))
    pixels[:,:,:3]=np.clip(np.round(pixels[:,:,:3]*.6+[102,0,0]),0,255)
    Image.fromarray(pixels).save(c.directory/"2313-1.png")
    c.render(1595,[1],scale=1)
    row=c.rows[-1]
    with Image.open(c.directory/row[2]) as source: fog=source.convert("RGBA")
    rgb=fog.convert("RGB").point(lambda value:round(value*.1))
    rgb.putalpha(fog.getchannel("A"))
    pixels=np.array(rgb)
    tail=np.tile(pixels[:,-100:],(1,4,1))
    Image.fromarray(np.concatenate((pixels,tail),axis=1)).save(c.directory/row[2])
    c.rows[-1]=(*row[:5],row[5]+400,row[6])
    table(c.directory/"textures.tsv",c.rows)
    table(c.directory/"cursors.tsv",((symbol,c.timeline(symbol).getFrameCount()) for symbol in (2310,2313,8821,8824,8785,8800)))
    build_audio()


def build_audio():
    loader=(WORK/"extract/scripts/com/brockw/stickwar/engine/SoundLoader.as").read_text()
    audio=ROOT/"assets/audio"
    audio.mkdir(exist_ok=True)
    rows=[]
    for name,linkage,args in re.findall(r'soundManager\.addSound\("([^"]+)",(\w+),([^;]+)\);',loader):
        sources=list((WORK/"extract/sounds").glob(f"*_{linkage}_{linkage}.*"))
        if not sources: raise ValueError(f"Missing original sound {linkage}")
        target=audio/(name+".wav")
        if not target.exists(): subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(),"-v","error","-y","-i",sources[0],
                        "-ac","2","-ar","44100","-c:a","pcm_s16le",target],check=True)
        fields=args.split(",")
        gain=float(fields[1]) if len(fields)>1 else 1.
        rows.append((name,target.name,gain*1.4))
    required={"ClickGold","ClickMana","CommandMove1","swordwrathSwing1","hitOnFlesh1","UnitReady"}
    if not required.issubset({r[0] for r in rows}): raise ValueError("Incomplete interaction sounds")
    table(audio/"sounds.tsv",rows)


if __name__=="__main__":
    build_audio() if "--audio-only" in sys.argv else build()
