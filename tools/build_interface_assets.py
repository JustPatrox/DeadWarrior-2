"""Recover the original castle hit areas; artwork is exported by build_castle_assets."""
from build_menu_assets import ArtCompiler,ROOT
from build_archidon_assets import table
from PIL import Image


def build():
    c=ArtCompiler(ROOT/'assets/interface')
    names={'magicShopMc':'magicHitArea','templeMc':'templeHitArea','bankMc':'bankHitArea',
           'siegeShop':'dungeonHitArea','barracksMc':'barracksHitArea','archeryRangeMc':'fletcherHitArea'}
    events=c.index['timelines']['1866'];rows=[];masks=[]
    for name,area in names.items():
        e=next(e for e in events if e.get('name')==name)
        hit=next(e for e in events if e.get('name')==area)
        b=c.swf.getCharacter(hit['id']).getRect()
        rows.append((e['id'],*e['matrix'][4:],hit['matrix'][4]+b.Xmin/20,
                     hit['matrix'][5]+b.Ymin/20,(b.Xmax-b.Xmin)/20,(b.Ymax-b.Ymin)/20))
        c.render(hit['id'],[1],scale=1)
        record=c.rows.pop()
        with Image.open(c.directory/record[2]) as image:
            alpha=image.convert('RGBA').getchannel('A')
            for y in range(alpha.height):
                start=None
                for x in range(alpha.width+1):
                    inside=x<alpha.width and alpha.getpixel((x,y))>0
                    if inside and start is None:start=x
                    elif not inside and start is not None:
                        masks.append((e['id'],hit['matrix'][4]+record[3]+start,hit['matrix'][5]+record[4]+y,x-start,1));start=None
        (c.directory/record[2]).unlink()
    table(c.directory/'castle.tsv',rows)
    table(c.directory/'castle-masks.tsv',masks)


if __name__=='__main__':build()
