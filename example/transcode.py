import rawmedia
session = rawmedia.Session(width=320, height=240)
decoder = rawmedia.Decoder("/Users/aw/Movies/thehillshaveeyes.mov", session)
encoder = rawmedia.Encoder("/tmp/out.mov", session)

abuf = decoder.create_audio_buffer()
for i in range(200):
    decoder.decode_audio(abuf)
    vbuf, linesize = decoder.decode_video()
    encoder.encode_audio(abuf)
    encoder.encode_video(vbuf, linesize)

decoder.destroy()
encoder.destroy()
