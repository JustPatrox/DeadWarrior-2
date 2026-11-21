"""Export denser original giant textures without changing logical bounds/timing."""
import argparse
import csv
from pathlib import Path
from build_menu_assets import ArtCompiler, ROOT
from build_giants_assets import freeze_bag
from build_archidon_assets import table


def build(output, density):
    c=ArtCompiler(output/'giants',loadout_overrides={2121:'Wrapped Helmet'})
    freeze_bag(c)
    count=0
    for folder,ids in [('giants',{6465,6550,6730,6815,7024,7388,7519,7525}),('bomber',{7514})]:
        c.directory=output/folder;c.directory.mkdir(parents=True,exist_ok=True)
        with (ROOT/'assets'/folder/'textures.tsv').open(encoding='utf-8') as source:
            rows=[r for r in csv.reader(source,delimiter='\t') if int(r[0]) in ids]
        c.supersample(rows,density)
        table(c.directory/'textures.tsv',rows)
        count+=len(rows)
        print(folder,len(rows),'original vector frames at',density,'x density',flush=True)
    print('Prepared',count,'frames; logical bounds and animation manifests unchanged',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,default=ROOT/'work/giant-hires-assets')
    parser.add_argument('--density',type=int,default=3,choices=(2,3,4))
    args=parser.parse_args();build(args.output,args.density)
