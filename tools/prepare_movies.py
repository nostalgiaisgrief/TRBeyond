"""Convert original RPL movies to a lossless, streamable runtime cache.
Requires PyAV (FFmpeg bindings) only during preparation, never at runtime.
"""
import argparse,struct
from pathlib import Path

def convert(source,destination,cancel=None):
    import av
    destination.parent.mkdir(parents=True,exist_ok=True)
    temporary=destination.with_suffix('.tmp')
    try:
        with av.open(str(source)) as container,temporary.open('wb') as output:
            video=container.streams.video[0];rate=video.average_rate
            width,height=video.codec_context.width,video.codec_context.height
            if not rate or not 0<width<=2048 or not 0<height<=2048:raise ValueError('Invalid video dimensions/rate')
            output.write(bytes(40));count=0;audio=bytearray()
            resampler=av.AudioResampler(format='s16',layout='stereo',rate=44100)
            for packet in container.demux():
                if cancel is not None and cancel.is_set():raise ValueError('Movie conversion cancelled')
                for frame in packet.decode():
                    if isinstance(frame,av.VideoFrame):
                        rgb=frame.reformat(format='rgb24');plane=rgb.planes[0];data=memoryview(plane)
                        for row in range(height):output.write(data[row*plane.line_size:row*plane.line_size+width*3])
                        count+=1
                    elif isinstance(frame,av.AudioFrame):
                        for pcm in resampler.resample(frame):audio.extend(bytes(pcm.planes[0])[:pcm.samples*4])
            for pcm in resampler.resample(None):audio.extend(bytes(pcm.planes[0])[:pcm.samples*4])
            if not count:raise ValueError('No video frames')
            output.write(audio);output.seek(0)
            output.write(struct.pack('<8s8I',b'TRBMOV1\0',width,height,rate.numerator,rate.denominator,count,44100,2,len(audio)))
        temporary.replace(destination)
        print(f'{source.name}: {count} frames, {width}x{height}, {rate} fps, {len(audio)//4} audio samples')
    finally:
        if temporary.exists():temporary.unlink()

def prepare_movies(root):
    movies=sorted((root/'FMV').glob('*.RPL'))
    if not movies:raise ValueError('No FMV/*.RPL files found')
    for source in movies:convert(source,source.with_suffix('.bmv'))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('assets',type=Path);args=parser.parse_args();prepare_movies(args.assets)
