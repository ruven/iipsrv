; INSTALLER for IIPIMAGE JPEG2000 x86 with Kakadu and Memcached for WinXP
; Virtual directory is installed in apache htdocs folder and application folder
; is symlinked (junction point) because of mod_fcgid bug with spaces in path
; NOTE: if changing apache server installer, make sure to update Code section,
; because it looks for apache version 2.2.22 in registry
[Setup]
PrivilegesRequired=admin
AppName=IIPImage JPEG2000
AppVersion=0.9.9
DefaultDirName={pf32}\IIPImage JPEG2000
DefaultGroupName=IIPImage JPEG2000
UninstallDisplayIcon={app}\iipsrv.fcgi
LicenseFile={#file AddBackslash(SourcePath) + "IIPServerLicense.rtf"}
Compression=lzma2
SolidCompression=yes
AllowNoIcons=no
OutputDir=userdocs:Inno Setup Examples Output\IIPImage JPEG2000\Apache version
OutputBaseFileName=IIPImage_Apache_x86
;maximal resolution 164x314, set is 163x314 creating 1 pixel border 
WizardImageFile=wizardImage.bmp
WizardImageStretch=no
;color which creates border
WizardImageBackColor=$A0A0A0
;max resolution 55x58 pixels
WizardSmallImageFile=wizardSmallImage.bmp

[Dirs]
;directories for images and Logs
Name: "{code:GetImagesDir}"
Name: "{app}\Logs"

[Files]
;Apache server
Source: "apache\httpd-2.2.22-win32-x86-no_ssl.msi";  Flags: dontcopy
Source: "apache\mod_fcgid.so"; DestDir: "{code:GetApacheDir}\modules"
Source: "ProgramFiles\msvcp90.dll"; DestDir: "{code:GetApacheDir}\modules"
Source: "ProgramFiles\msvcr90.dll"; DestDir: "{code:GetApacheDir}\modules"
;Program files - fcgi,dlls,Readme
Source: "ProgramFiles\iipsrv.fcgi"; DestDir: "{app}"
Source: "ProgramFiles\kdu_v64R.dll"; DestDir: "{app}"
Source: "ProgramFiles\libfcgi.dll"; DestDir: "{app}"
Source: "ProgramFiles\msvcp90.dll"; DestDir: "{app}"
Source: "ProgramFiles\msvcr90.dll"; DestDir: "{app}"
Source: "Readme.rtf"; DestDir: "{app}"; Flags: isreadme
;Virtual directory files 
Source: "virtual-directory\MooViewer2.0\css\ie.css"; DestDir: "{code:GetVirtDir}\MooViewer2.0\css"
Source: "virtual-directory\MooViewer2.0\css\iip.css"; DestDir: "{code:GetVirtDir}\MooViewer2.0\css"
Source: "virtual-directory\MooViewer2.0\images\iip.32x32.png"; DestDir: "{code:GetVirtDir}\MooViewer2.0\images"
Source: "virtual-directory\MooViewer2.0\images\iip.png"; DestDir: "{code:GetVirtDir}\MooViewer2.0\images"
Source: "virtual-directory\MooViewer2.0\images\iip-favicon.png"; DestDir: "{code:GetVirtDir}\MooViewer2.0\images"
Source: "virtual-directory\MooViewer2.0\images\reset.png"; DestDir: "{code:GetVirtDir}\MooViewer2.0\images"
Source: "virtual-directory\MooViewer2.0\images\reset.svg"; DestDir: "{code:GetVirtDir}\MooViewer2.0\images"
Source: "virtual-directory\MooViewer2.0\images\zoomIn.png"; DestDir: "{code:GetVirtDir}\MooViewer2.0\images"
Source: "virtual-directory\MooViewer2.0\images\zoomIn.svg"; DestDir: "{code:GetVirtDir}\MooViewer2.0\images"
Source: "virtual-directory\MooViewer2.0\images\zoomOut.png"; DestDir: "{code:GetVirtDir}\MooViewer2.0\images"
Source: "virtual-directory\MooViewer2.0\images\zoomOut.svg"; DestDir: "{code:GetVirtDir}\MooViewer2.0\images"
Source: "virtual-directory\MooViewer2.0\javascript\iipmooviewer-2.0.js"; DestDir: "{code:GetVirtDir}\MooViewer2.0\javascript"
Source: "virtual-directory\MooViewer2.0\javascript\mootools-core-1.3.2-full-nocompat.js"; DestDir: "{code:GetVirtDir}\MooViewer2.0\javascript"
Source: "virtual-directory\MooViewer2.0\javascript\mootools-more-1.3.2.1.js"; DestDir: "{code:GetVirtDir}\MooViewer2.0\javascript"
Source: "virtual-directory\MooViewer2.0\javascript\protocols.js"; DestDir: "{code:GetVirtDir}\MooViewer2.0\javascript"
Source: "virtual-directory\index.html"; DestDir: "{code:GetVirtDir}"
Source: "virtual-directory\IIPImage MooViewer 2.0.url"; DestDir: "{code:GetVirtDir}"
Source: "virtual-directory\IIPImage Project Website.url"; DestDir: "{code:GetVirtDir}"
Source: "virtual-directory\Online Demo.url"; DestDir: "{code:GetVirtDir}"
;Images directory files - demo image
Source: "Images\demo.jp2"; DestDir: "{code:GetImagesDir}"
;JPEG2000Transcoder files
Source: "Jpeg2000Transcoder\Jpeg2000Transcoder.exe"; DestDir: "{code:GetTranscoderDir}\Jpeg2000Transcoder"
Source: "Jpeg2000Transcoder\djpeg.exe"; DestDir: "{code:GetTranscoderDir}\Jpeg2000Transcoder"
Source: "Jpeg2000Transcoder\kdu_compress.exe"; DestDir: "{code:GetTranscoderDir}\Jpeg2000Transcoder"
Source: "Jpeg2000Transcoder\kdu_v64R.dll"; DestDir: "{code:GetTranscoderDir}\Jpeg2000Transcoder"
Source: "Jpeg2000Transcoder\QtCore4.dll"; DestDir: "{code:GetTranscoderDir}\Jpeg2000Transcoder"
Source: "Jpeg2000Transcoder\QtGui4.dll"; DestDir: "{code:GetTranscoderDir}\Jpeg2000Transcoder"
Source: "ProgramFiles\msvcp90.dll"; DestDir: "{code:GetTranscoderDir}\Jpeg2000Transcoder"
Source: "ProgramFiles\msvcr90.dll"; DestDir: "{code:GetTranscoderDir}\Jpeg2000Transcoder"
Source: "Jpeg2000Transcoder\configuration.ini"; DestDir: "{code:GetTranscoderDir}\Jpeg2000Transcoder"
;Memcached
Source: "Memcached\memcached.exe"; DestDir: "{code:GetMemcachedDir}"
;Temporary files - first configures memcached, second apache, third creates urls and configure transcoder
Source: "batch-files\memcached.bat"; DestDir: "{tmp}"
Source: "batch-files\apache-conf.bat"; DestDir: "{tmp}"
Source: "batch-files\urls-transcoder.bat"; DestDir: "{tmp}"
Source: "batch-files\junction.exe"; DestDir: "{tmp}"

[Icons]
Name: "{group}\IIPImage MooViewer 2.0"; Filename: "{code:GetVirtDir}\IIPImage MooViewer 2.0.url"
Name: "{group}\Jpeg2000 Transcoder"; Filename: "{code:GetTranscoderDir}\Jpeg2000Transcoder.exe"
Name: "{group}\Images"; Filename: "{code:GetImagesDir}"
Name: "{group}\Uninstall IIPImage"; Filename: "{app}\unins000.exe"
Name: "{group}\Readme"; Filename: "{app}\Readme.rtf"
Name: "{group}\IIPImage Project Website"; Filename: "{code:GetVirtDir}\IIPImage Project Website.url"
Name: "{group}\Online Demo"; Filename: "{code:GetVirtDir}\Online Demo.url"

[Code]
var
  //global variables - in whole code there is reference to this pages
  ApachePreInstallPage: TWizardPage;
  VirtDirNamePage: TInputQueryWizardPage;
  ImagesPathPage: TInputDirWizardPage;
  MemcachedPathPage: TInputDirWizardPage;
  TranscoderPathPage: TInputDirWizardPage;
  ProgressPage: TOutputProgressWizardPage;
  
  { Creates pages that will be shown in installation wizard }
procedure CreateTheWizardPages;
var StaticText: TNewStaticText;
begin
  ApachePreInstallPage := CreateCustomPage(wpWelcome, 'Apache installation information',
  'Information for smooth installation of apache http server.');
  StaticText := TNewStaticText.Create(ApachePreInstallPage);
  StaticText.Caption := 'You must install apache http server version 2.2.22 before continue.'#13
  +'Please use typical installation if you are not advanced apache user.'#13
  +'Default installation (clicking only Next buttons) should work fine.'#13
  +'Now click Next button to start installation of apache server.';
  StaticText.Parent := ApachePreInstallPage.Surface;
  StaticText.WordWrap := True;

  VirtDirNamePage := CreateInputQueryPage(wpSelectDir,
    'Name of virtual directory', 'What will be name of directory?',
    'Please specify name for your virtual directory, it will be included in URL for all accesses to ImageServer.');
  VirtDirNamePage.Add('Name of virtual directory:', False);
  VirtDirNamePage.Values[0] := 'imageserver';
  
  MemcachedPathPage := CreateInputDirPage(VirtDirNamePage.ID,
    'Select directory for Memcached', 'Where should be Memcached installed?',
    'Select folder where will be installed Memcached, cache server improving performance of IIPImage, then click Next',
    True, 'Memcached');
  MemcachedPathPage.Add('Memcached directory:');
  MemcachedPathPage.Values[0] := ExpandConstant('{pf32}\Memcached');
  
  TranscoderPathPage := CreateInputDirPage(MemcachedPathPage.ID,
    'Select directory for JPEG2000 Transcoder', 'Where should be JPEG2000 Transcoder installed?',
    'Select directory where will be installed JPEG2000 Transcoder, utility for transcoding images to jp2 format',
    True, 'JPEG2000 Transcoder');
  TranscoderPathPage.Add('JPEG2000 Transcoder directory:');
  TranscoderPathPage.Values[0] := ExpandConstant('{pf32}\JPEG2000 Transcoder');
    
  ImagesPathPage := CreateInputDirPage(TranscoderPathPage.ID,
    'Select physical path for Images directory', 'Where should be stored images?',
    'Select folder where will be stored images, then click Next',
    True, 'IIPImage Images');
  ImagesPathPage.Add('Images directory:');
  ImagesPathPage.Values[0] := ExpandConstant('{commondocs}\IIPImage Images');
end;

{ Getters for directories and names set by user in installation, needed in Setup section }
function GetImagesDir(Param: String): String;
begin
  Result := ImagesPathPage.Values[0];
end;

function GetApacheDir(Param: String): String;
var ApacheDir: String;
begin
  if RegQueryStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\Apache Software Foundation\Apache\2.2.22',
     'ServerRoot', ApacheDir) then
  begin
    Result := ApacheDir;
  end
  else
  Result := '';
end;

function GetVirtDir(Param: String): String;
begin
  Result := GetApacheDir('')+'htdocs\iipimage';
end;

function GetVirtDirName(Param: String): String;
begin
  Result := VirtDirNamePage.Values[0];
end;

function GetMemcachedDir(Param: String): String;
begin
  Result := MemcachedPathPage.Values[0];
end;

function GetTranscoderDir(Param: String): String;
begin
  Result := TranscoderPathPage.Values[0];
end;

{ Updates information shown just before installation}
function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo,
  MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
var
  S: String;
begin
  S := '';
  S := S + MemoDirInfo + NewLine;
  S := S + NewLine;
  S := S + 'Memcached Directory:' + NewLine;
  S := S + Space + 'Destination:' + MemcachedPathPage.Values[0] + NewLine;
  S := S + NewLine;
  S := S + 'JPEG2000 Transcoder Directory:' + NewLine;
  S := S + Space + 'Destination:' + TranscoderPathPage.Values[0] + NewLine;
  S := S + NewLine;
  S := S + 'Virtual Directory:' + NewLine;
  S := S + Space + 'Name: ' + VirtDirNamePage.Values[0] + NewLine;
  S := S + Space + 'Destination:' + GetVirtDir('') + NewLine;
  S := S + NewLine;
  S := S + 'Images Directory:' + NewLine;
  S := S + Space + 'Destination:' + ImagesPathPage.Values[0] + NewLine;
  S := S + NewLine;
  S := S + MemoGroupInfo + NewLine;
  Result := S;
end;

{ Configuration section }
procedure MyAfterInstall();
var
  ResultCode: Integer; 
  ForwardVirtual,ForwardImages,Junction,ForwardJunction: String;
begin
    // Execute memcached.bat to install memcached as service and start it
    // 1. param - path to memcached directory
    Exec(ExpandConstant('{tmp}\memcached.bat'), 
      '"'+MemcachedPathPage.Values[0]+'"',
       '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    // Execute apache-conf.bat to configure apache server for iipImage
    // 1. param - path to app. directory, 
    // 2. param - path to virtual directory with forward slashes instead of backslashes
    // 3. param - name of virtual directory,
    // 4. param - path to Images directory with forward slashes instead of backslashes,
    // 5. param - path to apache directory
    // 6. param - path to temporary folder, where will be copied junction.exe
    // 7. param - path to junction point, that will be created
    // 8. param - path to junction point with forward slashes
    ForwardVirtual := GetVirtDir('');
    ForwardImages := ImagesPathPage.Values[0];
    StringChangeEx(ForwardVirtual,'\','/',True);//change backslashes to forward slashes in {app}
    StringChangeEx(ForwardImages,'\','/',True);//the same as above just in images directory
    Junction := ExpandConstant('{sd}'+'{\}')+'IIPImageServer';
    ForwardJunction := Junction;
    StringChangeEx(ForwardJunction,'\','/',True);
    Exec(ExpandConstant('{tmp}\apache-conf.bat'), 
      '"'+ExpandConstant('{app}')
      +'" "'+ForwardVirtual
      +'" "'+VirtDirNamePage.Values[0]
      +'" "'+ForwardImages
      +'" "'+GetApacheDir('')
      +'" "'+ExpandConstant('{tmp}')
      +'" "'+Junction
      +'" "'+ForwardJunction+'"',
       '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    // Execute index-Moo.bat to create index.html, url shortcuts and configuration.ini
    // 1. param - name of virtual directory,
    // 2. param - physical path to virtual directory,
    // 3. param - path to Images folder,
    // 4. param - path to transcoder folder
    Exec(ExpandConstant('{tmp}\urls-transcoder.bat'), 
      '"'+VirtDirNamePage.Values[0]
      +'" "'+GetVirtDir('')
      +'" "'+ImagesPathPage.Values[0]
      +'" "'+TranscoderPathPage.Values[0]+'"', 
      '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    //save path to junction point to registry
    RegWriteStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\IIPImage JPEG2000_is1',
    'JunctionPoint', Junction);
end;

{ Start of Installer }
procedure InitializeWizard();
begin
  CreateTheWizardPages;
end;

{ Function after clicking Next button (for apache installer) }
function NextButtonClick(CurPageID: Integer): Boolean;
var UninstallString, PreviousApacheDir: String; ResultCode: Integer; InstallApache: Boolean;
begin
  //IIPIMAGE BRANCH
  if CurPageID = wpWelcome then
  begin;
    // check if previous installation exists
    if (RegValueExists(HKEY_LOCAL_MACHINE,
      'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\IIPImage JPEG2000_is1', 'UninstallString')) then
    begin
      if (MsgBox('Already installed:' #13#13 'The previous IIPImage installation was found. '
                + 'Previous version should be uninstalled before installing new one.'
                + 'Do you want to uninstall previous version now?', mbConfirmation, MB_YESNO) = IDYES) then
      begin
        RegQueryStringValue(HKEY_LOCAL_MACHINE,
          'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\IIPImage JPEG2000_is1',
          'UninstallString', UninstallString);
        Exec('>', UninstallString, '', SW_SHOW, ewWaitUntilTerminated, ResultCode);
        if (ResultCode <> 0) then
        begin
          MsgBox('Uninstallation cancelled:' #13#13 'Uninstallation was cancelled, please uninstall again.',
                mbInformation, MB_OK);
          Abort();
        end;
        MsgBox('Installation continue:' #13#13 'The previous IIPImage installation was removed. '
                + 'Installation may continue.', mbInformation, MB_OK);
      end
      else
      begin
      MsgBox('Installation failed:' #13#13 'Can''t continue installation without removing previous version.',
      mbInformation, MB_OK);
      Abort();
      end;
    end;//end of branch with existing previous IIPImage
  end;//end of wpWelcome Next button click
  //APACHE BRANCH
  if CurPageID = ApachePreInstallPage.ID then
  begin
    InstallApache := True;
     // check if Apache 2.2.22 is already installed
    if RegValueExists(HKEY_LOCAL_MACHINE, 'SOFTWARE\Apache Software Foundation\Apache\2.2.22',
                      'ServerRoot') then
    begin
      RegQueryStringValue(HKEY_LOCAL_MACHINE,
          'SOFTWARE\Apache Software Foundation\Apache\2.2.22', 'ServerRoot', PreviousApacheDir);
      if MsgBox('Previous Apache installation:' #13#13 'The previous IIPImage installation was found in '
          +PreviousApacheDir+'. Do you want to use it for IIPImage?', mbConfirmation, MB_YESNO) = IDYES then
        InstallApache := False;
    end; //end of apache existance check
    if InstallApache = True then
    begin
      ExtractTemporaryFile('httpd-2.2.22-win32-x86-no_ssl.msi');
      if not Exec('msiexec',ExpandConstant('/i "{tmp}\httpd-2.2.22-win32-x86-no_ssl.msi"'), '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode) then
      begin
        MsgBox('Installation failed:' #13#13 'The apache installer could not be executed. '
                + SysErrorMessage(ResultCode) + '. Installation will exit now.', mbError, MB_OK);
        Abort();
      end
      else//installation successfully initialized
      begin
        if(ResultCode <> 0) then
        begin
          MsgBox('Installation failed:' #13#13 'The apache installation unsuccessful, return code: '
              +IntToStr(ResultCode)+'. Try to install apache again.', mbError, MB_OK);
          Abort();    
        end;
        BringToFrontAndRestore();
        MsgBox('Apache server installed:' #13#13 'Installation of IIPImage will start now.', mbInformation, MB_OK);
      end;//end of else
    end;// end of installation of apache
  end;//end of ApachePreInstallPage case branch
  Result := True;
end;

{ After files were copied in appropriate directories }
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then begin
    MyAfterInstall();
  end;
end;

{ Uninstall }
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var ResultCode: Integer; Junction: String;
begin
  if CurUninstallStep = usUninstall then
  begin
    //uninstall apache
    if RegValueExists(HKEY_LOCAL_MACHINE, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{85262A06-2D8C-4BC1-B6ED-5A705D09CFFC}',
        'UninstallString') then
    begin
      if(MsgBox('Apache server uninstallation:' #13#13 'Do you want uninstall Apache Server?',
                       mbConfirmation, MB_YESNO) = IDYES) then
        //this uninstaller can stop started processes without restart, so we don't need to cancel them
        Exec('msiexec.exe', '/x{85262A06-2D8C-4BC1-B6ED-5A705D09CFFC}', 
              '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode);
       
    end;
    //stop memcached so it can be uninstalled and remove memcached service from windows services
    Exec(ExpandConstant('{cmd}'), '/C sc stop "memcached Server"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Exec(ExpandConstant('{cmd}'), '/C sc delete "memcached Server"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    //kill iipsrv.fcgi if running, so it can be uninstalled
    Exec(ExpandConstant('{cmd}'), '/C taskkill /F /IM iipsrv.fcgi', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    //remove junction point
    if RegQueryStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\IIPImage JPEG2000_is1',
        'JunctionPoint', Junction) then
    begin
      Exec(ExpandConstant('{cmd}'), '/C rmdir '+Junction, '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    end;
  end;//end of usUninstall if branch
end;//end of CurUninstallStepChanged
