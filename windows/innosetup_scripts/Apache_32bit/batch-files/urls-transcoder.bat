REM Create url file for viewing images
echo [InternetShortcut]^

URL=http://localhost/%~1/index.html > "%~2\IIPImage MooViewer 2.0.url"
REM Complete configuration.ini file for Jpeg2000Transcoder
echo [OutputFolder]^

name=%3^

[IIPImageURL]^

url="http://localhost/%~1/index.html" >> %4\configuration.ini
REM Open test image in browser (command may have problem with virt. directory with spaces in name)
start http://localhost/%~1/index.html