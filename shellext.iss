[Deco]
(*//

#include "C:\Program Files (x86)\Inno Download Plugin\idp.iss"

[Setup]
AppName=shellext
AppVersion=1.0.0.9
AppId={{8545BEA4-83DF-4167-B67D-5FF71162F6D4}
ShowLanguageDialog=no
LanguageDetectionMethod=none
AppPublisher=datadiode
AppPublisherURL=https://github.com/datadiode/shellext
RestartApplications=True
CloseApplicationsFilter=explorer.exe
VersionInfoVersion=1.0.0.9
VersionInfoCompany=datadiode
OutputBaseFilename=shellext-setup
ArchitecturesInstallIn64BitMode=x64
DefaultDirName={pf}\datadiode\shellext
UsePreviousAppDir=no

[Files]
Source: "shellext.png"; DestDir: "{app}"; Flags: ignoreversion
Source: "shellext.ini"; DestDir: "{app}"; Flags: ignoreversion
Source: "MinSizeRel\shellext.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace regserver

[Code]
//*)

{ ---- VCRedist processing code ----- }
// Modified from https://www.stefanobordoni.cloud/howto-include-visual-c-rtl-in-a-inno-setup-kit/
// minimal RTL version required (default: 14.23.27820.0)
// maximal RTL version installable on XP: 14.27.29114.0
// minimal VC2019 RTL version: 14.29.30133.0
// minimal VC2022 RTL version: 14.32.31326.0
const
  VCRTL_MIN_V1 = 14;
  VCRTL_MIN_V2 = 27;
  VCRTL_MIN_V3 = 29114;
  VCRTL_MIN_V4 = 0;

var
  vcrtl_file, vcrtl_url: string;

// check if the needed RTL 32/64 bits is installed (by looking the registry)
function RTL_IsNeeded(bUse64BitsRTL: Boolean): Boolean;
var
  sRegKey: string;
  v1: Cardinal;
  v2: Cardinal;
  v3: Cardinal;
  v4: Cardinal;
begin
  // is it running on 64 bits OS?
  if IsWin64 then begin
    sRegKey := 'SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\';
    // need 64 bits RTL?
    if bUse64BitsRTL then
      sRegKey := sRegKey + 'x64'
    else
      sRegKey := sRegKey + 'x86'
  end
  // else we have only 32 bits RTL on 32 bits OS...
  else if not bUse64BitsRTL then
    sRegKey := 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\X86';
  // if we have a key, let's check it
  if ((sRegKey <> '')
      and RegQueryDWordValue(HKEY_LOCAL_MACHINE, sRegKey, 'Major', v1)
      and RegQueryDWordValue(HKEY_LOCAL_MACHINE, sRegKey, 'Minor', v2)
      and RegQueryDWordValue(HKEY_LOCAL_MACHINE, sRegKey, 'Bld', v3)
      and RegQueryDWordValue(HKEY_LOCAL_MACHINE, sRegKey, 'RBld', v4)) then
  begin
    Log('VC 2015-2022 Redist version: ' + IntToStr(v1) + '.' + IntToStr(v2) + '.' + IntToStr(v3) + '.' + IntToStr(v4));
    { Version info was found. Return true if later or equal to our minimal required version RTL_MIN_Vx }
    Result := not ((v1 > VCRTL_MIN_V1) or ((v1 = VCRTL_MIN_V1) and ((v2 > VCRTL_MIN_V2) or ((v2 = VCRTL_MIN_V2) and ((v3 > VCRTL_MIN_V3) or ((v3 = VCRTL_MIN_V3) and (v4 >= VCRTL_MIN_V4)))))));
  end
  else
    Result := TRUE;
end;

procedure InitializeWizard();
var
  ver: TWindowsVersion;
begin
  GetWindowsVersionEx(ver);
  if RTL_IsNeeded(Is64BitInstallMode) then begin
    case Format('%d%d', [ver.Major < 6, Is64BitInstallMode]) of
      '00': vcrtl_url := 'https://aka.ms/vs/17/release/VC_redist.x86.exe';
      '01': vcrtl_url := 'https://aka.ms/vs/17/release/VC_redist.x64.exe';
      '10': vcrtl_url := 'http://download.visualstudio.microsoft.com/download/pr/56f631e5-4252-4f28-8ecc-257c7bf412b8/D305BAA965C9CD1B44EBCD53635EE9ECC6D85B54210E2764C8836F4E9DEFA345/VC_redist.x86.exe#/EC3BEE79A85AE8E3581A8C181B336D1E/';
      '11': vcrtl_url := 'http://download.visualstudio.microsoft.com/download/pr/722d59e4-0671-477e-b9b1-b8da7d4bd60b/591CBE3A269AFBCC025681B968A29CD191DF3C6204712CBDC9BA1CB632BA6068/VC_redist.x64.exe#/BC8E3E714B727B3BB18614BD6A51A3D3/';
    end;
    vcrtl_file := ExpandConstant('{tmp}\VC_redist.exe');
    idpAddFile(vcrtl_url, vcrtl_file);
  end;
  idpDownloadAfter(wpReady);
end;

function PrepareToInstall(var NeedsRestart: Boolean): string;
var
  ret_code: Integer;
begin
  try
    // Inno Settup 5 lacks GetSHA256OfFile(), therefore resort to GetMD5OfFile() and rely on the MD5 occurring within the URL's fragment portion
    if (Pos('http:', vcrtl_url) = 1) and (Pos('/' + UpperCase(GetMD5OfFile(vcrtl_file)) + '/', vcrtl_url) = 0) then
      Result := 'VC++ runtime setup is not genuine.'
    else if not ((vcrtl_file = '') or Exec(vcrtl_file, '/install /passive /norestart', '', SW_SHOW, ewWaitUntilTerminated, ret_code)) and (ret_code <> 0) and (ret_code <> 1638) and (ret_code <> 3010) then
      Result := Format('VC++ runtime installation failed (code %d).', [ret_code])
    else if ret_code = 3010 then
      NeedsRestart := True;
    Log(Format('- done : %u', [ret_code]));
  except
    Result := AddPeriod(GetExceptionMessage);
  end;
  if Result = '' then
    RenameFile(ExpandConstant('{app}\shellext.dll'), ExpandConstant('{tmp}\shellext.dll.old'))
end;
