require 'spec_helper'

module RawMedia
  describe Encoder do
    let(:framerate) { Rational(15) }
    let(:session) { Session.new(framerate) }
    let(:filename) { File.expand_path('../../fixtures/320x180-25fps.mov', __FILE__) }
    let(:decoder) { Decoder.new(filename, session, 1000, 1000) }

    it 'should encode video' do
      encoder = Encoder.new('/dev/null', session, 320, 180)
      decoder.decode_video
      encoder.encode_video(decoder.video_buffer, decoder.video_buffer_size)
      encoder.destroy
    end

    it 'should encode audio' do
      encoder = Encoder.new('/dev/null', session, 320, 180)
      buffer = session.create_audio_buffer
      decoder.decode_audio(buffer)
      encoder.encode_audio(buffer)
    end

    it 'should destroy' do
      encoder = Encoder.new('/dev/null', session, 320, 180)
      encoder.destroy
    end
  end
end
