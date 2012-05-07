require 'rawmedia'

session = RawMedia::Session.new(320, 240)
decoder = RawMedia::Decoder.new("/Users/aw/Movies/thehillshaveeyes.mov", session)
encoder = RawMedia::Encoder.new("/tmp/out.mov", session)

abuf = decoder.create_audio_buffer

200.times do
  decoder.decode_audio(abuf)
  vbuf, linesize = decoder.decode_video

  encoder.encode_audio(abuf)
  encoder.encode_video(vbuf, linesize)
end

encoder.destroy
decoder.destroy
