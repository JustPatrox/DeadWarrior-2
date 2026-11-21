"""Compile original Cat/Crawler artwork and the tenth campaign deployment."""
from xml.etree import ElementTree as ET
from build_menu_assets import ArtCompiler, ROOT, WORK
from build_archidon_assets import table
from build_campaign_assets import set_field


def build():
    c=ArtCompiler(ROOT/'assets/crawlers')
    data=ET.parse(next((WORK/'extract/binaryData').glob('10208_*')))
    campaign=ET.parse(next((WORK/'extract/binaryData').glob('29_*')))
    level=campaign.findall('level')[9]
    terrain=c.swf.getCharacter(4124).getRect()
    rules=[('map.length',(terrain.Xmax-terrain.Xmin)/20),('campaign.points',level.get('points')),
           ('normalHealthScale',level.findtext('normalHealthScale')),('normalDamageScale',level.findtext('normalDamageScale','1')),
           ('enemy.handicap',level.findtext('insane')),('enemy.castleArchers',level.findtext('oponent/castleArcher','0'))]
    for side in ('player','oponent'):
        rules.extend(((side+'.statueHealth',level.find(side).get('statueHealth')),
                      (side+'.gold',level.findtext(side+'/gold','500')),(side+'.mana',level.findtext(side+'/mana','0'))))
    for node in data.find('Chaos/Units/cat'):
        for field in (list(node) or [node]):
            try:value=float(field.text)
            except (ValueError,TypeError):continue
            rules.append(('cat.'+(node.tag+'.' if list(node) else '')+field.tag,value))
    for name in ('ChaosMiner','Bomber','Giant','Knight','Cat','Dead'):
        rules.append(('composition.'+name,level.findtext('oponent/UnitComposition/'+name,campaign.findtext('Chaos/UnitComposition/'+name))))
    rules.append(('composition.Miner',dict(rules)['composition.ChaosMiner']))
    # Original Unit.firstInit readback; Cat then explicitly retains pheight150.
    rules.extend((('cat.hitWidth',93.45),('cat.hitHeight',150)))
    poses=[];tips=[];label=''
    wanted={'stand','run','attack_1','attack_2','death_1'}
    for event in c.index['timelines']['6123']:
        if event['op']=='label':label=event['name']
        if event.get('name')!='mc' or 'id' not in event or label not in wanted:continue
        symbol=event['id'];c.render(symbol,scale=1);timeline=c.timeline(symbol)
        poses.append((6123,label,symbol,*event['matrix'][4:],timeline.getFrameCount(),0))
        if label.startswith('attack_'):
            for number,frame in enumerate(timeline.getFrames(),1):
                for state in frame.layers.values():
                    if str(state.instanceName)=='tip':tips.append((symbol,number,state.matrix.translateX/20,state.matrix.translateY/20))
    if len(poses)!=5 or not tips:raise ValueError('Incomplete original Crawler animation or hit markers')
    for symbol,value in [(292,'Order'),(293,level.findtext('oponent/raceName')),(337,level.findtext('tip'))]:set_field(c,symbol,value)
    for symbol,depths in [(343,(2,3,19,22,25,30,31,32,34,36,40,41,57,68)),(338,(4,))]:
        for depth in depths:c.timeline(symbol).getFrame(0).layers.remove(c.J('java.lang.Integer')(depth))
        c.render(symbol,[1],scale=1,frame_offset=9)
    table(c.directory/'textures.tsv',c.rows);table(c.directory/'rules.tsv',rules)
    table(c.directory/'animations.tsv',poses);table(c.directory/'tips.tsv',tips)
    table(c.directory/'starting.tsv',((side,n.text) for side in ('player','oponent') for n in level.findall(side+'/startingUnit')))
    table(c.directory/'available.tsv',((n.text,) for n in level.findall('oponent/unit')))


if __name__=='__main__':build()
