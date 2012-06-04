# Copyright (c) 2012 Hewlett-Packard Development Company, L.P. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

module RawMedia
  class AudioMixer
    # @param [Session] session
    def initialize(session)
      @session = session
    end

    # @param [Array<#to_ptr>] input_buffers input audio buffers,
    #  may contain nil elements
    # @param [#to_ptr] output_buffer where to write mixed audio
    def mix(input_buffers, output_buffer)
      fill_buffer_array(input_buffers)
      Internal::rawmedia_mix_audio(@session.session, @buffer_list_ptr,
                                   @buffer_count, output_buffer)
    end

    def fill_buffer_array(input_buffers)
      if input_buffers.length != @buffer_count
        @buffer_count = input_buffers.length
        @buffer_list_ptr = FFI::MemoryPointer.new(:pointer, @buffer_count)
      end
      @buffer_list_ptr.put_array_of_pointer(0, input_buffers)
    end
    private :fill_buffer_array
  end
end
