Dependencies currently used:

1. fcgi-2.4.1
2. jpeg-8c
3. tiff-3.9.4
4. zlib-1.2.5

Read BUILDING_DEPENDENCIES.txt for information how to build them.

Copy the include, lib and dll files from these libraries into the 
appropriate directories here in the windows/dependencies folder
(windows/dependencies/dlls, windows/dependencies/libs, 
windows/dependencies/includes).

Once you have built the iipsrv project, the *.dlls will need to be 
copied into your fcgi-bin alongside iipsrv.fcgi
