Download Qt Libraries here: http://qt-project.org/downloads
(I have used Qt libraries 4.8.4 for Windows (VS 2010, 234 MB)
- http://releases.qt-project.org/qt4/source/qt-win-opensource-4.8.4-vs2010.exe)
Copy QtCore4.dll and QtGui4.dll into JPEG2000Transcoder folder. 

Build kdu_compress with your kakadu and copy it to 32/64 bit version.
We can't provide kakadu binaries because of their license.


NEXT IS IMPORTANT ONLY IF YOU PLAN TO REBUILD SOME OF FOLLOWING BINARIES

->GUI WRAPPER
Jpeg2000Transcoder is utility for transcoding image files into jp2 format.
Project source is here: https://github.com/moravianlibrary/JPEG2000-Transcoder
It was build as 32bit application only. (Both in 32 and 64 bit installer)
If you encounter any problem with compiling, try to download Qt libraries 4.8.x for VS2010
and replace every occurence of $(QTDIR) in Jpeg2000Transcoder.vcprojx with the path to your libraries.

->BINARIES
djpeg.exe was built 32 and 64 bit, both with VC10.
kdu_compres.exe was built 32 and 64 bit, both with VC10, I have used Kakadu 6.4.
kdu_compress.exe needs kakadu dll (kdu_v64.dll) and it takes it from Porgram_Files folder,
so kdu_compress and iipsrv.exe must be build with the same version of kakadu.