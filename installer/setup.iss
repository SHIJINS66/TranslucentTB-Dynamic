#define ProductName "TranslucentTB Dynamic"
#define ProductVersion "2026.2.5-rc1"
#ifndef ReleaseDir
#define ReleaseDir SourcePath + "\..\..\outputs\TranslucentTB-Dynamic-2026.2.5-rc1"
#endif

[Setup]
#ifdef TestBuild
AppId={{AA51C501-8688-4E25-B6B1-29D1EA213730}
AppName={#ProductName} Installer Test
OutputDir={#SourcePath}\test-build
OutputBaseFilename=Setup-Test
#else
AppId={{CFB772B2-A4DF-4B92-9F83-A85DE4C1D26A}
AppName={#ProductName}
AppMutex=344635E9-9AE4-4E60-B128-D53E25AB70A7
OutputDir={#ReleaseDir}
OutputBaseFilename=TranslucentTB-Dynamic-2026.2.5-rc1-win11-x64-Setup
#endif
AppVersion={#ProductVersion}
AppVerName={#ProductName} {#ProductVersion} 开发预览版
AppPublisher=TranslucentTB Dynamic community build
VersionInfoVersion=2026.2.5.0
VersionInfoDescription=TranslucentTB Dynamic community installer
VersionInfoCopyright=Original TranslucentTB contributors; modified build 2026-11-12
DefaultDirName={localappdata}\Programs\TranslucentTBDynamic
#ifdef TestBuild
DefaultGroupName=TranslucentTB Dynamic Installer Test
#else
DefaultGroupName={#ProductName}
#endif
PrivilegesRequired=lowest
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0.22000
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
SetupIconFile={#SourcePath}\stage\app\app.ico
UninstallDisplayIcon={app}\TranslucentTB.exe
InfoBeforeFile={#SourcePath}\stage\app\INSTALL-NOTES.txt
DisableProgramGroupPage=yes
DisableDirPage=no
CloseApplications=no
RestartApplications=no
UninstallLogMode=append
SetupLogging=yes
ChangesEnvironment=no
SetupMutex=TranslucentTBDynamicSetup

[Languages]
Name: "chinesesimp"; MessagesFile: "{#SourcePath}\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
chinesesimp.DesktopShortcut=创建桌面快捷方式
chinesesimp.AutoStart=登录 Windows 后自动启动（可选）
chinesesimp.RunApplication=启动 TranslucentTB Dynamic
chinesesimp.PrereqFailure=无法准备微软运行时。安装未继续，请查看安装日志中的详细错误。
chinesesimp.BackupFailure=无法备份已有程序或配置。为保护已有文件，安装已停止。
chinesesimp.EditSettings=编辑动态取色配置
chinesesimp.UninstallApp=卸载 TranslucentTB Dynamic
english.DesktopShortcut=Create a desktop shortcut
english.AutoStart=Start automatically when signing in to Windows (optional)
english.RunApplication=Launch TranslucentTB Dynamic
english.PrereqFailure=Microsoft runtime preparation failed. Setup did not continue. See the setup log for details.
english.BackupFailure=Could not back up existing files. Setup stopped to protect the current installation.
english.EditSettings=Edit dynamic tint settings
english.UninstallApp=Uninstall TranslucentTB Dynamic

[Tasks]
Name: "desktopicon"; Description: "{cm:DesktopShortcut}"; Flags: unchecked
Name: "autostart"; Description: "{cm:AutoStart}"; Flags: unchecked

[Files]
Source: "{#SourcePath}\stage\app\*"; DestDir: "{app}"; Excludes: "settings.json"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourcePath}\stage\app\settings.json"; DestDir: "{app}"; Flags: onlyifdoesntexist uninsneveruninstall
Source: "{#ReleaseDir}\TranslucentTB-Dynamic-2026.2.5-rc1-Source.zip"; DestDir: "{app}"; DestName: "Source.zip"; Flags: ignoreversion
Source: "{#SourcePath}\stage\prerequisites\*"; Flags: dontcopy

[Icons]
Name: "{group}\{#ProductName}"; Filename: "{app}\TranslucentTB.exe"; WorkingDir: "{app}"
Name: "{group}\{cm:EditSettings}"; Filename: "{sys}\notepad.exe"; Parameters: """{app}\settings.json"""; WorkingDir: "{app}"
Name: "{group}\{cm:UninstallApp}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#ProductName}"; Filename: "{app}\TranslucentTB.exe"; WorkingDir: "{app}"; Tasks: desktopicon
Name: "{userstartup}\TranslucentTB Dynamic"; Filename: "{app}\TranslucentTB.exe"; WorkingDir: "{app}"; Tasks: autostart

[Run]
#ifndef TestBuild
Filename: "{app}\TranslucentTB.exe"; Description: "{cm:RunApplication}"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent
#endif

[Code]
function SetEnvironmentVariable(lpName, lpValue: String): Boolean;
external 'SetEnvironmentVariableW@kernel32.dll stdcall';
function GetTickCount: LongWord;
external 'GetTickCount@kernel32.dll stdcall';

function BackupDirectory(const SourceDir, DestDir: String): Boolean;
var
  FindRec: TFindRec;
  SourceName, DestName: String;
begin
  Result := True;
  if not DirExists(SourceDir) then Exit;
  if not ForceDirectories(DestDir) then begin Result := False; Exit; end;
  if FindFirst(AddBackslash(SourceDir) + '*', FindRec) then begin
    try
      repeat
        if (FindRec.Name <> '.') and (FindRec.Name <> '..') then begin
          SourceName := AddBackslash(SourceDir) + FindRec.Name;
          DestName := AddBackslash(DestDir) + FindRec.Name;
          if (FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
            Result := BackupDirectory(SourceName, DestName)
          else
            Result := CopyFile(SourceName, DestName, True);
          if not Result then Exit;
        end;
      until not FindNext(FindRec);
    finally
      FindClose(FindRec);
    end;
  end;
end;

function BackupExistingInstallation: Boolean;
var
  FindRec: TFindRec;
  BackupRoot, SourceName: String;
begin
  Result := True;
  if not DirExists(ExpandConstant('{app}')) then Exit;
  BackupRoot := ExpandConstant('{localappdata}\TranslucentTBDynamic\Backups\') + GetDateTimeString('yyyymmdd-hhnnss', '-', ':') + '-' + IntToStr(GetTickCount);
  if not ForceDirectories(BackupRoot) then begin Result := False; Exit; end;
  { Back up existing top-level files; only owned subdirectories are overwritten. }
  if FindFirst(ExpandConstant('{app}\*'), FindRec) then begin
    try
      repeat
        if (FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY) = 0 then begin
          SourceName := ExpandConstant('{app}\') + FindRec.Name;
          if not CopyFile(SourceName, AddBackslash(BackupRoot) + FindRec.Name, True) then begin
            Result := False; Exit;
          end;
        end;
      until not FindNext(FindRec);
    finally
      FindClose(FindRec);
    end;
  end;
  Result := BackupDirectory(ExpandConstant('{app}\Assets'), BackupRoot + '\Assets') and
    BackupDirectory(ExpandConstant('{app}\ThirdPartyNotices'), BackupRoot + '\ThirdPartyNotices');
  if Result then Log('Existing installation backed up to: ' + BackupRoot);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Command, Report: AnsiString;
  ResultCode: Integer;
begin
  Result := '';
  if not BackupExistingInstallation then begin Result := CustomMessage('BackupFailure'); Exit; end;
  ExtractTemporaryFile('Microsoft.VCLibs.x64.14.00.appx');
  ExtractTemporaryFile('Microsoft.VCLibs.x64.14.00.Desktop.appx');
  ExtractTemporaryFile('Microsoft.UI.Xaml.2.8.appx');
  ExtractTemporaryFile('prerequisites.b64');
  if not LoadStringFromFile(ExpandConstant('{tmp}\prerequisites.b64'), Command) then begin
    Result := CustomMessage('PrereqFailure'); Exit;
  end;
  SetEnvironmentVariable('TTBDynamicPrereqRoot', ExpandConstant('{tmp}'));
  SetEnvironmentVariable('TTBDynamicPrereqCheckOnly', '0');
  if not Exec(ExpandConstant('{sysnative}\WindowsPowerShell\v1.0\powershell.exe'),
    '-NoProfile -NonInteractive -EncodedCommand ' + String(Command), '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then begin
    Result := CustomMessage('PrereqFailure'); Exit;
  end;
  if LoadStringFromFile(ExpandConstant('{tmp}\prerequisites-result.json'), Report) then
    Log(String(Report));
  if ResultCode <> 0 then Result := CustomMessage('PrereqFailure');
end;
