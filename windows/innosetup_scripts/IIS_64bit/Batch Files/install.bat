@echo off
REM Setting Required rights IIS_IUSRS and Everyone read and execute, for Logs also write
icacls %1 /grant IIS_IUSRS:(OI)(CI)(RX)
icacls %1 /grant Everyone:(OI)(CI)(RX)
icacls %1\Logs /grant IIS_IUSRS:(OI)(CI)(W)
icacls %2 /grant Everyone:(OI)(CI)(RX,WD)
REM Installation of IIS7 and Fast CGI
Start /w pkgmgr /iu:IIS-WebServerRole;IIS-WebServer;IIS-CommonHttpFeatures;IIS-StaticContent;IIS-DefaultDocument;IIS-DirectoryBrowsing;IIS-HttpErrors;IIS-HealthAndDiagnostics;IIS-HttpLogging;IIS-LoggingLibraries;IIS-RequestMonitor;IIS-Security;IIS-RequestFiltering;IIS-HttpCompressionStatic;IIS-WebServerManagementTools;IIS-ManagementConsole;WAS-WindowsActivationService;WAS-ProcessModel;WAS-NetFxEnvironment;WAS-ConfigurationAPI;IIS-CGI
REM Set registry value, telling installer, that script has ended
REG ADD "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\IIPImage JPEG2000_is1" /v IISScript /t REG_BINARY /d 01