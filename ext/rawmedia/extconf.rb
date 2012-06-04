# Copyright (c) 2012 Hewlett-Packard Development Company, L.P. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

root = File.expand_path('../../..', __FILE__)
build = File.expand_path('../build', __FILE__)

if ARGV.first == '--debug'
  args = '-DCMAKE_BUILD_TYPE:STRING=Debug'
else
  cleanup = %{rm -rf "#{build}"}
end

File.open('Makefile', 'w') do |f|
  f.write <<-EOF
all:
	mkdir -p "#{build}"
	cd "#{build}"; cmake "-DCMAKE_INSTALL_PREFIX:PATH=#{root}" #{args} "#{root}"
	$(MAKE) -C "#{build}"

install:
	cd "#{build}"; cmake -DCOMPONENT=libraries -P cmake_install.cmake
	#{cleanup}
  EOF
end
