# Copyright (c) 2012 Hewlett-Packard Development Company, L.P. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

module RawMedia
  module Util
    # @param [FFI::Pointer] pointer
    # @param [Fixnum] size number of bytes pointer points to
    # @return [java.nio.ByteBuffer] ByteBuffer wrapping pointers memory
    def self.wrap_pointer(pointer, size=pointer.size)
      org.jruby.ext.ffi.Factory.getInstance().
        wrapDirectMemory(JRuby.runtime, pointer.address).
        slice(0, size).asByteBuffer()
    end
  end
end
