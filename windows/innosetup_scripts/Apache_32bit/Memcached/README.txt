Add memcached:
You can find binaries and sources here: http://code.jellycan.com/memcached/
For 32bit you can use binary, for 64bit there is project with manual how to build it.
You will need libevent, link is in that manual, download 1.4.14b version, it includes
MSVC project and builds withou problem. Building memcached had some issues, I don't remember
exactly all of them, but were easy to solve, one was conflict with windows some function,
just add underscore everywhere where is this function called and also before the name of 
function, so it calls this function and not windows embedded alternative. Other problems
was only setting path to libevent I think, mainly C/C++ -> include and Linker.

These binaries works perfectly with MemcacheClient, you can try it by MemcacheClientTest.exe
built with MemcacheClient.
 