REM Install memcached as windows service and start it
%5\memcached.exe -d install
%5\memcached.exe -d start
REM Configuration of fcgi settings (Adding application)
%windir%\system32\inetsrv\appcmd.exe set config -section:system.webServer/fastCgi /+"[fullPath='%~1\iipsrv.exe',arguments='',maxInstances='4',idleTimeout='300',activityTimeout='30',requestTimeout='90',instanceMaxRequests='200',protocol='NamedPipe',flushNamedPipe='False']" /commit:apphost
%windir%\system32\inetsrv\appcmd.exe set config -section:system.webServer/fastCgi /+"[fullPath='%~1\iipsrv.exe'].environmentVariables.[name='FILESYSTEM_PREFIX',value='%~4\']" /commit:apphost
REM Creation of virtual directory
%windir%\system32\inetsrv\appcmd.exe add vdir /app.name:"Default Web Site"/ /path:/%2 /physicalPath:%3
REM Unlocking handlers for web.config
%windir%\system32\inetsrv\appcmd.exe unlock config -section:system.webServer/handlers
REM Making web.config
%windir%\system32\inetsrv\appcmd.exe set config "Default Web Site/%~2" /section:handlers /+"[name='IIPImageFcgiHandler',path='*.fcgi',verb='*',modules='FastCgiModule',scriptProcessor='%~1\iipsrv.exe',resourceType='Unspecified',requireAccess='Execute']"
REM Setting access policy for handlers for web.config
%windir%\system32\inetsrv\appcmd.exe set config "Default Web Site/%~2" /section:handlers /accessPolicy:Read,Script,Execute
REM Now start Default Web Site, if not already
%windir%\system32\inetsrv\appcmd.exe start site /site.name:"Default Web Site"
