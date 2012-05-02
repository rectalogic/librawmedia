import numpy
import rawmedia

session = rawmedia.Session(width=320, height=240)
decoder = rawmedia.Decoder("/Users/aw/Movies/thehillshaveeyes.mov", session)
encoder = rawmedia.Encoder("/tmp/out.mov", session)

abuf = decoder.create_audio_buffer()
# Wrap raw buffer with numpy int16 so we can manipulate sound
# This causes a stupid warning http://bugs.python.org/issue10744
nbuf = numpy.ctypeslib.as_array(abuf)
nbuf.dtype = numpy.int16

for i in range(200):
    decoder.decode_audio(abuf)
    vbuf, linesize = decoder.decode_video()

    nbuf /= 4

    encoder.encode_audio(abuf)
    encoder.encode_video(vbuf, linesize)

decoder.destroy()
encoder.destroy()
