REM add module and include to httpd.conf (main apache configuration file)
echo.>> %5\conf\httpd.conf
echo LoadModule fcgid_module modules/mod_fcgid.so >> %5\conf\httpd.conf
echo.>> %5\conf\httpd.conf
echo ^<IfModule fcgid_module^> >> %5\conf\httpd.conf
echo # IIPImageServer configuration directory >> %5\conf\httpd.conf
echo Include conf/extra/httpd-iipimage.conf >> %5\conf\httpd.conf
echo ^</IfModule^> >> %5\conf\httpd.conf
echo.>> %5\conf\httpd.conf
REM create apache configuration file for iipImage
echo # Settings for user home directories > %5\conf\extra\httpd-iipimage.conf
echo # Required module: mod_fcgid >> %5\conf\extra\httpd-iipimage.conf
echo # Create a directory for the iipsrv binary >> %5\conf\extra\httpd-iipimage.conf
echo ScriptAlias /fcgi-bin %~8 >> %5\conf\extra\httpd-iipimage.conf
echo # Set the options on that directory >> %5\conf\extra\httpd-iipimage.conf
echo ^<Directory %8^> >> %5\conf\extra\httpd-iipimage.conf
echo AllowOverride None >> %5\conf\extra\httpd-iipimage.conf
echo Options ExecCGI >> %5\conf\extra\httpd-iipimage.conf
echo Order allow,deny >> %5\conf\extra\httpd-iipimage.conf
echo Allow from all >> %5\conf\extra\httpd-iipimage.conf
echo # Set the module handler >> %5\conf\extra\httpd-iipimage.conf
echo AddHandler fcgid-script .fcgi >> %5\conf\extra\httpd-iipimage.conf
echo ^</Directory^> >> %5\conf\extra\httpd-iipimage.conf
echo # Set our environment variables for the IIP server >> %5\conf\extra\httpd-iipimage.conf
echo DefaultInitEnv FILESYSTEM_PREFIX "%~4/" >> %5\conf\extra\httpd-iipimage.conf
echo # Define the idle timeout as unlimited and the number of processes we want >> %5\conf\extra\httpd-iipimage.conf
echo FcgidIdleTimeout 0 >> %5\conf\extra\httpd-iipimage.conf
echo FcgidMaxProcessesPerClass 1 >> %5\conf\extra\httpd-iipimage.conf
echo.>> %5\conf\extra\httpd-iipimage.conf
echo #Rewrite rules - uncomment next 2 lines to allow  rewriting >> %5\conf\extra\httpd-iipimage.conf
echo #try to enter localhost/demo/preview.jpg for testing (localhost may be replaced by your domain) >> %5\conf\extra\httpd-iipimage.conf
echo #you may edit and add rewrite rules if you want, but don't forget to include [PT] flag >> %5\conf\extra\httpd-iipimage.conf
echo.>> %5\conf\extra\httpd-iipimage.conf
echo #LoadModule rewrite_module modules/mod_rewrite.so
echo #RewriteEngine on >> %5\conf\extra\httpd-iipimage.conf
echo #RewriteRule ^([a-zA-Z0-9_\/\.\-]*)\/preview.jpg$ /fcgi-bin/iipsrv.fcgi?FIF=$1.jp2^&hei=128^&cvt=jpeg [PT] >> %5\conf\extra\httpd-iipimage.conf
echo.>> %5\conf\extra\httpd-iipimage.conf
echo #Directory for testing >> %5\conf\extra\httpd-iipimage.conf
echo Alias /%~3 %2 >> %5\conf\extra\httpd-iipimage.conf
REM Create hidden junction point on root pointing to application directory, so mod_fcgid won't have problem with spaces
%~6\junction.exe %7 %1 /accepteula
attrib +h %7 /L
REM Restart server to load new settings
"%~5\bin\httpd.exe" -w -n "Apache2.2" -k restart