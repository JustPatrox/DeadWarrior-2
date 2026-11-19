"""Compile CampaignBomber data, bomber poses, and the allied Giant's throw."""
from xml.etree import ElementTree as ET
from build_menu_assets import ArtCompiler, ROOT, WORK
from build_archidon_assets import table
from build_campaign_assets import set_field


def build():
    c=ArtCompiler(ROOT/'assets/bomber')
    data=ET.parse(next((WORK/'extract/binaryData').glob('10208_*')))
    campaign=ET.parse(next((WORK/'extract/binaryData').glob('29_*')))
    level=campaign.findall('level')[6]
    bounds=c.swf.getCharacter(4098).getRect()
    rules=[('map.length',(bounds.Xmax-bounds.Xmin)/20),('campaign.points',level.get('points')),
           ('normalHealthScale',level.findtext('normalHealthScale')),('normalDamageScale',level.findtext('normalDamageScale','1')),
           ('enemy.handicap',level.findtext('insane')),('enemy.castleArchers',level.findtext('oponent/castleArcher','0'))]
    for side in ('player','oponent'):
        rules.extend(((side+'.statueHealth',level.find(side).get('statueHealth')),
                      (side+'.gold',level.findtext(side+'/gold','500')),(side+'.mana',level.findtext(side+'/mana','0'))))
    for prefix,path in [('bomber','Chaos/Units/bomber'),('enslavedGiant','Order/Units/giant')]:
        for node in data.find(path):
            if list(node) or node.text is None:continue
            try:value=float(node.text)
            except ValueError:continue
            rules.append((prefix+'.'+node.tag,value))
    for name in ('ChaosMiner','Bomber','Giant'):
        rules.append(('composition.'+name,level.findtext('oponent/UnitComposition/'+name,campaign.findtext('Chaos/UnitComposition/'+name))))
    rules.append(('composition.Miner',dict(rules)['composition.ChaosMiner']))
    # Unit.firstInit measures the original display object, before cropped PNG
    # export. Adobe readback verifies these bounds (including authored scale).
    for prefix,width,height in [('bomber',80.7,81.45),('enslavedGiant',184.5,264.1),
                                ('giant',299.95,288.35),('chaosMiner',180,130)]:
        rules.extend(((prefix+'.hitWidth',width),(prefix+'.hitHeight',height)))
    poses=[]
    for owner,names in [(8774,{'run','stand'}),(7526,{'attack_1'})]:
        label=''
        for event in c.index['timelines'][str(owner)]:
            if event['op']=='label':label=event['name']
            if event.get('name')!='mc' or 'id' not in event or label not in names:continue
            symbol=event['id'];c.render(symbol,scale=1)
            poses.append((owner,label,symbol,*event['matrix'][4:],c.timeline(symbol).getFrameCount(),0))
    c.render(3970,[1],scale=1)
    # Boulder places its child at (-width/2,-height/2) before rotating it.
    c.rows=[(*row[:3],-row[5]/2,-row[6]/2,*row[5:]) if row[0]==3970 else row for row in c.rows]
    c.render(9852,[1],scale=1)
    for symbol,value in [(292,'Order'),(293,level.findtext('oponent/raceName')),
                         (326,data.findtext('Order/Units/giant/info')),(334,data.findtext('Order/Units/giant/name')),
                         (337,level.findtext('tip'))]:set_field(c,symbol,value)
    for symbol,depths in [(343,(2,3,19,22,25,30,31,32,34,36,40,41,57,68)),(335,(4,)),(338,(4,))]:
        for depth in depths:c.timeline(symbol).getFrame(0).layers.remove(c.J('java.lang.Integer')(depth))
        c.render(symbol,[1],scale=1,frame_offset=6)
    # The Order statue is Swordwrath; Chaos uses its team-wide statue skin.
    frames=c.timeline(1904).getFrames();order=frames.get(7);chaos=frames.get(10)
    frames.set(0,order);frames.subList(1,frames.size()).clear();c.render(1922,scale=.8,frame_offset=534)
    frames.set(0,chaos);c.render(1922,scale=.8,frame_offset=623)
    table(c.directory/'textures.tsv',c.rows);table(c.directory/'rules.tsv',rules)
    table(c.directory/'animations.tsv',poses)
    table(c.directory/'starting.tsv',((side,n.text) for side in ('player','oponent') for n in level.findall(side+'/startingUnit')))


if __name__=='__main__':build()
