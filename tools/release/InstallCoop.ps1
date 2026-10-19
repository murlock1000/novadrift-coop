param(
 [string]$GameDirectory,
 [string]$Destination=(Join-Path ([Environment]::GetFolderPath('UserProfile')) 'Games\NovaDriftCoop'),
 [string]$PackageRoot=$env:NOVACOOP_PACKAGE
)
$ErrorActionPreference='Stop'
if(-not $PackageRoot){$PackageRoot=$PSScriptRoot}
function FullPath([string]$path){return [IO.Path]::GetFullPath($path).TrimEnd('\')}
function Under([string]$child,[string]$parent){return $child.Equals($parent,[StringComparison]::OrdinalIgnoreCase) -or $child.StartsWith($parent+'\',[StringComparison]::OrdinalIgnoreCase)}
function Hash([string]$path){return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
function RejectLinks([string]$path){
 $item=Get-Item -LiteralPath $path
 if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Linked folders are not supported: $path"}
 foreach($child in Get-ChildItem -LiteralPath $path -Recurse -Force){if($child.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Linked files/folders are not supported: $($child.FullName)"}}
}
$PackageRoot=FullPath $PackageRoot
$manifest=Get-Content -LiteralPath (Join-Path $PackageRoot 'release.json') -Raw | ConvertFrom-Json
foreach($file in $manifest.files){
 $path=FullPath (Join-Path $PackageRoot $file.path)
 if(-not (Under $path $PackageRoot) -or -not (Test-Path -LiteralPath $path -PathType Leaf) -or (Hash $path) -ne $file.sha256){throw "Package is incomplete or modified: $($file.path). Extract the ZIP again."}
}
if(-not $GameDirectory){
 $roots=@()
 foreach($key in @('HKCU:\Software\Valve\Steam','HKLM:\SOFTWARE\WOW6432Node\Valve\Steam')){
  $steam=Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue
  if($steam.SteamPath){$roots+=$steam.SteamPath};if($steam.InstallPath){$roots+=$steam.InstallPath}
 }
 foreach($root in @($roots)){
  $libraries=Join-Path $root 'steamapps\libraryfolders.vdf'
  if(Test-Path -LiteralPath $libraries){foreach($match in [regex]::Matches([IO.File]::ReadAllText($libraries),'"path"\s*"([^"\r\n]+)"')){$roots+=$match.Groups[1].Value.Replace('\\','\')}}
 }
 foreach($root in ($roots | Select-Object -Unique)){
  $candidate=Join-Path $root 'steamapps\common\Nova Drift'
  if(Test-Path -LiteralPath (Join-Path $candidate 'NovaDrift.exe')){$GameDirectory=$candidate;break}
 }
 if(-not $GameDirectory){$GameDirectory=(Read-Host 'Nova Drift Steam folder (the folder containing NovaDrift.exe)').Trim('"')}
}
$GameDirectory=FullPath $GameDirectory;$Destination=FullPath $Destination
if(-not (Test-Path -LiteralPath (Join-Path $GameDirectory 'NovaDrift.exe')) -or (Hash (Join-Path $GameDirectory 'NovaDrift.exe')) -ne $manifest.game_sha256){throw 'Requires an unmodified Nova Drift 1.2.38.4 Steam installation.'}
foreach($path in @($GameDirectory,$PackageRoot)){if((Under $Destination $path) -or (Under $path $Destination)){throw 'Choose a destination separate from the game and extracted download.'}}
RejectLinks $GameDirectory
if(Test-Path -LiteralPath $Destination){
 if(-not (Test-Path -LiteralPath (Join-Path $Destination '.novadriftcoop-owned'))){throw 'Destination already exists and is not an owned co-op installation. Choose another folder.'}
 RejectLinks $Destination
}
$parent=[IO.DirectoryInfo]$Destination
while($parent){if($parent.Exists -and ($parent.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Destination cannot be inside a linked folder.'};$parent=$parent.Parent}
$running=@(Get-CimInstance Win32_Process | Where-Object {$_.ExecutablePath -and (Under $_.ExecutablePath $Destination)})
if($running.Count){throw 'Close the co-op game, control editor and Steam helper before installing or updating.'}
Write-Host "Installing Nova Drift Co-op $($manifest.version) to $Destination"
$cache=Join-Path ([IO.Path]::GetTempPath()) ('NovaDriftCoop-'+[guid]::NewGuid().ToString())
$null=New-Item -ItemType Directory -Path $cache
try{
 [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
 foreach($dep in $manifest.dependencies){
  $download=Join-Path $cache ([IO.Path]::GetFileName($dep.destination))
  Write-Host "Downloading $([IO.Path]::GetFileName($download))..."
  Invoke-WebRequest -UseBasicParsing -Uri $dep.url -OutFile $download
  if((Hash $download) -ne $dep.sha256){throw "Dependency checksum mismatch: $($dep.destination)"}
 }
 $runtime=Join-Path $Destination 'runtime'
 $null=New-Item -ItemType Directory -Path $runtime -Force
 [IO.File]::WriteAllText((Join-Path $Destination '.novadriftcoop-owned'),"Nova Drift co-op isolated installation; original game lives in Steam.`r`n")
 Write-Host 'Copying game files...'
 foreach($file in Get-ChildItem -LiteralPath $GameDirectory -Force){if($file.Name -ne 'mods'){Copy-Item -LiteralPath $file.FullName -Destination $runtime -Recurse -Force}}
 foreach($folder in @('Local','Roaming','Documents','Temp')){$null=New-Item -ItemType Directory -Path (Join-Path $Destination ('test-profile\'+$folder)) -Force}
 $profile=Join-Path $Destination 'test-profile\Local\NovaDrift'
 if(-not (Test-Path -LiteralPath $profile)){
  $original=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'NovaDrift'
  if(Test-Path -LiteralPath $original){Copy-Item -LiteralPath $original -Destination $profile -Recurse}else{$null=New-Item -ItemType Directory -Path $profile}
 }
 foreach($file in Get-ChildItem -LiteralPath (Join-Path $PackageRoot 'payload') -Recurse -File){
  $relative=$file.FullName.Substring((Join-Path $PackageRoot 'payload').Length+1)
  $target=Join-Path $Destination $relative
  $null=New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($target)) -Force
  Copy-Item -LiteralPath $file.FullName -Destination $target -Force
 }
 foreach($dep in $manifest.dependencies){
  if($dep.destination -eq 'AuriePatcher.exe'){continue}
  $target=Join-Path $runtime $dep.destination
  $null=New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($target)) -Force
  Copy-Item -LiteralPath (Join-Path $cache ([IO.Path]::GetFileName($dep.destination))) -Destination $target -Force
 }
 $exe=Join-Path $runtime 'NovaDrift.exe';$loader=Join-Path $runtime 'mods\AurieCore.dll'
 & (Join-Path $cache 'AuriePatcher.exe') $exe $loader install
 if($LASTEXITCODE -ne 0){throw 'Mod loader setup failed.'}
 if(-not (Test-Path -LiteralPath (Join-Path $runtime 'p2-controls.ini'))){
  & (Join-Path $runtime 'NovaDriftP2Controls.exe') --write-defaults (Join-Path $runtime 'p2-controls.ini')
  if($LASTEXITCODE -ne 0){throw 'Could not create default controls.'}
 }
 Write-Host "Installed. Open $Destination\PlayCoop.cmd and start a fresh run."
 Write-Host 'For Steam Remote Play, open ConfigureRemotePlay.cmd and follow README.RemotePlay.md.'
}finally{Remove-Item -LiteralPath $cache -Recurse -Force}
