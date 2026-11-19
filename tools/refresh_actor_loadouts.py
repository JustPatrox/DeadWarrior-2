"""Rebake actor poses only, retaining terrain, UI, rules and audio manifests."""
import csv
import json
from build_menu_assets import ArtCompiler,ROOT
from build_archidon_assets import table

def build():
    c=ArtCompiler(ROOT/"assets/tutorial",loadout_overrides={2121:"Wrapped Helmet"})
    report=dict(items=c.loadouts,folders={})
    # Native archers compose the bow independently at its authored transform.
    for symbol in (4176,4221,4232):
        for frame in c.timeline(symbol).getFrames():
            for depth in list(frame.layers.keySet()):
                if int(frame.layers.get(depth).characterId)==4168: frame.layers.remove(depth)
    for folder in ("tutorial","archidon","shadow","magic","giants"):
        c.select_loadout(2004,"Basic Helmet" if folder=="tutorial" else "Default")
        c.directory=ROOT/"assets"/folder; c.rows=[]
        with (c.directory/"textures.tsv").open() as stream:
            previous=list(csv.reader(stream,delimiter="\t"))
        jobs={}
        for line in (c.directory/"animations.tsv").read_text().splitlines():
            fields=line.split(); symbol=int(fields[2]); base=int(fields[6]) if len(fields)>6 else 0
            jobs[symbol,base]=None
        if folder=="tutorial": jobs.update({(4176,0):None,(4168,0):None})
        for symbol,base in jobs: c.render(symbol,scale=1,frame_offset=base)
        rows={(int(row[0]),int(row[1])):row for row in previous}
        rows.update({(row[0],row[1]):row for row in c.rows})
        table(c.directory/"textures.tsv",rows.values())
        report["folders"][folder]=dict(poses=len(jobs),frames=len(c.rows))
        print(folder,report["folders"][folder],flush=True)
    destination=ROOT/"evidence/windows-interaction/loadout-checks.json"
    destination.write_text(json.dumps(report,indent=2)+"\n")

if __name__=="__main__": build()
