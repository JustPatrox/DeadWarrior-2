"""Export authored HUD button states, action-slot pulses and recruit highlights."""
from math import lcm
from PIL import Image
from build_menu_assets import ArtCompiler, ROOT
from build_archidon_assets import table

SIMPLE={'defendButton':9977,'garrisonButton':9984,'attackButton':9991,
        'leftMinerButton':10067,'rightMinerButton':10077,'menuButton':10084}
SLOTS={'action1':9997,'action2':10003,'action3':10015,'action4':10009,'action5':10009,
       'action6':10009,'action7':10021,'action8':10027,'action9':10033}
HIGHLIGHTS={'bankHighlight':10109,'barracksHighlight':9859,'archerHighlight':9861,
            'templeHighlight':10111,'magikillHighlight':10113,'giantHighlight':10115}
OVERLAYS={name:9855 for name in ('minerOverlay','swordwrathOverlay','archerOverlay','monkOverlay',
                                'magikillOverlay','speartonOverlay','ninjaOverlay','giantOverlay')}


def build():
    c=ArtCompiler(ROOT/'assets/toolbar');states=[];layout=[];masks=[]
    events=c.index['timelines']['10118']
    for name,owner in {**SIMPLE,**SLOTS,**HIGHLIGHTS,**OVERLAYS}.items():
        event=next(e for e in events if e.get('name')==name)
        matrix=event['matrix'];expected=([1,0,0,1],[1,0,0,-1]) if name in OVERLAYS else ([1,0,0,1],)
        assert event['id']==owner and matrix[:4] in expected,(name,event)
        color=event.get('color',[256,256,256,256,0,0,0,0])
        assert color[:3]==[256,256,256] and color[4:]==[0,0,0,0],(name,color)
        layout.append((name,owner,*matrix[4:],color[3]/256,int(matrix[3]<0)))
    def period(owner,seen=frozenset()):
        if owner in seen:return 1
        meta=c.index['symbols'][str(owner)]
        if meta['kind']!='sprite':return 1
        result=meta['frames']
        for event in c.index['timelines'].get(str(owner),[]):
            if 'id' in event:result=lcm(result,period(event['id'],seen|{owner}))
        if result>600:raise ValueError(('Inspect authored toolbar period',owner,result))
        return result
    for owner in sorted(set(SIMPLE.values())|set(SLOTS.values())):
        timeline=c.timeline(owner)
        for state in (1,2,3):
            count=1
            for child in timeline.getFrame(state-1).layers.values():
                if child.characterId>0:count=lcm(count,period(int(child.characterId)))
            base=70000+(state-1)*1000
            c.render(owner,[state]*count,child_ticks=range(count),scale=1.5,frame_offset=base)
            states.append((owner,state,base+1,count))
        if owner in SIMPLE.values():
            c.render(owner,[4],scale=1,frame_offset=72997)
            row=c.rows[-1]
            with Image.open(c.directory/row[2]) as image:
                alpha=image.convert('RGBA').getchannel('A')
                for y in range(alpha.height):
                    start=None
                    for x in range(alpha.width+1):
                        inside=x<alpha.width and alpha.getpixel((x,y))>0
                        if inside and start is None:start=x
                        elif not inside and start is not None:
                            masks.append((owner,row[3]+start,row[4]+y,x-start,1));start=None
    for owner in HIGHLIGHTS.values():c.render(owner,[1],scale=1.5,frame_offset=74000)
    c.render(9855,[1],scale=1.5,frame_offset=74000)
    # Keep each interactive part separate from the same original HUD plate.
    depths={1,2,10,17,132,145};hud=c.timeline(10118).getFrame(0).layers
    for depth in list(hud.keySet()):
        if int(depth) not in depths:hud.remove(depth)
    c.render(10118,[1],scale=1.5,frame_offset=11000)
    table(c.directory/'textures.tsv',c.rows);table(c.directory/'states.tsv',states)
    table(c.directory/'layout.tsv',layout);table(c.directory/'hits.tsv',masks)
    print('Original toolbar:',len(c.rows),'textures;',states)


if __name__=='__main__':build()
