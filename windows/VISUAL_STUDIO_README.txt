The Solution (ipsrv-MSVC2010.sln) and Project (IIPSrv.vcxproj) files are for Visual Studio 2010.
The Solution (ipsrv-MSVC2008.sln) and Project (IIPSrv.vcproj) files are for Visual Studio 2008.

The project links to the libraries in the $(ProjectDir)\dependencies directory. Thus:

C/C++ ->General->Additional Include Directories = $(ProjectDir)\dependencies\includes
Linker->General->Additional Library Directories = $(ProjectDir)\dependencies\libs
Linker->Input  ->Additional Dependencies        = jpeg.lib;libfcgi.lib;libtiff.lib;zlibwapi.lib;
For MSVC 2008 use space instead of semicolon (;)

If you don't want Kakadu or Memcached support or want to add some support, change values in C/C++ -> Preprocessor -> Preprocessor Definitions 

If you decide to build your own dependencies, you will need to modify the project accordingly to link to your *.lib and *.h files.
(It means you must add all libraries *.lib to dependencies\libs, all dll files
*.dll to dependencies\dlls and all header files *.h to dependencies\includes)
Read README.txt in dependencies folder for closer information about building dependencies.

Recommended setting for MSVC 2010 - most of them should already be added to project
DEBUG Configuration Settings of Interest:
Linker -> Input         -> Ignore Specific Default Libraries  = MSVCRT;%(IgnoreSpecificDefaultLibraries)
Linker -> Command LIne  -> Additional Options                 = /LTCG
Linker -> Manifest File -> Generate Manifest                  = Yes (/MANIFEST)
Linker -> General       -> Enable Incremental Linking         = No (/INCREMENTAL:NO)

RELEASE Configurstion Settings of Interest:
Linker -> Input         -> Ignore Specific Default Libraries = %(IgnoreSpecificDefaultLibraries)
Linker -> Command LIne  -> Additional Options                = /LTCG
Linker -> Manifest File -> Generate Manifest                 = No (/MANIFEST:NO)
Linker -> General       -> Enable Incremental Linking        = No (/INCREMENTAL:NO)

IMPORTANT NOTE FOR MSVC2008: You must always build manifest! Linker -> Manifest File -> Generate Manifest = Yes