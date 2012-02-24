#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

#include <ruby.h>
#include <pthread.h>
#include <stdlib.h>

#define COMPILED_AT __DATE__ " " __TIME__
#define DEBUG true

static VALUE cFSEventSystemStream;

typedef struct cFSEventEventType {
  char * path;
  long eventFlags;
  unsigned int long eventId;
} cFSEventEvent;

typedef struct cFSEventSystemStreamType {
  cFSEventEvent *events; // pointer to an arry of events
  int numEvents;
  double latency;
  VALUE path;
  VALUE running;
  FSEventStreamRef native_stream;
} cFSEventSystemStreamType;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t FSEventThread;
int semaphore = 0;

CFRunLoopRef FSEventRunLoop;
int timer_passes;
VALUE last_events;
VALUE rbFSEventModule;

void *fs_rb_cf_allocate(CFIndex size, CFOptionFlags hint, void *info);
void *fs_rb_cf_reallocate(void *ptr, CFIndex newsize, CFOptionFlags hint, void *info);
void  fs_rb_cf_deallocate(void *ptr, void *info);
void  fs_callback( const FSEventStreamRef streamRef, void *callbackInfo, size_t numEvents, void *eventPaths,
                  const FSEventStreamEventFlags eventFlags[],
                  const FSEventStreamEventId eventIds[]);
static VALUE fs_init(VALUE self, VALUE directory, VALUE latency);
void *fs_runloop();

// We need to create a custom allocator so that we place nice with
// ruby's GC and allocation practices
static CFAllocatorRef rb_CFAllocator(void) {
  static CFAllocatorRef allocator = NULL;
  if(!allocator) {
    CFAllocatorContext context = {
      0, // CFIndex Version - always 0
      NULL, // void * to context info
      NULL, // retain callback insn't necessary
      NULL, // release callback isn't necessary
      NULL, // a callback which returns a description based on info, so NULL here too.
      fs_rb_cf_allocate,   // core foundation allocation functions which play nice
      fs_rb_cf_reallocate, // with the ruby gc
      fs_rb_cf_deallocate,
      NULL // the preferred size allocater
    };
    allocator = CFAllocatorCreate(NULL, &context);
  }
  return allocator;
}

void * fs_rb_cf_allocate(CFIndex size, CFOptionFlags hint, void *info){
  // hint and info are always going to bu null in our case.
  // okay, so the size here is an int of BYTES.  ruby's ALLOC_N macro
  // requires that we allocate by pointer type.  I think this
  // is fairly cross platform - but I'm not sure.
  ALLOC_N(char,size);
}
void * fs_rb_cf_reallocate(void *ptr, CFIndex newsize, CFOptionFlags hint, void *info){
  // hint and info are always going to bu null in our case.
  REALLOC_N(ptr, char, newsize);
}

void fs_rb_cf_deallocate(void *ptr, void *info){
  xfree(ptr);
}

//function prototypes
//

// AND GO.
//
void fs_timer(){
#ifdef DEBUG
    printf("Timer Fire\n");
#endif
  pthread_mutex_lock(&mutex);
  timer_passes++;
  pthread_mutex_unlock(&mutex);
}

// so OSX doesn't actually have posix semaphores, and i can't make
// named sempahore work, so implement our own, rather stupid
// version of them.
void fs_wait_for_unlock(){
  int deadlock = 0;
  // really, it should never wait that long
  while(semaphore < 0 && deadlock < 32767) { deadlock++; }
  pthread_mutex_lock(&mutex);
  semaphore = -1;
  pthread_mutex_unlock(&mutex);
}

void fs_unlock(){
  pthread_mutex_lock(&mutex);
  semaphore = 0;
  pthread_mutex_unlock(&mutex);
}

void fs_callback(
    __attribute__((unused)) const FSEventStreamRef streamRef,
    void *callbackInfo, 
    size_t numEvents,
    void *eventPaths,
    const FSEventStreamEventFlags eventFlags[],
    const FSEventStreamEventId eventIds[]){
  
  // this is actually running on our pthread.
  // ruby is not NOT pthread aware or safe, we we
  // lockout everything that could happen from
  // calling ruby in this thread.

  // this is only going to do anything
  // if there's data corruption, in which case we probably
  // want to bail out anyway.
  
  // the situation is shutting down, so we don't really care
  if(FSEventRunLoop == false){
#ifdef DEBUG
    printf("CoreFoundation callback stopped\n");
#endif
    return;
  }
  
  pthread_mutex_lock(&mutex);

#ifdef DEBUG
    printf("CoreFoundation callback enter for stream <#FSEvent::NativeStream 0x%x>\n", (VALUE)callbackInfo * 2);fflush(stdout);
#endif
    // we do object allocation here in another thread
    // so mutex it incase the GC is running
    rb_gc_disable();
    
    int start_dex;
    char **paths = eventPaths;
    // #define REALLOC_N(var,type,n) (var)=(type*)xrealloc((char*)(var),sizeof(type)*(n)) 
    // make a new FSEvent ruby object
    cFSEventSystemStreamType *obj;
    Check_Type((VALUE)callbackInfo, T_DATA);
    obj = (cFSEventSystemStreamType *)DATA_PTR((VALUE)callbackInfo);
    
    if(obj->numEvents > 0){
#ifdef DEBUG
      printf("Adding %d to %d existing events\n", numEvents, obj->numEvents);
#endif
      start_dex = obj->numEvents;
      obj->numEvents = obj->numEvents + numEvents;
    } else {
      start_dex = 0;
      obj->numEvents = numEvents;
    }
    
    // just always do realloc - since it will act as an malloc
    // if we've already nulled the pointer
#ifdef DEBUG
      printf("Allocating for %d events\n", obj->numEvents);
#endif
    cFSEventEvent *old = obj->events; // yes, i want to store the address.
    obj->events = (cFSEventEvent *)realloc(obj->events, sizeof(cFSEventEvent) * (int)obj->numEvents);
    
    
    if(obj->events == NULL){
      rb_bug("Couldn't malloc events from secondary watch thread");
    }

    int i,j;
    
    for(i = start_dex, j = 0; j < numEvents; i++, j++){
      obj->events[i].path = malloc(strlen(paths[j])+1);
      if(obj->events[i].path == NULL){
        rb_bug("Couldn't malloc path from secondary watch thread");
      }
      strcpy(obj->events[i].path,paths[j]);
      obj->events[i].eventFlags = eventFlags[i];
      obj->events[i].eventId = eventIds[i];
    }
    rb_gc_enable();
    
#ifdef DEBUG
    printf("CoreFoundation callback exited\n");
#endif

  pthread_mutex_unlock(&mutex);
}

static VALUE fs_timer_events(VALUE self){
#ifdef DEBUG
  printf("Timer value retrieved\n"); fflush(stdout);
#endif

  VALUE timer;
  pthread_mutex_lock(&mutex);
  timer = INT2FIX(timer_passes);
  pthread_mutex_unlock(&mutex);
  return timer;
}

static VALUE fs_events(VALUE self) {
  // this should be running on the main ruby thread
  pthread_mutex_lock(&mutex);
#ifdef DEBUG
    printf("Ruby thread event enter for <#FSEvent::NativeStream 0x%x>\n",(VALUE)self * 2); fflush(stdout);
#endif

    cFSEventSystemStreamType *obj;
    // DataGetStruct(self,cFSEventSystemStreamType,obj);
    Check_Type(self, T_DATA);
    obj = (cFSEventSystemStreamType *)DATA_PTR(self);

    if(obj->numEvents == 0){
#ifdef DEBUG
      printf("Ruby thread event break\n"); fflush(stdout);
#endif
      pthread_mutex_unlock(&mutex);
      return Qnil;
    }
 
    volatile VALUE FSEvent = rb_const_get(rbFSEventModule,rb_intern("Event"));
  
    if(rb_block_given_p()) {
      int len = obj->numEvents;
      int i;
#ifdef DEBUG
      printf("Yielding %d values\n",obj->numEvents);
#endif
      for(i=0;i<len;i++){
        volatile VALUE fs_event = rb_funcall(FSEvent,rb_intern("new"),0);
        rb_funcall(fs_event,rb_intern("event_id="),1,ULL2NUM(obj->events[i].eventId));
        rb_funcall(fs_event,rb_intern("event_path="),1,rb_str_new2(obj->events[i].path));
        rb_funcall(fs_event,rb_intern("event_flags="),1,LONG2NUM(obj->events[i].eventFlags));
        
#ifdef DEBUG
        printf("Ruby thread event yield\n"); fflush(stdout);
#endif

        rb_yield(fs_event);
      }
      
      // delete all of the events
      obj->numEvents = 0;
      free(obj->events);
      obj->events = NULL;
    }
#ifdef DEBUG
        printf("Ruby thread event exit\n"); fflush(stdout);
#endif

  pthread_mutex_unlock(&mutex);
  return Qnil;
}

static VALUE fs_running(VALUE self){
  cFSEventSystemStreamType *obj;
  Check_Type(self, T_DATA);
  obj = (cFSEventSystemStreamType *)DATA_PTR(self);
  return (VALUE)obj->running;
}

static VALUE fs_path(VALUE self){
  cFSEventSystemStreamType *obj;
  Check_Type(self, T_DATA);
  obj = (cFSEventSystemStreamType *)DATA_PTR(self);
  return (VALUE)obj->path;
}

// create a new FS Event Stream
FSEventStreamRef fs_new_stream(char *dir, VALUE *self) {
  // always runs on the ruby thread.
  //
  cFSEventSystemStreamType *obj;
  Check_Type(self, T_DATA);
  obj = (cFSEventSystemStreamType *)DATA_PTR(self);

  CFStringRef mypath = CFStringCreateWithCString(NULL,dir,CFStringGetSystemEncoding());
  CFArrayRef paths = CFArrayCreate(NULL, (const void **)&mypath, 1, NULL);
  FSEventStreamContext context = {0,self,NULL,NULL,NULL};
  FSEventStreamRef stream;
  CFAbsoluteTime latency = obj->latency;
  
  stream = FSEventStreamCreate(
      rb_CFAllocator(),  // use the customer corefoundation allocator...
      (FSEventStreamCallback)&fs_callback,
      &context,
      paths,
      kFSEventStreamEventIdSinceNow, //allow to replace with a meaningufl "since"
      latency,
      kFSEventStreamCreateFlagNone //look this up
    );
  return stream;
}

static void fs_free(cFSEventSystemStreamType *me){
  //unconditionally free unprocessed events
#ifdef DEBUG
  printf("GC Free for native stream\n"); fflush(stdout);
#endif
 
  pthread_mutex_lock(&mutex);
    if(me->running == Qtrue){
      // we only get called from ruby's GC - but it's possible
      // for this object to be GCd after the runloop itself has been
      // so check for that situation
      FSEventStreamFlushSync(me->native_stream);
      FSEventStreamStop(me->native_stream);
      FSEventStreamInvalidate(me->native_stream);
    }
  free(me->events);
  FSEventStreamRelease(me->native_stream);
  xfree(me);
  pthread_mutex_unlock(&mutex);
}

static void fs_mark(cFSEventSystemStreamType *me){
#ifdef DEBUG
  printf("GC Mark for native stream\n"); fflush(stdout);
#endif
  
  rb_gc_mark(me->path);
  rb_gc_mark(me->running); // i think that's extraneous - but mark it just to be safe.
}

static VALUE fs_stop(VALUE self){
#ifdef DEBUG
  printf("Native stream stopping #<FSEvent::NativeStream 0x%x>\n",self * 2); fflush(stdout);
#endif
  
  cFSEventSystemStreamType *obj;
  Check_Type(self, T_DATA);
  obj = (cFSEventSystemStreamType *)DATA_PTR(self);

  if(obj->running == Qfalse){
    VALUE excp = rb_const_get(rb_cObject,rb_intern("RuntimeError"));
    rb_raise(excp, "native stream watcher was stopped twice");
  }

  FSEventStreamFlushSync(obj->native_stream);
  FSEventStreamStop(obj->native_stream);
  FSEventStreamInvalidate(obj->native_stream);
  obj->running = Qfalse;
 
  return obj->running;
}

static VALUE fs_alloc(VALUE klass){
  // create and assign an empty eventstream.
#ifdef DEBUG
  printf("GC Alloc for native stream\n"); fflush(stdout);
#endif
  cFSEventSystemStreamType *obj;
  return Data_Make_Struct(klass,cFSEventSystemStreamType,fs_mark,fs_free,obj);
}

static VALUE fs_init(VALUE self, VALUE directory, VALUE latency) {
  char *dir;
  cFSEventSystemStreamType *obj;
  /* obj, type, sval */
  //DataGetStruct(self,cFSEventSystemStreamType,obj);
  Check_Type(self, T_DATA);
  obj = (cFSEventSystemStreamType *)DATA_PTR(self);
  
  dir = RSTRING_PTR(StringValue(directory));
  obj->latency = NUM2DBL(latency);
  obj->native_stream = fs_new_stream(dir,(VALUE *)self);
  obj->events = NULL; 
  obj->path = directory;
  obj->numEvents = 0;
  
  
  FSEventStreamScheduleWithRunLoop(obj->native_stream, FSEventRunLoop, kCFRunLoopDefaultMode);
  FSEventStreamStart(obj->native_stream);

  obj->running = Qtrue;

  return self;
}

void *fs_runloop() {
  FSEventRunLoop = CFRunLoopGetCurrent();
  // create a one second timer to keep the event loop running even
  // if we haven't actually added an FSEventStream yet.
  CFRunLoopTimerRef timer = CFRunLoopTimerCreate(
      NULL,
      (CFAbsoluteTime)0,
      (CFTimeInterval)1.0, //why not.
      0,
      0,
      &fs_timer,
      NULL
      );
  CFRunLoopAddTimer(FSEventRunLoop,timer,kCFRunLoopCommonModes);
#ifdef DEBUG
  printf("Starting CFRunLoop\n"); fflush(stdout);
#endif
  // this will not get set until the runloop terminates
  // when it will show the reason for termination
  CFRunLoopRun();
  FSEventRunLoop = false;

  // do we need to do any cleanup from our thread?
}

static void fs_stoploop(VALUE dummy) {
#ifdef DEBUG
  printf("Stopping CFRunLoop\n"); fflush(stdout);
#endif
  CFRunLoopStop(FSEventRunLoop);
}

void Init_fs_native_stream(){
  rbFSEventModule = rb_const_get(rb_cObject,rb_intern("FSEvent"));
  cFSEventSystemStream = rb_define_class_under(rbFSEventModule, "NativeStream", rb_cObject);
  rb_define_alloc_func(cFSEventSystemStream,fs_alloc);
  rb_define_method(cFSEventSystemStream,"initialize",fs_init,2);
  rb_define_method(cFSEventSystemStream,"events",fs_events,0);
  rb_define_method(cFSEventSystemStream,"path",fs_path,0);
  rb_define_method(cFSEventSystemStream,"running?",fs_running,0);
  rb_define_method(cFSEventSystemStream,"stop",fs_stop,0);
  
  rb_define_singleton_method(cFSEventSystemStream,"timer_events",fs_timer_events,0);

  // we create a new pthread that we manage
  pthread_create(&FSEventThread,NULL,&fs_runloop,NULL);
  rb_set_end_proc(fs_stoploop,0);
}
