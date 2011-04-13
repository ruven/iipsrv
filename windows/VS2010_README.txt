The Solution (ipsrv.sln) and Project (IIPSrv.vcxproj) files are for Visual Studio 2010.

The project links to the libraries in the $(ProjectDir)\dependencies directory. Thus:

C/C++ ->General->Additional Include Directories = $(ProjectDir)\dependencies\includes
Linker->General->Additional Library Directories = $(ProjectDir)\dependencies\libs
Linker->Input  ->Additional Dependencies        = jpeg.lib;libfcgi.lib;libtiff.lib;zlibwapi.lib;

If you decide to build your own dependencies, you will need to modify the project accordingly to link to your *.lib and *.h files.

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