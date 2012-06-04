# Copyright (c) 2012 Hewlett-Packard Development Company, L.P. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

module RawMedia
  module Log
    LEVEL_QUIET = -8
    LEVEL_PANIC = 0
    LEVEL_FATAL = 8
    LEVEL_ERROR = 16
    LEVEL_WARNING = 24
    LEVEL_INFO = 32
    LEVEL_VERBOSE = 40
    LEVEL_DEBUG = 48

    # @param [Fixnum] level the log level
    # @param [Proc] callback the callback to call with log message
    def self.set_callback(level, callback)
      # Save callback so it doesn't get GC'd
      @callback = callback
      Internal::rawmedia_set_log(level, callback)
    end
  end
end
