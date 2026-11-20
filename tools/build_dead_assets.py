"""Compile CampaignDeads' original mobile loadout and mission data."""
from xml.etree import ElementTree as ET
from build_menu_assets import ArtCompiler, ROOT, WORK
from build_archidon_assets import table
from build_campaign_assets import set_field


def build():
    c=ArtCompiler(ROOT/'assets/deads')
    data=ET.parse(next((WORK/'extract/binaryData').glob('10208_*')))
    campaign=ET.parse(next((WORK/'extract/binaryData').glob('29_*')))
    level=campaign.findall('level')[8]
    bounds=c.swf.getCharacter(4098).getRect()
    rules=[('map.length',(bounds.Xmax-bounds.Xmin)/20),('campaign.points',level.get('points')),
           ('normalHealthScale',level.findtext('normalHealthScale')),('normalDamageScale',level.findtext('normalDamageScale','1')),
           ('enemy.handicap',level.findtext('insane')),('enemy.castleArchers',level.findtext('oponent/castleArcher','0'))]
    for side in ('player','oponent'):
        rules.extend(((side+'.statueHealth',level.find(side).get('statueHealth')),
                      (side+'.gold',level.findtext(side+'/gold','500')),(side+'.mana',level.findtext(side+'/mana','0'))))
    for node in data.find('Chaos/Units/dead'):
        for field in (list(node) or [node]):
            try:value=float(field.text)
            except (ValueError,TypeError):continue
            key='dead.'+(node.tag+'.' if list(node) else '')+field.tag
            rules.append((key,value))
    for name in ('ChaosMiner','Bomber','Giant','Knight','Dead'):
        rules.append(('composition.'+name,level.findtext('oponent/UnitComposition/'+name,campaign.findtext('Chaos/UnitComposition/'+name))))
    rules.append(('composition.Miner',dict(rules)['composition.ChaosMiner']))
    # Default mobile head and Wrapped Helmet castle head use separate frames.
    rules.extend((('dead.hitWidth',104.95),('dead.hitHeight',116.15)))
    poses=[];label=''
    for event in c.index['timelines']['8529']:
        if event['op']=='label':label=event['name']
        if event.get('name')!='mc' or 'id' not in event or label not in {'stand','run','attack_1','death_1'}:continue
        symbol=event['id'];c.render(symbol,scale=1,frame_offset=10000)
        poses.append((8529,label,symbol,*event['matrix'][4:],c.timeline(symbol).getFrameCount(),10000))
    if len(poses)!=4:raise ValueError('Incomplete mobile Dead animation')
    c.render(1904,[3],scale=1)
    frames=c.timeline(1904).getFrames();frames.set(0,frames.get(2));frames.subList(1,frames.size()).clear()
    c.render(1922,scale=.8,frame_offset=712)
    for symbol,value in [(292,'Order'),(293,level.findtext('oponent/raceName')),(337,level.findtext('tip'))]:set_field(c,symbol,value)
    for symbol,depths in [(343,(2,3,19,22,25,30,31,32,34,36,40,41,57,68)),(338,(4,))]:
        for depth in depths:c.timeline(symbol).getFrame(0).layers.remove(c.J('java.lang.Integer')(depth))
        c.render(symbol,[1],scale=1,frame_offset=8)
    table(c.directory/'textures.tsv',c.rows);table(c.directory/'rules.tsv',rules)
    table(c.directory/'animations.tsv',poses)
    table(c.directory/'starting.tsv',((side,n.text) for side in ('player','oponent') for n in level.findall(side+'/startingUnit')))
    table(c.directory/'available.tsv',((n.text,) for n in level.findall('oponent/unit')))


if __name__=='__main__':build()
