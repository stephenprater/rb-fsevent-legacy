require 'mkmf'

if RUBY_PLATFORM.downcase.include?("darwin")
  with_ldflags($LDFLAGS + ' -framework Foundation') { true }
  with_ldflags($LDFLAGS + ' -framework CoreServices') { true }
else
  raise "Compiling this for something other than OSX doesn't make sense."
end

$CFLAGS="-g -O0 -pipe -fno-common"

create_makefile("rb-fsevent-legacy/fs_native_stream")
