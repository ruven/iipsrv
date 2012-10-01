BUILDING IIPSERVER PROJECT IN VISUAL STUDIO 2008/2010
(only release versions are built, if you want build debug version, it should be similar)
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
LibFCGI (Tested with version 2.4.1 - SNAP)
	1.Acquired from: http://www.fastcgi.com/dist/ (tar.gz version)
	2.Open fcgi-2.4.1-SNAP-0910052249\Win32\libfcgi.dsp in Visual Studio.
	3.Convert and Open Projects (Yes)
	4.Create fcgi_config.h in fcgi-2.4.1-SNAP-0910052249\include and copy content 
	  of fcgi_config_x86.h to this file
	7.Build Release version
	8.Verify fcgi-2.4.1-SNAP-0910052249\libfcgi\Release\libfcgi.dll exists and copy 
	  it to windows\dependencies\dlls in iipsrv sources
	9.Verify fcgi-2.4.1-SNAP-0910052249\libfcgi\Release\libfcgi.lib exists and copy 
	  it to windows\dependencies\libs in iipsrv sources
	10.Copy header files from fcgi-2.4.1-SNAP-0910052249\include to windows\dependencies\includes 
	  in iipsrv sources

JPEGSR-8d (Tested with version 8d)
	 1.Acquired from: http://www.ijg.org/files (zip version)

MSVC2008:2.Run NMAKE /f makefile.vc setup-vc6 (Start -> Visual Studio Command Prompt,
	   go to jpeg-8d folder, type it in and press enter)
	 3.Run NMAKE /f "jpeg.mak" 
	 4.Copy lib from release to windows\dependencies\libs in iipsrv sources
	   (only static lib will be created)
	 5.Copy all header files from jpeg-8d to windows\dependencies\includes in iipsrv sources

MSVC2010:2.Run NMAKE /f makefile.vc setup-v10 (Start -> Visual Studio Command Prompt,
	   go to jpeg-8d folder, type it in and press enter)
   	   This will move jconfig.vc to jconfig.h and makefiles to project files.
   	   (Note that the renaming is critical!)
	 3.Open the solution file jpeg-8d\jpeg.sln
	 4.Build the Release Version of the library project.
	 5.Verify jpeg-8d\Release\jpeg.lib exists and copy it to windows\dependencies\libs
	  in iipsrv sources
	 6.Copy all header files from jpeg-8d to windows\dependencies\includes in iipsrv sources

LIBTIFF (Tested with version 4.0.2)
	1.Acquired from: ftp://ftp.remotesensing.org/pub/libtiff (zip version)
	2.Run NMAKE /f makefile.vc (Start -> Visual Studio Command Prompt, 
	  go to tiff-4.0.2\libtiff folder)
	3.Verify tiff-4.0.2\libtiff\libtiff.lib exists and copy it to windows\dependencies\libs
	  in iipsrv sources
	4.Copy all header files from tiff-4.0.2\libtiff to windows\dependencies\includes
	  in iipsrv sources

ZLIB (Tested with version 1.2.5)
	1.Acquired from: http://www.winimage.com/zLibDll/index.html (source and dll zip versions)
	3.Unzip both to zlib-1.2.5
	4.Verify dll32\zlibwapi.dll exists and copy it to windows\dependencies\dlls
	5.Verify dll32\zlibwapi.lib exists and copy it to windows\dependencies\libs
	6.Verify zlib-1.2.5\zlib.h exists and copy it ti windows\dependencies\includes

KAKADU (version 6.4 was tested)
	(Closer instructions are also in Compiling_Instructions.txt in main folder of Kakadu source)
	1. Open coresys/coresys_2008.sln
	2. Choose version - 32bit/64bit (Project->Properties->Configuration Properties->Platform)
	3. Choose Release and build project
	4. Verify folders lib_x86 and bin_x86 (or x64) were created, contains kdu_v64R.dll, kdu_v64R.lib
	5. Copy previous files to windows\dependencies\dlls and to windows\dependencies\libs
	   in iipsrv sources
	6. Copy header files from apps/compressed_io, apps/jp2 and coresys/common folder in Kakadu source
	   and also stripe_compressor_local.h, stripe_decompressor_local.h, kdu_stripe_compressor.h and 
	   kdu_stripe_decompressor.h from apps/support folder in Kakadu source to windows\dependencies\includes
	   in iipsrv sources and also jp2.cpp, jpx.cpp (apps/jp2 folder in Kakadu source) and 
	   kdu_stripe_compressor.cpp and kdu_stripe_decompressor (apps/support folder in Kakadu source)
	   to windows\dependencies\includes in iipsrv sources

MEMCACHECLIENT (Tested with version 2.0)
	1.Acquired from http://code.jellycan.com/files/memcacheclient-2.0.zip
	2.Unzip it and open MemcacheClient.sln
	3.Transform to 2008/2010 project (Yes)
    	4.In Properties -> C/C++ -> Code Generation -> Runtime Library set to Multi-threaded DLL 
	  (For Debug use Multi-threaded DLL Debug - the same as is set in iipsrv project)
    	5.Build correct version (Release - Win32/x64)
	6.Copy MemcacheClient.lib to windows\dependencies\libs in iipsrv sources 
	7.Copy all header files to windows\dependencies\includes in iipsrv sources