require 'rake/clean'
require 'bundler'
Bundler::GemHelper.install_tasks

require 'rspec'
require 'rspec/core/rake_task'
RSpec::Core::RakeTask.new(:spec)
task :default => :spec

file "lib/rb-fsevent-legacy/fs_native_stream.bundle" => Dir.glob("ext/rb-fsevent-legacy/*{.rb,.c}") do
  Dir.chdir("ext/rb-fsevent-legacy") do
    ruby "extconf.rb"
    sh "make"
  end
  cp "ext/rb-fsevent-legacy/fs_native_stream.bundle", "lib/rb-fsevent-legacy/fs_native_stream.bundle"
end

task :spec => "lib/rb-fsevent-legacy/fs_native_stream.bundle"

CLEAN.include('ext/**/*{.o,.log,.bundle}')
CLEAN.include('ext/**/Makefile')
CLOBBER.include('lib/**/*.bundle')

namespace(:spec) do
  desc "Run all specs on multiple ruby versions (requires rbenv)"
  task(:portability) do
    %w[1.8.6 1.8.7 1.9.2 jruby-head].each do |version|
      system <<-BASH
        bash -c 'source ~/.rvm/scripts/rvm;
                 rvm #{version};
                 echo "--------- version #{version} ----------\n";
                 bundle install;
                 rake spec'
      BASH
    end
  end
  desc "run all specs with rcov"
  RSpec::Core::RakeTask.new(:coverage) do |t|
    t.rcov = true
    t.rcov_opts =  %q[--exclude "spec"]
    t.verbose = true
  end
end
