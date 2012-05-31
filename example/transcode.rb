require_relative '../lib/rawmedia'

unless ARGV.length == 2
  puts "Usage: #{File.basename(__FILE__)} <input-video> <output-video>"
  exit 1
end

log = Proc.new do |msg|
  puts "RUBY #{msg}"
end
RawMedia::Log.set_callback(RawMedia::Log::LEVEL_VERBOSE, log)

session = RawMedia::Session.new
decoder = RawMedia::Decoder.new(ARGV[0], session, 320, 240)
encoder = nil
abuf = session.create_audio_buffer

decoder.duration.times do
  decoder.decode_audio(abuf) if decoder.has_audio?
  decoder.decode_video if decoder.has_video?

  unless encoder
    encoder = RawMedia::Encoder.new(ARGV[1], session,
                                    decoder.width, decoder.height,
                                    decoder.has_video?, decoder.has_audio?)
  end
  encoder.encode_audio(abuf) if decoder.has_audio?
  encoder.encode_video(decoder.video_buffer, decoder.video_buffer_size) if decoder.has_video?
end

encoder.destroy
decoder.destroy
