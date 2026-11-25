"""Render the original toolBoxMc/TipBox fields, artwork and embedded fonts."""
from math import floor
from html import escape
import os
from pathlib import Path
import re
from xml.etree import ElementTree as ET
from build_menu_assets import ArtCompiler, ROOT, WORK
from build_archidon_assets import table
from build_campaign_assets import set_field

UNITS={3104:'miner',4922:'swordwrath',6456:'spearton',4236:'archer',7289:'ninja',
       7926:'magikill',8233:'monk',7526:'giant'}
COMMANDS={-20:'attackMove',-21:'stand',-22:'hold',-23:'garrison',-24:'ungarrison'}
ABILITIES={90001:'swordwrath/rage',90002:'ninja/stealth',90003:'magikill/nuke',
           90004:'magikill/poisonSpray',90005:'magikill/electricWall',90006:'monk/heal',90007:'monk/cure'}


def build():
    c=ArtCompiler(Path(os.environ.get('DW2_TOOLTIP_OUT',ROOT/'assets/tooltips')))
    data=ET.parse(next((WORK/'extract/binaryData').glob('10208_*'))).getroot()
    root=c.timeline(1119).getFrame(0).layers
    stats=c.timeline(1116).getFrame(0).layers
    originalStats=c.J('java.util.HashMap')(stats)
    statDisplay=root.get(c.J('java.lang.Integer')(6))
    rows=[]
    def render(key,title,info,time=0,gold=0,mana=0,population=0,hide=False,delay=4):
        set_field(c,1102,title);set_field(c,1103,info)
        if key==90008:
            # Preserve the original TextField's three measured lines. FFDec's
            # automatic HTML wrap clips this particular tooltip's second line.
            lines=(ROOT/'tools/fixtures/original-block-tooltip.txt').read_text(encoding='utf-8').splitlines()
            assert ' '.join(lines)==info
            field=c.swf.getCharacter(1103)
            font=re.search(r'<font[^>]*>',str(field.initialText)).group(0)
            field.initialText=''.join('<p align="left">'+font+escape(line.rstrip())+'</font></p>' for line in lines)
        for symbol,value in ((1106,str(gold)),(1107,str(floor(time/30+.5))+'s'),
                             (1108,str(population)),(1109,str(mana))):set_field(c,symbol,value)
        root.get(c.J('java.lang.Integer')(3)).matrix.translateY=round((421.6 if hide else 497.6)*20)
        if hide:root.remove(c.J('java.lang.Integer')(6))
        else:root.put(c.J('java.lang.Integer')(6),statDisplay)
        stats.clear();stats.putAll(originalStats)
        if not population:
            for depth in (4,9):stats.remove(c.J('java.lang.Integer')(depth))
        frame=20001+len(rows)
        c.render(1119,[1],scale=1.5,frame_offset=frame-1)
        rows.append((key,frame,delay,title))
    def xml(key,node,tech=False):
        if node is None:raise ValueError('Missing original tooltip XML')
        render(key,node.findtext('name'),node.findtext('tip' if tech else 'info'),
               int(node.findtext('time' if tech else 'cooldown','0')),
               int(node.findtext('cost' if tech else 'gold','0')),
               int(node.findtext('mana','0')),int(node.findtext('population','0')))
    for key,name in UNITS.items():xml(key,data.find('Order/Units/'+name))
    xml(-1,data.find('Order/Tech/castleArchers1'),True)
    for key,name in COMMANDS.items():xml(key,data.find('Commands/'+name))
    for key,path in ABILITIES.items():xml(key,data.find('Order/Units/'+path))
    source=(WORK/'extract/scripts/com/brockw/stickwar/engine/UserInterface.as').read_text(encoding='utf-8')
    originals=dict(re.findall(r'displayTip\("([^"]+)","([^"]+)",0,0,0,0,true\)',source))
    for key,name in ((-10,'Garrison'),(-11,'Defend'),(-12,'Attack'),(-13,'Garrison Miners'),(-14,'Resume Mining')):
        render(key,name,originals[name],hide=True,delay=24)
    # Append to preserve every existing original tooltip frame binding.
    for tier in (2,3):xml(-tier,data.find(f'Order/Tech/castleArchers{tier}'),True)
    xml(90008,data.find('Order/Units/spearton/shieldWall'))
    table(c.directory/'textures.tsv',c.rows)
    table(c.directory/'tooltips.tsv',rows)
    print('Original toolBoxMc variants:',len(rows),flush=True)


if __name__=='__main__':build()
