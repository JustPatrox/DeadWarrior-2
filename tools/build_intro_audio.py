"""Recover the supplied trailer's root audio without mixing music and effects.

SeekSamples is applied at the MP3's native sample rate before conversion to
44.1 kHz. The generated WAVs are PCM16 stereo; mono duplication adds no gain.
This builder owns only music.wav, audio/*.wav, events.tsv and audio-report.json.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import wave
import zlib

import imageio_ffmpeg
import numpy as np
from inventory import Reader

ROOT=Path(__file__).resolve().parents[1]
EXPECTED_SHA256='a2a1bc496bb9e784fa4f5650c0368e0c3faf0bfda3fa3350979aa9f4651ef4ac'
SAMPLE_RATE=44100
FIRST_FRAME=3
REFERENCES={
    'adobeSWF19':'https://open-flash.github.io/mirrors/swf-spec-19.pdf',
    'jpexsNativeSeek':'https://raw.githubusercontent.com/jindrapetrik/jpexs-decompiler/master/libsrc/ffdec_lib/src/com/jpexs/decompiler/flash/types/sound/SoundFormat.java',
    'jpexsNativeMp3Rate':'https://raw.githubusercontent.com/jindrapetrik/jpexs-decompiler/master/libsrc/ffdec_lib/src/com/jpexs/decompiler/flash/types/sound/MP3Decoder.java',
}


def tags(data):
    offset=0
    while offset<len(data):
        word=struct.unpack_from('<H',data,offset)[0];offset+=2
        code,size=word>>6,word&63
        if size==63:size=struct.unpack_from('<I',data,offset)[0];offset+=4
        assert offset+size<=len(data),'Truncated SWF tag'
        yield code,data[offset:offset+size]
        offset+=size
        if code==0:break


def sound_info(data,frame,owner=0):
    sound,flags=struct.unpack_from('<HB',data);offset=3
    event=dict(frame=frame,id=sound,owner=owner,flags=flags,loops=1,inPoint=None,outPoint=None,envelope=[])
    for bit,name,size,fmt in ((1,'inPoint',4,'<I'),(2,'outPoint',4,'<I'),(4,'loops',2,'<H')):
        if flags&bit:event[name]=struct.unpack_from(fmt,data,offset)[0];offset+=size
    if flags&8:
        count=data[offset];offset+=1
        for _ in range(count):
            position,left,right=struct.unpack_from('<IHH',data,offset);offset+=8
            event['envelope'].append(dict(position44=position,left=left,right=right))
    assert offset==len(data),'Unparsed SOUNDINFO data'
    return event


def inspect(source):
    raw=source.read_bytes();digest=hashlib.sha256(raw).hexdigest()
    assert digest==EXPECTED_SHA256,'This builder expects the supplied, unmodified trailer SWF'
    assert raw[:3] in (b'CWS',b'FWS')
    body=zlib.decompress(raw[8:]) if raw[:3]==b'CWS' else raw[8:]
    assert len(body)+8==struct.unpack_from('<I',raw,4)[0]
    reader=Reader(body);stage=reader.rect();fps=reader.uint(2)/256;frames=reader.uint(2)
    assert fps==30 and frames==3235,(fps,frames)
    definitions={};events=[];nested=[];heads=[];blocks=[];frame=1

    def nested_events(data,owner):
        local=1
        for code,payload in tags(data):
            if code==1:local+=1
            elif code==15:nested.append(sound_info(payload,local,owner))
            elif code==39:nested_events(payload[4:],struct.unpack_from('<H',payload)[0])

    for code,data in tags(body[reader.bit//8:]):
        if code==1:frame+=1
        elif code==14:
            sound,flags,count=struct.unpack_from('<HBI',data)
            assert flags>>4==2,'Unexpected non-MP3 definition'
            definitions[sound]=dict(id=sound,rate=(5512,11025,22050,44100)[(flags>>2)&3],
                                    channels=2 if flags&1 else 1,count=count,
                                    seek=struct.unpack_from('<h',data,7)[0],mp3=data[9:])
        elif code==15:events.append(sound_info(data,frame))
        elif code in (18,45):
            assert data[1]>>4==2,'Unexpected non-MP3 stream'
            heads.append(dict(frame=frame,tag=code,rate=(5512,11025,22050,44100)[(data[1]>>2)&3],
                              channels=2 if data[1]&1 else 1,samplesPerFrame=struct.unpack_from('<H',data,2)[0],
                              latency=struct.unpack_from('<h',data,4)[0],payload=data.hex()))
        elif code==19:
            count,seek=struct.unpack_from('<Hh',data)
            blocks.append(dict(frame=frame,samples=count,seek=seek,mp3=data[4:]))
        elif code==39:nested_events(data[4:],struct.unpack_from('<H',data)[0])
    assert frame-1==frames
    return digest,stage,fps,frames,definitions,events,nested,heads,blocks


def mp3_frames(data):
    """Count encoded native sample frames without relying on decoder padding."""
    offset=0;samples=0;frames=0;formats=set()
    mpeg1=(0,32,40,48,56,64,80,96,112,128,160,192,224,256,320)
    mpeg2=(0,8,16,24,32,40,48,56,64,80,96,112,128,144,160)
    while offset<len(data):
        assert offset+4<=len(data),'Truncated MP3 header'
        h=int.from_bytes(data[offset:offset+4],'big')
        version=(h>>19)&3;layer=(h>>17)&3;bitrate_index=(h>>12)&15;rate_index=(h>>10)&3
        if h>>21!=2047:
            # Original sound1680 carries seven non-MPEG bytes and an ID3v1
            # record after its46 complete audio frames. Keep that fact visible.
            tail=data[offset:]
            assert len(tail)==135 and tail[:7]==bytes.fromhex('ff147a00000029') and tail[7:10]==b'TAG',('Unrecognized MP3 tail',offset,tail[:16].hex())
            break
        assert version!=1 and layer==1 and 0<bitrate_index<15 and rate_index<3
        rate=(44100,48000,32000)[rate_index]//(1 if version==3 else 2 if version==2 else 4)
        bitrate=(mpeg1 if version==3 else mpeg2)[bitrate_index]
        size=(144000 if version==3 else 72000)*bitrate//rate+((h>>9)&1)
        assert offset+size<=len(data),'Truncated MP3 frame'
        formats.add((rate,1 if (h>>6)&3==3 else 2))
        samples+=1152 if version==3 else 576;frames+=1;offset+=size
    assert len(formats)==1,formats
    rate,channels=next(iter(formats))
    return dict(frames=frames,samples=samples,rate=rate,channels=channels,audioBytes=offset,
                nonAudioTailBytes=len(data)-offset,nonAudioTailSHA256=hashlib.sha256(data[offset:]).hexdigest())


def decode(ffmpeg,data,rate,channels):
    encoded=mp3_frames(data)
    assert (encoded['rate'],encoded['channels'])==(rate,channels)
    result=subprocess.run([ffmpeg,'-v','error','-f','mp3','-i','pipe:0','-f','s16le',
                           '-acodec','pcm_s16le','-ar',str(rate),'-ac',str(channels),'pipe:1'],
                          input=data[:encoded['audioBytes']],capture_output=True,check=True,timeout=60)
    pcm=np.frombuffer(result.stdout,dtype='<i2').reshape(-1,channels)
    assert len(pcm)==encoded['samples'],'Decoder introduced an unaccounted gapless trim'
    assert not result.stderr,result.stderr.decode(errors='replace')
    if encoded['nonAudioTailBytes']:
        original=subprocess.run([ffmpeg,'-v','error','-f','mp3','-i','pipe:0','-f','s16le',
                                 '-acodec','pcm_s16le','-ar',str(rate),'-ac',str(channels),'pipe:1'],
                                input=data,capture_output=True,check=True,timeout=60)
        assert original.stdout==result.stdout,'Excluding non-audio metadata changed PCM'
        encoded['fullPayloadDecodePCMMatches']=True
    return pcm,encoded


def stereo_44100(ffmpeg,pcm,rate):
    channels=pcm.shape[1]
    if rate!=SAMPLE_RATE:
        expected=len(pcm)*SAMPLE_RATE
        assert expected%rate==0,'Non-integral output sample count needs explicit handling'
        result=subprocess.run([ffmpeg,'-v','error','-f','s16le','-ar',str(rate),'-ac',str(channels),
                               '-i','pipe:0','-f','s16le','-acodec','pcm_s16le','-ar',str(SAMPLE_RATE),
                               '-ac',str(channels),'pipe:1'],input=pcm.tobytes(),capture_output=True,check=True,timeout=30)
        pcm=np.frombuffer(result.stdout,dtype='<i2').reshape(-1,channels)
        assert len(pcm)==expected//rate
    # FFmpeg's default mono-to-stereo matrix attenuates by sqrt(.5). Flash's
    # mono signal is instead duplicated; apply only an authored envelope gain.
    return np.repeat(pcm,2,axis=1) if channels==1 else pcm.copy()


def write_wav(path,pcm):
    assert pcm.dtype==np.dtype('<i2') and pcm.shape[1]==2
    data=pcm.tobytes()
    with wave.open(str(path),'wb') as output:
        output.setnchannels(2);output.setsampwidth(2);output.setframerate(SAMPLE_RATE);output.writeframes(data)
    with wave.open(str(path),'rb') as check:
        assert check.getnchannels()==2 and check.getsampwidth()==2 and check.getframerate()==SAMPLE_RATE
        assert check.getnframes()==len(pcm) and check.readframes(check.getnframes())==data
    return dict(samples=len(pcm),seconds=len(pcm)/SAMPLE_RATE,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                pcmSHA256=hashlib.sha256(data).hexdigest(),bytes=path.stat().st_size)


def build(source,output,exports):
    digest,stage,fps,last,definitions,root_events,nested,heads,blocks=inspect(source)
    assert len(heads)==1 and heads[0]['rate']==44100 and heads[0]['channels']==1
    head=heads[0];assert head['samplesPerFrame']==1470 and head['latency']==1677
    assert len(blocks)==3084 and blocks[0]['frame']==5 and blocks[-1]['frame']==3088
    assert all(b['frame']==5+i for i,b in enumerate(blocks))
    assert blocks[0]['seek']==head['latency']
    assert len(definitions)==18 and len(root_events)==19
    events=[e for e in root_events if FIRST_FRAME<=e['frame']<=last]
    assert len(events)==19 and len({e['id'] for e in events})==17
    assert [e['id'] for e in events if e['frame']==2634]==[1458,1459]
    assert all(e['id']==45 for e in nested),'Unexpected nested audio needs timeline translation'
    ffmpeg=imageio_ffmpeg.get_ffmpeg_exe()
    output.mkdir(parents=True,exist_ok=True);(output/'audio').mkdir(exist_ok=True)

    stream_data=b''.join(b['mp3'] for b in blocks)
    assert stream_data==(exports/'-1.mp3').read_bytes(),'Raw stream differs from independent FFDec export'
    for sound in definitions.values():assert sound['mp3']==(exports/f"{sound['id']}.mp3").read_bytes()
    decoded,encoded=decode(ffmpeg,stream_data,head['rate'],head['channels'])
    stream=stereo_44100(ffmpeg,decoded[head['latency']:],head['rate'])
    samples_per_frame=SAMPLE_RATE//int(fps);total=(last-FIRST_FRAME+1)*samples_per_frame
    offset=(blocks[0]['frame']-FIRST_FRAME)*samples_per_frame
    assert total==4752510 and offset==2940
    music=np.zeros((total,2),dtype='<i2');assert offset+len(stream)<=total
    music[offset:offset+len(stream)]=stream
    assert not np.any(music[:offset]) and not np.any(music[offset+len(stream):])
    assert np.array_equal(music[offset:offset+len(stream)],stream)
    music_report=write_wav(output/'music.wav',music)
    drift=[];written=0
    for block in blocks:
        expected=head['latency']+(block['frame']-blocks[0]['frame'])*samples_per_frame-written
        if block['seek']!=expected:drift.append(dict(frame=block['frame'],actual=block['seek'],expected=expected,samples=block['samples'],mp3Bytes=len(block['mp3'])))
        written+=block['samples']
    assert drift==[dict(frame=3088,actual=-2039,expected=567,samples=0,mp3Bytes=417)],drift
    music_report.update(firstSourceFrame=5,leadingPaddingSamples=offset,trailingPaddingSamples=total-offset-len(stream),
                        latencySkippedNativeSamples=head['latency'],decodedNativeSamples=len(decoded),encoded=encoded,
                        declaredBlockSamples=written,seekRecurrenceMatchedBlocks=len(blocks)-len(drift),terminalSeekAnomalies=drift,
                        rawMP3SHA256=hashlib.sha256(stream_data).hexdigest(),rawMP3MatchesFFDec=True)

    effects={};levels={};event_rows=[]
    for event in events:
        assert event['flags'] in (0,8) and event['loops']==1 and event['inPoint'] is None and event['outPoint'] is None
        envelope=event['envelope'];assert not envelope or len(envelope)==1 and envelope[0]['position44']==0
        raw_levels=(envelope[0]['left'],envelope[0]['right']) if envelope else (32768,32768)
        sound=definitions[event['id']]
        # SOUNDENVELOPE specifies averaging unequal L/R levels for mono.
        effective=((raw_levels[0]+raw_levels[1])/2,)*2 if sound['channels']==1 else raw_levels
        assert all(0<=v<=32768 for v in effective)
        if event['id'] in levels:assert levels[event['id']]==effective,'Per-event gain variants need distinct WAV assets'
        else:
            levels[event['id']]=effective
            native,encoded=decode(ffmpeg,sound['mp3'],sound['rate'],sound['channels'])
            seek,count=sound['seek'],sound['count'];assert 0<=seek and seek+count<=len(native)
            trimmed=native[seek:seek+count];assert len(trimmed)==count
            pcm=stereo_44100(ffmpeg,trimmed,sound['rate'])
            if effective!=(32768,32768):pcm=np.trunc(pcm.astype(np.float64)*np.array(effective)/32768).astype('<i2')
            report=write_wav(output/'audio'/f"{event['id']}.wav",pcm)
            report.update(nativeRate=sound['rate'],nativeChannels=sound['channels'],nativeSampleCount=count,
                          nativeSeekSamples=seek,decodedNativeSamples=len(native),discardedTailNativeSamples=len(native)-seek-count,
                          rawEnvelopeLevels=list(raw_levels),effectiveStereoLevels=list(effective),encoded=encoded,
                          rawMP3SHA256=hashlib.sha256(sound['mp3']).hexdigest(),rawMP3MatchesFFDec=True)
            effects[str(event['id'])]=report
        event_rows.append((event['frame'],f"audio/{event['id']}.wav"))
    assert 45 not in levels and len(effects)==17
    assert effects['1459']['samples']==66894*2 and effects['1651']['samples']==29453*2
    assert effects['1651']['effectiveStereoLevels']==[11543,11543]
    (output/'events.tsv').write_text(''.join(f'{frame}\t{file}\n' for frame,file in event_rows),encoding='utf-8',newline='\n')
    assert [(int(line.split('\t')[0]),line.split('\t')[1]) for line in (output/'events.tsv').read_text().splitlines()]==event_rows
    assert hashlib.sha256(source.read_bytes()).hexdigest()==digest
    report=dict(sourceSHA256=digest,sourceUnchanged=True,sourceStage=stage,fps=fps,firstSourceFrame=FIRST_FRAME,lastSourceFrame=last,
                introFrames=last-FIRST_FRAME+1,sampleRate=SAMPLE_RATE,channels=2,bitsPerSample=16,totalSamples=total,
                durationSeconds=total/SAMPLE_RATE,music=music_report,effects=effects,events=events,excludedNestedEvents=nested,
                eventCount=len(event_rows),effectFiles=len(effects),eventOrderAndDuplicateFramesPreserved=True,
                waveReadbackPassed=True,nativeRateSeekBeforeResample=True,musicAndEffectsSeparate=True,
                ffmpegVersion=subprocess.check_output([ffmpeg,'-version'],text=True).splitlines()[0],
                primaryReferences=REFERENCES,referenceNotes=['SeekSamples trims decoded native-rate PCM before resampling (JPEXS SoundFormat.decode and MP3Decoder).',
                'Adobe SWF19 continuous playback concatenates MP3 frames; only initial stream latency is skipped here.',
                'Adobe SWF19 SOUNDENVELOPE averages unequal left/right levels for mono; 1651 uses11543/32768 on both output channels.'],
                limitations=['No Adobe Flash audio loopback comparison has been performed; perfect AVM audio/video synchronization is not claimed.',
                'FFmpeg MP3 decoding and22050-to44100 resampling are not claimed bit-identical to the Adobe decoder/mixer.',
                'Final block3088 declares zero samples but contains one MP3 frame and seek-2039. Continuous raw payload is preserved; terminal seek is recorded rather than inventing a separate trim.',
                'Authored constant envelopes are converted to PCM16 with truncation toward zero; Adobe final sample quantization has not been measured.'])
    (output/'audio-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps(dict(sourceSHA256=digest,totalSamples=total,musicSamples=music_report['samples'],
                          events=len(event_rows),effects=len(effects),leadingSilence=offset,
                          trailingSilence=music_report['trailingPaddingSamples'],sourceUnchanged=True),indent=2))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=ROOT/'work/trailer-source/source.swf')
    parser.add_argument('--output',type=Path,default=ROOT/'assets/intro')
    parser.add_argument('--exports',type=Path,default=ROOT/'work/trailer-source/extract/sounds')
    args=parser.parse_args();build(args.source,args.output,args.exports)
