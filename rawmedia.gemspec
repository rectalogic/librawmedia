lib = File.expand_path('../lib/', __FILE__)
$:.unshift lib unless $:.include?(lib)

require 'rawmedia/version'

Gem::Specification.new do |s|
  s.name        = "rawmedia"
  s.version     = RawMedia::VERSION
  s.platform    = Gem::Platform::RUBY
  s.authors     = ["Andrew Wason"]
  s.email       = ["rectalogic@rectalogic.com"]
  s.homepage    = "https://github.com/rectalogic/librawmedia"
  s.summary     = %q{Audio/video decoding and raw encoding via FFmpeg}
  s.description = s.summary

  s.add_development_dependency 'rspec', '~> 2.10.0'
  s.add_development_dependency 'simplecov', '~> 0.6.4'
  s.add_development_dependency 'rake', '~> 0.9.0'

  if system('git rev-parse')
    s.files     = `git ls-files`.split("\n")
  else
    s.files     = Dir.glob('**/*')
  end

  s.extensions  = ['ext/rawmedia/extconf.rb']

  s.require_paths = ["lib"]
end
