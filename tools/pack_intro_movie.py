"""Convert a v1 PNG key/XOR intro into one resumable lossless DW2MOV1 movie.

Requires Pillow with animated WebP support and NumPy. Each independent job reads
at most its nearest old key plus 30 source slots, checks source RGBA hashes,
encodes one <=30-slot chunk, and verifies its expanded visual spans once.
"""
from __future__ import annotations

import argparse
from concurrent.futures import ProcessPoolExecutor, as_completed
import ctypes
from ctypes import wintypes
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import struct
import sys
import time

import numpy as np
from PIL import Image, _webp, features
import PIL

HEADER = struct.Struct("<8s6I")
INDEX = struct.Struct("<IIQQ")
SETTINGS = dict(algorithmVersion=1, format="DW2MOV1", chunkSlots=30, lossless=True,
                quality=75, method=4, kmin=16, kmax=30, minimize_size=False,
                allow_mixed=False, timestampQuantization="round(slot*1000/30)")
CONTEXT: dict = {}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def file_digest(path):
    result = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024*1024), b""):
            result.update(block)
    return result.hexdigest()


def atomic(path, payload):
    path = Path(path)
    temporary = path.with_name(path.name+".partial")
    temporary.write_bytes(payload)
    os.replace(temporary, path)


def write_json(path, value):
    atomic(path, (json.dumps(value, indent=2)+"\n").encode())


def peak_memory():
    if os.name == "nt":
        class Counters(ctypes.Structure):
            _fields_ = [("cb",wintypes.DWORD),("faults",wintypes.DWORD)]+[
                (name,ctypes.c_size_t) for name in ("peak","working","pagedPeak","paged","nonpagedPeak","nonpaged","pagefile","pagefilePeak")]
        value=Counters();value.cb=ctypes.sizeof(value)
        process=ctypes.windll.kernel32.GetCurrentProcess
        process.argtypes=[];process.restype=wintypes.HANDLE
        query=ctypes.windll.psapi.GetProcessMemoryInfo
        query.argtypes=[wintypes.HANDLE,ctypes.POINTER(Counters),wintypes.DWORD];query.restype=wintypes.BOOL
        if not query(process(),ctypes.byref(value),value.cb):raise ctypes.WinError()
        return value.peak
    import resource
    value=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    return int(value if sys.platform=="darwin" else value*1024)


def source_manifest(source):
    source=Path(source).resolve()
    table=(source/"frames.tsv").read_bytes();hash_bytes=(source/"frame-hashes.json").read_bytes()
    lines=table.decode().splitlines()
    if len(lines)<4 or lines[0]!="DeadWarrior2-Intro 1" or lines[2]!="music.wav":
        raise ValueError("Source must be a complete v1 PNG key/XOR intro")
    width,height,rate,count,first,last=map(int,lines[1].split())
    if width<=0 or height<=0 or rate!=30 or count!=last-first+1 or count<=0:
        raise ValueError("Invalid source dimensions/rate/range")
    hashes=json.loads(hash_bytes)
    if set(hashes)!={str(frame) for frame in range(first,last+1)}:
        raise ValueError("Source RGBA hash list differs from the complete range")
    records=[];key=-1
    for index,line in enumerate(lines[3:]):
        source_frame,kind,relative=line.split();source_frame=int(source_frame)
        expected=f"frames/f{source_frame:06}.png"
        if source_frame!=first+index or kind not in ("K","D") or relative!=expected:
            raise ValueError("Invalid source frame order/kind/path")
        if kind=="K":key=index
        if key<0 or index-key>29:raise ValueError("Source key interval exceeds 30 slots")
        if not (source/relative).is_file():raise ValueError(f"Source PNG is missing: {relative}")
        records.append((source_frame,kind,relative,key))
    if len(records)!=count:raise ValueError("Source frame count differs")
    identity=dict(settings=SETTINGS,pillow=PIL.__version__,libwebp=features.version("webp"),
                  sourceTableSHA256=digest(table),sourceRgbaHashListSHA256=digest(hash_bytes),
                  width=width,height=height,rate=rate,count=count,first=first,last=last)
    return dict(source=str(source),records=records,hashes=hashes,identity=identity,
                identitySHA256=digest(json.dumps(identity,sort_keys=True).encode()))


def timestamp(slot):
    return (slot*1000+15)//30


def lossless_chunks(data):
    if data[:4]!=b"RIFF" or data[8:12]!=b"WEBP" or len(data)!=struct.unpack_from("<I",data,4)[0]+8:
        raise ValueError("Invalid encoded WebP RIFF")
    found=0;offset=12
    while offset<len(data):
        tag=data[offset:offset+4];size=struct.unpack_from("<I",data,offset+4)[0]
        payload=data[offset+8:offset+8+size]
        if tag==b"VP8 ":raise ValueError("Lossy frame was emitted")
        if tag==b"VP8L":found+=1
        if tag==b"ANMF":
            nested=16
            while nested<len(payload):
                child=payload[nested:nested+4];length=struct.unpack_from("<I",payload,nested+4)[0]
                if child==b"VP8 ":raise ValueError("Lossy animation frame was emitted")
                if child==b"VP8L":found+=1
                nested+=8+length+(length&1)
            if nested!=len(payload):raise ValueError("Invalid animation image chunks")
        offset+=8+size+(size&1)
    if offset!=len(data) or not found:raise ValueError("No complete lossless image payload")
    return found


def verify_webp(data, expected_hashes, width, height):
    """Decode each visual frame once and expand its span over 30 Hz slots."""
    encoded_lossless=lossless_chunks(data)
    decoder=_webp.WebPAnimDecoder(data)
    dimensions,loop,background,visual_count,mode=decoder.get_info()
    count=len(expected_hashes)
    if dimensions!=(width,height) or mode not in ("RGBA","RGBX") or not 1<=visual_count<=count:
        raise ValueError("Encoded WebP canvas/visual count differs")
    if encoded_lossless!=visual_count:raise ValueError("Lossless payload/visual count differs")
    boundaries={timestamp(slot) for slot in range(1,count+1)}
    slot=0;previous_end=0
    for visual in range(visual_count):
        rgba,end=decoder.get_next()
        if visual_count==1 and end==0:end=timestamp(count)
        if len(rgba)!=width*height*4 or end<=previous_end or end not in boundaries:
            raise ValueError("Encoded visual span does not match source slots")
        rgba_hash=digest(rgba)
        while slot<count and timestamp(slot)<end:
            if rgba_hash!=expected_hashes[slot]:raise ValueError(f"Encoded RGBA differs at local source slot {slot}")
            slot+=1
        previous_end=end
    if slot!=count or previous_end!=timestamp(count):raise ValueError("Encoded WebP duration/slot coverage differs")
    return dict(verifiedExpandedRgbaSlots=count,encodedVisualFrames=visual_count,finalTimestampMs=previous_end,
                decodedVisualFramesOnce=True,allPayloadsLossless=True)


def initialize_worker(context):
    global CONTEXT
    CONTEXT=context


def cache_paths(context, chunk):
    directory=Path(context["cache"])
    return directory/f"chunk-{chunk:06}.webp",directory/f"chunk-{chunk:06}.json"


def encode_chunk(chunk):
    context=CONTEXT;identity=context["identity"];records=context["records"]
    first=chunk*30;count=min(30,len(records)-first);last=first+count
    start=records[first][3];source=Path(context["source"])
    encoder=_webp.WebPAnimEncoder((identity["width"],identity["height"]),0xffffffff,0,False,16,30,False,False)
    began=time.monotonic();cpu=time.process_time();encode_seconds=0;previous=None
    source_files={};expected=[]
    for index in range(start,last):
        frame,kind,relative,key=records[index]
        raw=(source/relative).read_bytes();source_files[relative]=digest(raw)
        with Image.open(io.BytesIO(raw)) as image:
            if image.format!="PNG" or image.size!=(identity["width"],identity["height"]) or image.mode not in ("RGB","RGBA"):
                raise ValueError(f"Invalid source PNG: {relative}")
            current=np.array(image.convert("RGBA"))
        if np.any(current[:,:,3]!=255):raise ValueError(f"Non-opaque source PNG: {relative}")
        if kind=="D":
            if previous is None:raise ValueError("Missing old source key")
            np.bitwise_xor(previous[:,:,:3],current[:,:,:3],out=previous[:,:,:3])
            del current
        else:previous=current;del current
        rgba_hash=digest(memoryview(previous))
        if rgba_hash!=context["hashes"][str(frame)]:raise ValueError(f"Source reconstruction differs at frame {frame}")
        if index>=first:
            expected.append(rgba_hash)
            image=Image.fromarray(previous)
            before=time.monotonic()
            encoder.add(image.getim(),timestamp(index-first),True,75,100,4)
            encode_seconds+=time.monotonic()-before
    before=time.monotonic()
    encoder.add(None,timestamp(count),True,75,100,0)
    encoded=encoder.assemble("","","")
    encode_seconds+=time.monotonic()-before
    del encoder,previous,image
    before=time.monotonic();verified=verify_webp(encoded,expected,identity["width"],identity["height"])
    verify_seconds=time.monotonic()-before
    payload,sidecar=cache_paths(context,chunk)
    record=dict(chunk=chunk,firstIndex=first,count=count,firstSource=records[first][0],lastSource=records[last-1][0],
        sourceKeyIndex=start,sourcePrerollSlots=first-start,identitySHA256=context["identitySHA256"],
        sourceFilesSHA256=source_files,expandedRgbaHashes=expected,payloadBytes=len(encoded),payloadSHA256=digest(encoded),
        encodeSeconds=encode_seconds,verifySeconds=verify_seconds,elapsedSeconds=time.monotonic()-began,
        processCpuSeconds=time.process_time()-cpu,workerPid=os.getpid(),workerProcessPeakWorkingSetBytes=peak_memory(),**verified)
    atomic(payload,encoded);write_json(sidecar,record)
    return record


def approved_cache(context,chunk):
    payload,sidecar=cache_paths(context,chunk)
    if not payload.is_file() or not sidecar.is_file():return None
    try:
        record=json.loads(sidecar.read_bytes());first=chunk*30;count=min(30,len(context["records"])-first)
        if record["identitySHA256"]!=context["identitySHA256"] or record["chunk"]!=chunk or record["firstIndex"]!=first or record["count"]!=count:
            return None
        if record["verifiedExpandedRgbaSlots"]!=count or not record["decodedVisualFramesOnce"] or not record["allPayloadsLossless"]:
            return None
        if record["payloadBytes"]!=payload.stat().st_size or record["payloadSHA256"]!=file_digest(payload):return None
        expected=[context["hashes"][str(context["records"][index][0])] for index in range(first,first+count)]
        if record["expandedRgbaHashes"]!=expected:return None
        source_files=record["sourceFilesSHA256"]
        wanted={context["records"][i][2] for i in range(context["records"][first][3],first+count)}
        if set(source_files)!=wanted:return None
        if any(file_digest(Path(context["source"])/name)!=value for name,value in source_files.items()):return None
        return record
    except (OSError,ValueError,KeyError,TypeError):return None


def safe_file(source, relative):
    path=PurePosixPath(relative)
    if path.is_absolute() or ".." in path.parts:raise ValueError("Invalid source audio path")
    result=(source/relative).resolve()
    if not result.is_relative_to(source):raise ValueError("Source audio escapes directory")
    return result


def assemble(context, output, records, workers, elapsed, resumed):
    identity=context["identity"];count=identity["count"]
    if sum(row["count"] for row in records)!=count:raise ValueError("Chunk index does not cover the full intro")
    target=output/"movie.dw2v";temporary=output/"movie.dw2v.partial"
    offset=HEADER.size+INDEX.size*len(records);index=[]
    for record in records:
        index.append((record["firstIndex"],record["count"],offset,record["payloadBytes"]))
        offset+=record["payloadBytes"]
    with temporary.open("wb") as stream:
        stream.write(HEADER.pack(b"DW2MOV1\0",identity["width"],identity["height"],30,count,identity["first"],len(records)))
        for entry in index:stream.write(INDEX.pack(*entry))
        for record in records:
            path,_=cache_paths(context,record["chunk"])
            if file_digest(path)!=record["payloadSHA256"]:raise ValueError("Verified chunk changed before assembly")
            with path.open("rb") as payload:shutil.copyfileobj(payload,stream,1024*1024)
    if temporary.stat().st_size!=offset:raise ValueError("Assembled movie length differs")
    os.replace(temporary,target)
    source=Path(context["source"])
    copied={}
    relatives=["music.wav","events.tsv","audio-report.json","frame-hashes.json"]
    relatives.extend(path.relative_to(source).as_posix() for path in sorted((source/"audio").rglob("*")) if path.is_file())
    for relative in relatives:
        old=safe_file(source,relative);new=output/relative;new.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(old,new)
        before=file_digest(old)
        if file_digest(new)!=before:raise ValueError(f"Audio/hash copy changed bytes: {relative}")
        copied[relative]=dict(bytes=old.stat().st_size,sha256=before)
    raw_bytes=sum((source/row[2]).stat().st_size for row in context["records"])
    movie_bytes=target.stat().st_size
    result=dict(format="DW2MOV1",width=identity["width"],height=identity["height"],fps=30,count=count,
        firstSource=identity["first"],lastSource=identity["last"],chunkCount=len(records),maximumSlotsPerChunk=30,
        encodedVisualFrames=sum(row["encodedVisualFrames"] for row in records),
        verifiedExpandedRgbaFrames=sum(row["verifiedExpandedRgbaSlots"] for row in records),
        allChunksDecodedOnceAndRgbaHashesVerified=True,oldPackedPngBytes=raw_bytes,movieBytes=movie_bytes,
        byteReductionPercent=100*(1-movie_bytes/raw_bytes),movieSHA256=file_digest(target),sourceIdentity=identity,
        sourceIdentitySHA256=context["identitySHA256"],copiedFilesByteIdentical=copied,
        parallelWorkers=workers,resumedApprovedChunks=resumed,elapsedThisInvocationSeconds=elapsed,
        sumChunkEncodingSeconds=sum(row["encodeSeconds"] for row in records),sumChunkWorkerCpuSeconds=sum(row["processCpuSeconds"] for row in records),
        mainProcessPeakWorkingSetBytes=peak_memory(),maximumWorkerProcessPeakWorkingSetBytes=max(row["workerProcessPeakWorkingSetBytes"] for row in records),
        memoryScope="Process peaks, not sampled simultaneous total: each worker includes Python/Pillow/NumPy, at most two source reconstruction raster arrays, libwebp encoder/decoder scratch/canvases, and one compressed chunk. The main process streams assembly without decoded frames. No full-movie RGBA cache; additional codec allocations are included in measured process peaks, not bounded by the two-array statement.",
        runtimeAssetsHaveIndividualPngFrames=False,frameTableVersion=2,
        nativeFullPlaybackVerified=False)
    write_json(output/"movie-report.json",result)
    table=f"DeadWarrior2-Intro 2\n{identity['width']} {identity['height']} 30 {count} {identity['first']} {identity['last']}\nmusic.wav\nmovie.dw2v\n"
    # Readiness marker is published only after every chunk and copied file passes.
    atomic(output/"frames.tsv",table.encode())
    return result


def run(args):
    began=time.monotonic();source=args.source.resolve();output=args.output.resolve();cache=args.cache.resolve()
    if source==output or source.is_relative_to(output) or output.is_relative_to(source) or cache==source or cache.is_relative_to(source):
        raise ValueError("Output/cache cannot overwrite or contain the source intro")
    if cache==output or cache.is_relative_to(output):raise ValueError("Temporary chunks must stay outside runtime intro assets")
    allowed={"movie.dw2v","movie.dw2v.partial","movie-report.json","movie-report.json.partial","frame-hashes.json","audio-report.json","events.tsv","music.wav","audio","frames.tsv","frames.tsv.partial"}
    if output.exists() and any(path.name not in allowed for path in output.iterdir()):
        raise ValueError("Output contains unrelated/legacy files; choose an independent empty output directory")
    output.mkdir(parents=True,exist_ok=True);cache.mkdir(parents=True,exist_ok=True)
    context=source_manifest(source);context["cache"]=str(cache)
    identity=context["identity"]
    if args.expected_table_sha and identity["sourceTableSHA256"]!=args.expected_table_sha:raise ValueError("Canonical source frame table SHA differs")
    if args.expected_hash_sha and identity["sourceRgbaHashListSHA256"]!=args.expected_hash_sha:raise ValueError("Canonical source RGBA hash-list SHA differs")
    total=(identity["count"]+29)//30;approved={};pending=[]
    for chunk in range(total):
        old=approved_cache(context,chunk)
        if old is None:pending.append(chunk)
        else:approved[chunk]=old
    resumed=len(approved)
    print(f"Source {identity['count']} slots, {total} chunks; approved resume {resumed}; pending {len(pending)}; workers {args.workers}",flush=True)
    def progress():
        write_json(cache/"resume-index.json",dict(identity=context["identity"],identitySHA256=context["identitySHA256"],
            completeChunks=sorted(approved),chunkRecords=[approved[key] for key in sorted(approved)]))
    progress()
    if pending:
        pool=ProcessPoolExecutor(max_workers=args.workers,initializer=initialize_worker,initargs=(context,))
        futures={pool.submit(encode_chunk,chunk):chunk for chunk in pending}
        try:
            for future in as_completed(futures):
                row=future.result();approved[row["chunk"]]=row;progress()
                if len(approved)%10==0 or len(approved) in (1,total):
                    print(f"Approved {len(approved)}/{total}; wall {time.monotonic()-began:.1f}s; chunk {row['chunk']} slots {row['count']} visuals {row['encodedVisualFrames']}; encoder {row['encodeSeconds']:.2f}s; workerCPU sum {sum(r['processCpuSeconds'] for r in approved.values()):.1f}s",flush=True)
        except BaseException:
            pool.shutdown(wait=True,cancel_futures=True)
            raise
        else:pool.shutdown(wait=True)
    # Re-read source identity before publishing anything runtime-ready.
    if source_manifest(source)["identitySHA256"]!=context["identitySHA256"]:raise ValueError("Source metadata changed during conversion")
    rows=[approved[chunk] for chunk in range(total)]
    result=assemble(context,output,rows,args.workers,time.monotonic()-began,resumed)
    print(json.dumps({key:value for key,value in result.items() if key not in ("copiedFilesByteIdentical","sourceIdentity")},indent=2),flush=True)
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--cache",type=Path,required=True)
    parser.add_argument("--workers",type=int,choices=(1,2),default=2)
    parser.add_argument("--expected-table-sha")
    parser.add_argument("--expected-hash-sha")
    run(parser.parse_args())


if __name__=="__main__":main()
