"""Pack prepared multi-frame sprites into lossless, extruded PNG sheets.

Static symbols and background extension variants retain their original PNGs.
The campaign/trailer SWFs are not read or modified by this conversion.
"""
import argparse
from collections import defaultdict
import csv
from dataclasses import dataclass, field
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import time

from PIL import Image
import numpy as np


EXCLUDED_FOLDERS = {'intro', 'branding', 'fonts'}
# The original tutorial export predates layer.<id>.width metadata. These are
# its two static terrain/parallax layers (CampaignTutorial.backgroundLayers).
LEGACY_BACKGROUND_SYMBOLS = {4098, 4102}
PADDING = 2
STANDARD_EDGE = 2048
LARGE_EDGE = 4096
LARGE_CELL_THRESHOLD = 1024


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024*1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def relative_file(folder, name):
    """Reject escaping paths, including drive names and escaping junctions."""
    relative = PurePosixPath(name.replace('\\', '/'))
    if (not name or relative.is_absolute() or '..' in relative.parts
            or any(':' in part for part in relative.parts)):
        raise ValueError(f'Unsafe texture path: {name}')
    path = (folder/Path(*relative.parts)).resolve(strict=True)
    if not path.is_relative_to(folder.resolve()):
        raise ValueError(f'Texture path escapes its manifest folder: {name}')
    if not path.is_file():
        raise ValueError(f'Texture is not a file: {name}')
    return path


def rgba(path):
    with Image.open(path) as source:
        source.load()
        return source.convert('RGBA')


def pixel_hash(width, height, pixels):
    digest = hashlib.sha256(struct.pack('>II', width, height))
    digest.update(pixels)
    return digest.hexdigest()


def background_symbols(source):
    symbols = set(LEGACY_BACKGROUND_SYMBOLS)
    for path in sorted(source.rglob('rules.tsv')):
        for line in path.read_text(encoding='utf-8').splitlines():
            match = re.match(r'^layer\.(\d+)\.width\s', line)
            if match:
                symbols.add(int(match[1]))
    return symbols


@dataclass
class Cell:
    file: str
    width: int
    height: int
    pixels_sha256: str
    first_row: int
    files: list[str] = field(default_factory=list)
    page: int = -1
    x: int = 0
    y: int = 0


@dataclass
class Page:
    edge: int
    shelves: list = field(default_factory=list)
    cells: list = field(default_factory=list)
    width: int = 0
    height: int = 0

    def fit(self, cell, commit=False):
        width, height = cell.width+PADDING*2, cell.height+PADDING*2
        options = [(self.edge-shelf[2]-width, shelf[1]-height, i)
                   for i, shelf in enumerate(self.shelves)
                   if shelf[1] >= height and shelf[2]+width <= self.edge]
        if options:
            unused, vertical_unused, index = min(options)
            shelf = self.shelves[index]
            x, y = shelf[2], shelf[0]
            if commit:
                shelf[2] += width
        else:
            y = self.shelves[-1][0]+self.shelves[-1][1] if self.shelves else 0
            if y+height > self.edge:
                return None
            x = 0
            if commit:
                self.shelves.append([y, height, width])
        if commit:
            self.width = max(self.width, x+width)
            self.height = max(self.height, y+height)
            self.cells.append(cell)
        return x, y


def pack_cells(cells):
    pages = []
    ordered = sorted(cells, key=lambda c: (-max(c.width, c.height), -c.width*c.height,
                                          -c.height, -c.width, c.first_row, c.file))
    for cell in ordered:
        required = max(cell.width, cell.height)+PADDING*2
        # A large two-dimensional frame cannot share a 2048 page with a peer.
        # Promote it so multiple unscaled map frames can occupy one 4096 sheet.
        large_in_both_axes = (cell.width+PADDING*2 > LARGE_CELL_THRESHOLD
                              and cell.height+PADDING*2 > LARGE_CELL_THRESHOLD)
        edge = LARGE_EDGE if required > STANDARD_EDGE or large_in_both_axes else STANDARD_EDGE
        if required > LARGE_EDGE:
            raise ValueError(f'Animated sprite needs an explicitly reviewed larger page: '
                             f'{cell.file} ({cell.width}x{cell.height} plus padding)')
        selected = next((i for i, page in enumerate(pages)
                         if page.edge == edge and page.fit(cell) is not None), None)
        if selected is None:
            selected = len(pages)
            pages.append(Page(edge))
        cell.page = selected
        cell.x, cell.y = pages[selected].fit(cell, commit=True)
    return pages


def paste_extruded(atlas, image, x, y):
    width, height = image.size
    px, py = x+PADDING, y+PADDING
    atlas.paste(image, (px, py))
    nearest = Image.Resampling.NEAREST
    atlas.paste(image.crop((0, 0, width, 1)).resize((width, PADDING), nearest), (px, y))
    atlas.paste(image.crop((0, height-1, width, height)).resize((width, PADDING), nearest), (px, py+height))
    atlas.paste(image.crop((0, 0, 1, height)).resize((PADDING, height), nearest), (x, py))
    atlas.paste(image.crop((width-1, 0, width, height)).resize((PADDING, height), nearest), (px+width, py))
    for cx, cy, sx, sy in ((x, y, 0, 0), (px+width, y, width-1, 0),
                          (x, py+height, 0, height-1), (px+width, py+height, width-1, height-1)):
        atlas.paste(image.crop((sx, sy, sx+1, sy+1)).resize((PADDING, PADDING), nearest), (cx, cy))


def file_inventory(folder):
    paths = sorted(p for p in folder.rglob('*') if p.is_file())
    return paths, sum(p.stat().st_size for p in paths)


def convert_folder(source, output, name, backgrounds):
    folder = relative_file(source, name+'/textures.tsv').parent
    target = output/name
    if target.exists():
        raise ValueError(f'Output folder already exists; refusing to overwrite: {target}')
    with (folder/'textures.tsv').open(encoding='utf-8', newline='') as stream:
        rows = list(csv.reader(stream, delimiter='\t'))
    seen = set()
    by_symbol = defaultdict(list)
    for index, row in enumerate(rows):
        if len(row) != 7:
            raise ValueError(f'Expected seven source columns at {name}:{index+1}')
        key = int(row[0]), int(row[1])
        if key in seen:
            raise ValueError(f'Duplicate texture key at {name}: {key}')
        seen.add(key)
        if not all(math.isfinite(float(value)) for value in row[3:]):
            raise ValueError(f'Nonfinite original texture bounds at {name}:{index+1}')
        relative_file(folder, row[2])
        by_symbol[key[0]].append(index)
    multi = {symbol for symbol, indices in by_symbol.items() if len(indices) > 1}
    excluded = multi & backgrounds
    animated = multi-excluded
    animated_rows = {index for symbol in animated for index in by_symbol[symbol]}
    files = sorted({rows[index][2] for index in animated_rows})
    cells = []
    by_file, by_pixels = {}, {}
    file_first_row = {}
    for index in sorted(animated_rows):
        file_first_row.setdefault(rows[index][2], index)
    source_details = {}
    for name_in_manifest in files:
        path = relative_file(folder, name_in_manifest)
        image = rgba(path)
        width, height = image.size
        pixels = image.tobytes()
        digest = pixel_hash(width, height, pixels)
        source_details[name_in_manifest] = dict(width=width, height=height, rgbaSHA256=digest,
                                               sourcePngSHA256=sha256_file(path), bytes=path.stat().st_size)
        # Hashing includes dimensions and every RGBA byte, including transparent
        # RGB values. The later decoded-output comparison verifies each row.
        cell = by_pixels.get(digest)
        if cell is None:
            cell = Cell(name_in_manifest, width, height, digest, file_first_row[name_in_manifest])
            cells.append(cell);by_pixels[digest] = cell
        cell.files.append(name_in_manifest)
        by_file[name_in_manifest] = cell
        image.close()
    pages = pack_cells(cells)
    target.mkdir(parents=True)
    atlas_dir = target/'atlases'
    if pages:
        atlas_dir.mkdir()
    page_reports = []
    for index, page in enumerate(pages):
        atlas = Image.new('RGBA', (page.width, page.height), (0, 0, 0, 0))
        for cell in page.cells:
            image = rgba(relative_file(folder, cell.file))
            paste_extruded(atlas, image, cell.x, cell.y)
            image.close()
        filename = f'atlases/sheet-{index:04}.png'
        path = target/filename
        atlas.save(path, format='PNG', compress_level=6)
        atlas.close()
        page_reports.append(dict(file=filename, width=page.width, height=page.height, pageLimit=page.edge,
                                 cells=len(page.cells), bytes=path.stat().st_size, sha256=sha256_file(path)))

    converted = []
    row_proofs = []
    for index, row in enumerate(rows):
        values = row.copy()
        if index in animated_rows:
            cell = by_file[row[2]];page = pages[cell.page]
            values[2] = page_reports[cell.page]['file']
            uv = ((cell.x+PADDING)/page.width, (cell.y+PADDING)/page.height,
                  cell.width/page.width, cell.height/page.height)
        else:
            uv = (0., 0., 1., 1.)
        values.extend(format(value, '.17g') for value in uv)
        converted.append(values)

    # Verify the actual encoded PNG and serialized normalized coordinates once.
    # Every output texture row is linked to its source pixel hash and dimensions.
    indices_by_page = defaultdict(list)
    for index in sorted(animated_rows):
        indices_by_page[by_file[rows[index][2]].page].append(index)
    for page_index, indices in sorted(indices_by_page.items()):
        atlas = rgba(target/page_reports[page_index]['file'])
        verified_files = {}
        for index in indices:
            source_row, row = rows[index], converted[index]
            detail = source_details[source_row[2]]
            width, height = detail['width'], detail['height']
            u, v, du, dv = map(float, row[7:])
            x, y, w, h = (round(u*atlas.width), round(v*atlas.height),
                          round(du*atlas.width), round(dv*atlas.height))
            if (w, h) != (width, height):
                raise ValueError(f'Normalized atlas extent changed source dimensions: {name}/{source_row[2]}')
            rectangle = [x, y, w, h]
            if source_row[2] in verified_files:
                # A castle frame can have several logical aliases referencing
                # exactly this source PNG and rectangle. Verify the reference,
                # without decoding/reconstructing the same pixels again.
                if verified_files[source_row[2]] != rectangle:
                    raise ValueError(f'Reused source has inconsistent atlas coordinates: {name}/{source_row[2]}')
                digest = detail['rgbaSHA256']
            else:
                source_image = rgba(relative_file(folder, source_row[2]))
                pixels = source_image.tobytes()
                crop = atlas.crop((x, y, x+w, y+h))
                if crop.tobytes() != pixels:
                    raise ValueError(f'Atlas RGBA differs from source: {name}/{source_row[2]}')
                digest = pixel_hash(width, height, pixels)
                if digest != detail['rgbaSHA256']:
                    raise ValueError(f'Source PNG changed during packing: {name}/{source_row[2]}')
                # Independent edge-repeat construction checks extrusion rather
                # than using the packing function to reproduce its own output.
                padded = atlas.crop((x-PADDING, y-PADDING, x+w+PADDING, y+h+PADDING))
                expected = np.pad(np.asarray(source_image), ((PADDING, PADDING), (PADDING, PADDING), (0, 0)), mode='edge')
                if padded.tobytes() != expected.tobytes():
                    raise ValueError(f'Atlas extrusion differs from source edges: {name}/{source_row[2]}')
                verified_files[source_row[2]] = rectangle
                source_image.close();crop.close();padded.close()
            row_proofs.append(dict(symbol=int(source_row[0]), frame=int(source_row[1]), sourceFile=source_row[2],
                                   atlasFile=row[2], rectangle=[x, y, w, h], rgbaSHA256=digest))
        atlas.close()

    # Keep shared PNGs used by a static row, every nonreferenced file, and every
    # non-PNG resource. Omit only PNGs whose references were all replaced above.
    references = defaultdict(list)
    for index, row in enumerate(rows):
        references[relative_file(folder, row[2]).relative_to(folder).as_posix()].append(index)
    omit = {name for name, indices in references.items()
            if Path(name).suffix.lower() == '.png' and all(index in animated_rows for index in indices)}
    input_paths, input_bytes = file_inventory(folder)
    preserved = []
    for path in input_paths:
        resolved = path.resolve()
        if not resolved.is_relative_to(folder):
            raise ValueError(f'Source entry escapes its folder: {path}')
        relative = path.relative_to(folder).as_posix()
        if relative in omit or relative == 'textures.tsv':
            continue
        destination = target/relative
        if destination.exists():
            raise ValueError(f'Generated atlas collides with an original source file: {relative}')
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, destination)
        if sha256_file(path) != sha256_file(destination):
            raise ValueError(f'Copied static/auxiliary resource changed: {relative}')
        preserved.append(relative)
    with (target/'textures.tsv').open('w', encoding='utf-8', newline='') as stream:
        csv.writer(stream, delimiter='\t', lineterminator='\n').writerows(converted)
    output_paths, output_bytes = file_inventory(target)
    return dict(folder=name, inputManifestSHA256=sha256_file(folder/'textures.tsv'),
                outputManifestSHA256=sha256_file(target/'textures.tsv'), sourceRows=len(rows),
                animatedSymbols=sorted(animated), animatedRows=len(animated_rows), staticRows=len(rows)-len(animated_rows),
                staticBackgroundVariantsExcluded=sorted(excluded),
                staticBackgroundRowsExcluded=sum(len(by_symbol[symbol]) for symbol in excluded),
                originalAnimatedPngFiles=len(files), uniqueRgbaCells=len(cells), identicalRgbaFilesDeduplicated=len(files)-len(cells),
                atlasFiles=len(pages), atlases=page_reports, inputFiles=len(input_paths), outputFiles=len(output_paths),
                packingPolicy='adaptive-2048-or-4096', largeCellThresholdIncludingPadding=LARGE_CELL_THRESHOLD,
                promotedLargeCells=sum(cell.width+PADDING*2 > LARGE_CELL_THRESHOLD
                                       and cell.height+PADDING*2 > LARGE_CELL_THRESHOLD for cell in cells),
                inputBytes=input_bytes, outputBytes=output_bytes, bytesSaved=input_bytes-output_bytes,
                replacedSourcePngFiles=sorted(omit), preservedFiles=preserved,
                animatedCropRgbaVerified=len(row_proofs), extrusionVerified=True,
                uniqueSourceCropsVerified=len(files), reusedVerifiedRowReferences=len(row_proofs)-len(files),
                originalLogicalColumnsPreserved=True, rows=row_proofs,
                resampling=False, paletteConversion=False, lossyCompression=False)


def build(source, output, folders=None):
    source = source.resolve(strict=True)
    output = output.resolve()
    if not source.is_dir() or output == source or output.is_relative_to(source) or source.is_relative_to(output):
        raise ValueError('Source and output must be separate, nonnested directories')
    names = folders if folders is not None else [p.parent.relative_to(source).as_posix()
                                                for p in sorted(source.glob('*/textures.tsv'))
                                                if p.parent.name not in EXCLUDED_FOLDERS]
    if len(names) != len(set(names)):
        raise ValueError('A folder was requested more than once')
    for name in names:
        if name in EXCLUDED_FOLDERS or PurePosixPath(name).name in EXCLUDED_FOLDERS:
            raise ValueError(f'Non-sprite folder is excluded: {name}')
        relative_file(source, name+'/textures.tsv')
        if (output/name).exists():
            raise ValueError(f'Output folder exists: {output/name}')
    backgrounds = background_symbols(source)
    report = dict(format='DeadWarrior2-SpriteSheets1', source=str(source), output=str(output),
                  padding=PADDING, defaultPageLimit=STANDARD_EDGE, largePageLimit=LARGE_EDGE,
                  normalizedCoordinates=True, excludedFolders=sorted(EXCLUDED_FOLDERS),
                  backgroundSymbolsFromSourceRules=sorted(backgrounds), folders=[])
    report['packingPolicy'] = '4096 if a padded cell exceeds 2048 on either axis or exceeds 1024 on both axes; otherwise 2048'
    report['legacyStaticBackgroundSymbolsWithoutLayerRules'] = sorted(LEGACY_BACKGROUND_SYMBOLS)
    started = time.perf_counter()
    for name in names:
        result = convert_folder(source, output, name, backgrounds)
        report['folders'].append(result)
        print(f"{name}: {result['animatedRows']} rows -> {result['atlasFiles']} atlases; "
              f"{result['animatedCropRgbaVerified']} RGBA crops verified; "
              f"{result['inputFiles']} -> {result['outputFiles']} files", flush=True)
    report['seconds'] = time.perf_counter()-started
    report['allAnimatedRowsVerified'] = all(f['animatedRows'] == f['animatedCropRgbaVerified'] for f in report['folders'])
    output.parent.mkdir(parents=True, exist_ok=True)
    report_path = output.parent/(output.name+'-sprite-sheets-report.json')
    report_path.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f'Report: {report_path}', flush=True)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='Clean prepared assets directory')
    parser.add_argument('--output', type=Path, required=True, help='New separate overlay assets directory')
    parser.add_argument('--folder', action='append', help='Convert only this texture-manifest folder; repeatable')
    args = parser.parse_args()
    build(args.source, args.output, args.folder)


if __name__ == '__main__':
    main()
