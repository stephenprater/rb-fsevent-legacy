require 'spec_helper'

describe FSEvent do

  it "should automatically start the FSEvent native thread" do
    fsevent = FSEvent::Stream.new(@fixture_path, 0.1)
    fsevent.running?.should be_true
    fsevent.streams.length.should == 1
    fsevent.killall
  end

  it "can start and stop native FSEvent watchers" do
    fsevent = FSEvent::Stream.new(@fixture_path, 0.1)
    fsevent.running?.should be_true
    fsevent.streams.length.should == 1
    fsevent.killall
    fsevent.running?.should be_false
   
    file = @fixture_path.join("newfile#{rand(10).to_s}.rb")
    FileUtils.touch file
    sleep 0.2  

    # that event should NOT show up because watcher is stopped 
    fsevent.events do |e|
      raise "event block called even though events should return an empty array"
    end.should == [] 

    event = FSEvent::Stream.new(@fixture_path,0.1)
    event.running?.should be_true
    
    event.events.should == []
    
    event.killall
  end

  it "can only stop it once" do
    fsevent = FSEvent::Stream.new(@fixture_path,0.1)
    fsevent.killall
    lambda { fsevent.killall }.should raise_error RuntimeError
  end

  it "has a long running thread timer" do
    FSEvent::Stream.running?.should be_true
    sleep 1  
    FSEvent::NativeStream.timer_events.should > 1
  end

  it "shouldn't throw an error when it gets GC'd" do
    #basically, if this doesn't throw a bus error it probably worked.
    #you can ifdef the debugging output in the c-extension if you want
    #to futz about with it.
    local_event = FSEvent::Stream.new(@fixture_path,0.1)
    local_event.running?.should be_true
    local_event = nil
    GC.start
  end

  it "should work with path with an apostrophe" do
    fsevent = FSEvent::Stream.new(@fixture_path, 0.1)
    custom_path = @fixture_path.join("custom 'path")
    file = custom_path.join("newfile#{rand(10)}.rb").to_s
    
    fsevent.add_path custom_path
    fsevent.paths.should include("#{custom_path}")
    FileUtils.touch file
    
    sleep 0.2

    fsevent.events.collect do |e|
      e.event_path
    end.should include(custom_path.to_s + '/')

    File.delete file
    fsevent.killall
  end

  it "should catch new file" do
    fsevent = FSEvent::Stream.new(@fixture_path, 0.1)
    file = @fixture_path.join("newfile#{rand(10).to_s}.rb")
    
    FileUtils.touch file
    sleep 0.2
    File.delete file
  
    fsevent.events.collect do |e|
      e.event_path
    end.should include(@fixture_path.to_s + '/')
    fsevent.killall
  end

  it "should catch file update" do
    fsevent = FSEvent::Stream.new(@fixture_path, 0.1)
    file = @fixture_path.join("folder1/file1.txt")
    File.exists?(file).should be_true
    FileUtils.touch file
    sleep 0.2

    fsevent.events.collect do |e|
      e.event_path
    end.should include(@fixture_path.join("folder1/").to_s)
    fsevent.killall
  end

  it "should store events until called" do
    fsevent = FSEvent::Stream.new(@fixture_path, 0.1)
    file1 = @fixture_path.join("folder1/file3.txt")
    file2 = @fixture_path.join("folder1/folder2/file2.txt")
    File.exists?(file1).should be_true
    File.exists?(file2).should be_true
    FileUtils.touch file1
    sleep 0.5 
    FileUtils.touch file2
    sleep 0.2 

    fsevent.events.collect do |e|
      e.event_path
    end.should == [@fixture_path.join("folder1/").to_s, @fixture_path.join("folder1/folder2/").to_s]
    fsevent.killall
  end

  it "will run in a separate thread" do
    fsevent = nil
    t = Thread.new do
      fsevent = FSEvent::Stream.new(@fixture_path,0.1)
      fsevent.run
    end

    
    file1 = @fixture_path.join("folder1/file1.txt")
    file2 = @fixture_path.join("folder1/folder2/file2.txt")

    FileUtils.touch file1

    sleep 0.2

    fsevent.events.collect do |e|
      e.event_path
    end.should include(@fixture_path.join("folder1/").to_s)

    FileUtils.touch file2
    
    sleep 0.2

    fsevent.events.collect do |e|
      e.event_path
    end.should include(@fixture_path.join("folder1/folder2/").to_s)
  end
end
