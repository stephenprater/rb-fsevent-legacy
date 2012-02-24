# -*- encoding: utf-8 -*-
$:.push File.expand_path("../lib", __FILE__)
require "rb-fsevent-legacy/version"

Gem::Specification.new do |s|
  s.name        = "rb-fsevent-legacy"
  s.version     = FSEvent::VERSION
  s.platform    = Gem::Platform::RUBY
  s.authors     = ['Stephen Prater']
  s.email       = ['me@stephenprater.com']
  s.homepage    = "http://github.com/stephenprater/rb-fsevents-legacy"
  s.summary     = "Very simple & usable FSEvents API - it works on PowerPCs and 10.5.8"
  s.description = "A legacy compatible version of an FSEvents API for Darwin."

  s.rubyforge_project = "rb-fsevent-legacy"

  s.add_development_dependency  'bundler',     '~> 1.0.10'
  s.add_development_dependency  'rspec',       '~> 2.5.0'
  s.add_development_dependency  'rcov'
  s.add_development_dependency  'pry'

  s.files         = `git ls-files`.split("\n")
  s.extensions    = ['ext/rb-fsevent-legacy/extconf.rb']
  s.require_paths = ['lib','ext']
end

