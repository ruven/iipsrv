Add here installer for Apache Server.
We have used 2.2.22 Apache Server with installer apache_2.2.22-x64-no-ssl.msi.
If you will use another, change it in Setup and Code section of installer scripts.
If you will use another version (not 2.2.22) also check CurUninstallStepChanged and
NextButtonClicked procedures in scripts (changes to registry calls will be needed).

Also add here mod_fcgid.so with correct bit version (you have to build it for x64)
http://www.apachelounge.com/download/