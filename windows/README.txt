To build with Visual Studio, you will need to make sure you have the necessary header includes for libtiff, libjpeg, zlib and optionally libmemcached. 
Place these directly in the windows\dependencies\includes folder.

You will need also the library .lib files for these dependencies, which should have been compiled with a compatible version of Visual Studio and 
placed in. These are usually called jpeg.lib, libfcgi.lib, libtiff.lib and zlibstat.lib and optionally MemCacheClient.lib. Place these in the
appropriate folder for your platform:

windows\Visual Studio XX\libs\x64 for 64bit
windows\Visual Studio XX\libs\x32 for 32bit

Now open the solution file for your version of Visual Studio and compile for your platform. The binary output, iipsrv.fcgi will be placed
in windows\Visual Studio XX\Release\x64 or windows\Visual Studio XX\Release\Win32.

To enable/disable libmemcached support, add or remove the HAVE_MEMCACHED preprocessor defintion from the solution properties.
