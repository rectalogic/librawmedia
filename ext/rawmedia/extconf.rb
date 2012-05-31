root = File.expand_path('../../..', __FILE__)
build = File.expand_path('../build', __FILE__)

unless ARGV.empty?
  args = ARGV.map {|a| %{"#{a}"} }.join(' ')
end

File.open('Makefile', 'w') do |f|
  f.write <<-EOF
all:
	mkdir -p "#{build}"
	cd "#{build}"; cmake "-DCMAKE_INSTALL_PREFIX:PATH=#{root}" #{args} "#{root}"
	$(MAKE) -C "#{build}"

install:
	cd "#{build}"; cmake -DCOMPONENT=libraries -P cmake_install.cmake
	rm -rf "#{build}"
  EOF
end
