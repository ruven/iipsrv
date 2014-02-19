REM Install memcached as windows service and start it
%1\memcached.exe -d install
%1\memcached.exe -d start
REM Rename iipsrv.exe to iipsrv.fcgi
REN %2\iipsrv.exe iipsrv.fcgi