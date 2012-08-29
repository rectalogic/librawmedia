# Copyright (c) 2012 Hewlett-Packard Development Company, L.P. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

require 'rake'
require 'rake/tasklib'

module RawMedia
  module Rake
    # Rake task to generate a raw media file test fixture
    class VideoFixtureTask < ::Rake::TaskLib
      include ::Rake::DSL if defined?(::Rake::DSL)

      # @return [String] the output file name, and task name
      attr_accessor :filename

      # @return [String] the media file framerate (Fixnum, Float or num/den)
      attr_accessor :framerate

      # @return [String] the video frame size (WxH)
      attr_accessor :size

      # @return [String] the media file duration in seconds
      attr_accessor :duration

      # The environment variable FFMPEG can be used to locate the ffmpeg executable.
      # @param [String] filename the name of the output file and rake task
      # @yield a block to allow any options to be modified on the task
      # @yieldparam [FixtureTask] self the task object to allow any parameters
      #   to be changed.
      def initialize(filename)
        @filename = filename
        @framerate = '30'
        @size = '320x240'
        @duration = '5'
        yield self if block_given?
        @ffmpeg = ENV.fetch('FFMPEG', 'ffmpeg')
        define
      end

      def define
        desc "Generate a raw media MOV file fixture"
        file filename do
          sh %{"#@ffmpeg" -f lavfi -i "aevalsrc=sin(440*2*PI*t)::s=8000,aconvert=s16:stereo" -f lavfi -i "testsrc=rate=#{framerate}:size=#{size}:decimals=3" -codec:a pcm_s16le -codec:v rawvideo -pix_fmt uyvy422 -tag:v yuvs -f mov -t #{duration} -y "#{filename}"}
        end
      end
      protected :define
    end
  end
end
