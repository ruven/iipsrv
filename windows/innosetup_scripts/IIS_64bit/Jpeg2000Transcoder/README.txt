Add JPEG2000Transcoder files here:
Jpeg2000Transcoder.exe, kdu_compress.exe,
kdu_v64R.dll (or appropriate dll to kdu_compress),
QtCore4.dll, QtGui4.dll (or appropriate versions of Qt dlls),
msvcp100.dll, msvcr100.dll (32bit version, if you won't build 64bit
Transcoder).
In Moravian Library installer, we have built 32bit Transcoder.

Also you can update configuration.ini, but don't write there
information about IIPImage Images directory or IIPImage url.
These are added in batch file index-Moo.bat

MSVC dlls (msvcp, msvcr), are taken from Program Files - iipsrv
binaries for 32bit version, but in 64bit version you must include
here 32bit MSVC dlls, unless you built 64bit Transcoder.