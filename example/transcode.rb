require_relative '../lib/rawmedia'

log = Proc.new do |msg|
  puts "RUBY #{msg}"
end
RawMedia::Log.set_callback(RawMedia::Log::LEVEL_VERBOSE, log)

session = RawMedia::Session.new
decoder = RawMedia::Decoder.new("/Users/aw/Movies/thehillshaveeyes.mov", session, 320, 240)
encoder = nil
abuf = session.create_audio_buffer

200.times do
  decoder.decode_audio(abuf)
  decoder.decode_video

  unless encoder
    encoder = RawMedia::Encoder.new("/tmp/out.mov", session, decoder.width, decoder.height)
  end
  encoder.encode_audio(abuf)
  encoder.encode_video(decoder.video_buffer, decoder.video_buffer_size)
end

encoder.destroy
decoder.destroy
