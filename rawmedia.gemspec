require_relative "lib/rawmedia/version"

git = system('git rev-parse')
if git
  git_tag = `git describe`.chop
  if RawMedia::VERSION != git_tag
    puts "WARNING: RawMedia VERSION #{RawMedia::VERSION} does not match git tag #{git_tag}"
  end
end

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

  if git
    s.files     = `git ls-files`.split("\n")
  else
    s.files     = Dir.glob("{ext,lib,rawmedia,spec,example}/**/*") +
      %w(LICENSE README.md CMakeLists.txt Gemfile Rakefile)
  end

  s.extensions  = ['ext/rawmedia/extconf.rb']

  s.require_paths = ["lib"]
end
