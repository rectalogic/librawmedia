require 'spec_helper'

module RawMedia
  describe AudioMixer do
    let(:session) { Session.new }
    let(:mixer) { session.create_audio_mixer }

    it 'should encode nil to silence' do
      buffers = [nil, nil, nil]
      output = session.create_audio_buffer
      mixer.mix(buffers, output)
      # Cast to int16 array and check elements
      int16 = FFI::Pointer.new(:int16, output)
      count = output.size / int16.type_size
      count.times do |i|
        int16[i].get_short.should == 0
      end
    end

    it 'should add sample values' do
      buffer_count = 3
      sample_value = 30
      buffers = Array.new(buffer_count)
      buffers.fill do
        buffer = session.create_audio_buffer
        buffer.put_short(0, sample_value)
        buffer
      end
      output = session.create_audio_buffer
      mixer.mix(buffers, output)

      int16 = FFI::Pointer.new(:int16, output)
      int16[0].get_short.should == sample_value * buffer_count
    end

    it 'should clamp sample values' do
      sample_value = 32000
      buffers = Array.new(4)
      buffers.fill do
        buffer = session.create_audio_buffer
        buffer.put_short(0, sample_value)
        buffer
      end
      output = session.create_audio_buffer
      mixer.mix(buffers, output)

      int16 = FFI::Pointer.new(:int16, output)
      int16[0].get_short.should == 32767
    end
  end
end
