param([switch]$SkipBuild,[string]$AppBuildDirectory='dispatch_cpp\build')
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$payload=Join-Path $PSScriptRoot 'build\payload'
$appBinary=Join-Path (Join-Path $projectRoot $AppBuildDirectory) 'SplitHaul.exe'
if(!$SkipBuild -and $AppBuildDirectory -ne 'dispatch_cpp\build'){throw 'A custom app output requires -SkipBuild after its checks pass.'}
$compiler=Join-Path $projectRoot 'tools\inno-setup\ISCC.exe'
function Get-PayloadRelativePath([string]$path) {
    $prefix=[IO.Path]::GetFullPath($payload).TrimEnd('\')+'\'
    $full=[IO.Path]::GetFullPath($path)
    if(!$full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw ('File is outside release staging: '+$full)}
    return $full.Substring($prefix.Length)
}
if(!(Test-Path -LiteralPath $compiler)){throw 'Inno Setup ISCC.exe is required in tools\inno-setup.'}
Push-Location $projectRoot
try {
    # Pin the already verified packages. Do not rebuild an older mod layer here.
    # Recreate the array so Windows PowerShell 5 does not serialize a JSON root
    # array's extended properties as { value: [...], Count: 2 } in the manifest.
    $trailerMods=@(foreach($entry in (Get-Content -LiteralPath installer\trailer_mods.json -Raw | ConvertFrom-Json)){
        [ordered]@{game=$entry.game;filename=$entry.filename;version=$entry.version;sha256=$entry.sha256}
    })
    foreach($mod in $trailerMods){
        $source=Join-Path $projectRoot ('trailer_mods\'+$mod.filename)
        if((Get-FileHash -LiteralPath $source).Hash.ToLowerInvariant() -ne $mod.sha256){throw ('Trailer package differs from the approved bundle: '+$mod.filename)}
    }
    if(!$SkipBuild){
        foreach($builder in @('dispatch_cpp\build.cmd','native_job_bridge\build_shift_f9_ats.cmd','native_job_bridge\build_ets2.cmd')){
            & cmd /c $builder
            if($LASTEXITCODE -ne 0){throw ('Build/check failure: '+$builder)}
        }
    }
    & cmd /c installer\build_tools.cmd
    if($LASTEXITCODE -ne 0){throw 'Catalog packaging tool compilation failed.'}
    $versionText=Get-Content -LiteralPath dispatch_cpp\app_version.h -Raw
    $version=[regex]::Match($versionText,'DISPATCH_VERSION_TEXT "([0-9.]+)"').Groups[1].Value
    if(!$version -or (Get-Item -LiteralPath $appBinary).VersionInfo.FileVersion -ne $version){throw 'The app binary and source versions do not match.'}
    New-Item -ItemType Directory -Path $payload -Force | Out-Null
    # Fail closed if an existing staging directory contains anything outside the
    # small, explicit payload. Never recursively copy the development workspace.
    $allowed=@('SplitHaul.exe','installed-layout.json','catalogs\ats.json','catalogs\ets2.json','plugins\ats\GT_Telemetry.dll','plugins\ets2\GT_Telemetry.dll','notices\nlohmann-json-MIT.txt','notices\SCS-SDK-license.txt','mods\GT_Tanker_Combinations_ATS.scs','mods\GT_Tanker_Combinations_ETS2.scs','package-manifest.json')
    foreach($file in Get-ChildItem -LiteralPath $payload -Recurse -File){
        $relative=Get-PayloadRelativePath $file.FullName
        if($relative -notin $allowed){throw ('Unexpected file in release staging: '+$relative)}
    }
    & (Join-Path $PSScriptRoot 'build\CatalogPack.exe') $projectRoot $payload
    if($LASTEXITCODE -ne 0){throw 'Packaged catalog/storage checks failed.'}
    foreach($dir in @('plugins\ats','plugins\ets2','notices','mods')){New-Item -ItemType Directory -Path (Join-Path $payload $dir) -Force | Out-Null}
    Copy-Item -LiteralPath $appBinary -Destination $payload
    Copy-Item -LiteralPath native_job_bridge\build\shift-f9-ats\GT_Telemetry.dll -Destination (Join-Path $payload 'plugins\ats')
    Copy-Item -LiteralPath native_job_bridge\build\ets2\GT_Telemetry.dll -Destination (Join-Path $payload 'plugins\ets2')
    Copy-Item -LiteralPath dispatch_cpp\vendor\LICENSE.json.MIT -Destination (Join-Path $payload 'notices\nlohmann-json-MIT.txt')
    Copy-Item -LiteralPath native_job_bridge\vendor\scs_sdk_1_16\sdk_license.txt -Destination (Join-Path $payload 'notices\SCS-SDK-license.txt')
    foreach($mod in $trailerMods){
        $target=Join-Path $payload ('mods\'+$mod.filename)
        Copy-Item -LiteralPath (Join-Path $projectRoot ('trailer_mods\'+$mod.filename)) -Destination $target
        if((Get-FileHash -LiteralPath $target).Hash.ToLowerInvariant() -ne $mod.sha256){throw ('Staged trailer mod hash mismatch: '+$mod.filename)}
    }
    $hosts=Get-Content -LiteralPath native_job_bridge\host_builds.h -Raw
    $split=[regex]::Match($hosts,'(?s)#ifdef GT_TARGET_ETS2(.*?)#else(.*?)#endif')
    if(!$split.Success){throw 'Cannot read supported game build hashes.'}
    $etsHashes=@([regex]::Matches($split.Groups[1].Value,'\{"([0-9a-f]{64})"')|ForEach-Object{$_.Groups[1].Value})
    $atsHashes=@([regex]::Matches($split.Groups[2].Value,'\{"([0-9a-f]{64})"')|ForEach-Object{$_.Groups[1].Value})
    if(!$etsHashes.Count -or !$atsHashes.Count){throw 'Supported-host hash list is empty.'}
    $atsCondition=($atsHashes|ForEach-Object{"(Lowercase(Hash) = '$_')"}) -join ' or '
    $etsCondition=($etsHashes|ForEach-Object{"(Lowercase(Hash) = '$_')"}) -join ' or '
    @("function SupportedHost(Game, Hash: String): Boolean;",'begin',"  if Game = 'ATS' then Result := $atsCondition","  else if Game = 'ETS2' then Result := $etsCondition",'  else Result := False;','end;') | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'build\supported_hosts.iss') -Encoding utf8
    ('#define AppVersion "'+$version+'"') | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'build\version.iss') -Encoding utf8
    $files=@(Get-ChildItem -LiteralPath $payload -Recurse -File | Where-Object {$_.Name -ne 'package-manifest.json'} | ForEach-Object {
        [ordered]@{path=(Get-PayloadRelativePath $_.FullName).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant();bytes=$_.Length}
    })
    [ordered]@{product='SplitHaul by grumpyman';version=$version;publisher='grumpyman';edition='development test installer';supported_game_sha256=[ordered]@{ats=$atsHashes;ets2=$etsHashes};trailer_mods=$trailerMods;files=$files;contains_personal_profiles_or_history=$false} | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $payload 'package-manifest.json') -Encoding utf8
    # Convert the existing logo to icon format; no new artwork is generated.
    Add-Type -AssemblyName System.Drawing
    $logo=[Drawing.Image]::FromFile((Join-Path $projectRoot 'dispatch_cpp\assets\grumpyman.png'))
    $bitmap=[Drawing.Bitmap]::new(256,256)
    $graphics=[Drawing.Graphics]::FromImage($bitmap)
    $graphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.DrawImage($logo,0,0,256,256)
    $png=[IO.MemoryStream]::new();$bitmap.Save($png,[Drawing.Imaging.ImageFormat]::Png)
    $ico=[IO.BinaryWriter]::new([IO.File]::Create((Join-Path $PSScriptRoot 'build\grumpyman.ico')))
    try{
        $ico.Write([uint16]0);$ico.Write([uint16]1);$ico.Write([uint16]1)
        foreach($unused in 1..4){$ico.Write([byte]0)}
        $ico.Write([uint16]1);$ico.Write([uint16]32);$ico.Write([uint32]$png.Length);$ico.Write([uint32]22);$ico.Write($png.ToArray())
    }finally{$ico.Dispose();$png.Dispose();$graphics.Dispose();$bitmap.Dispose();$logo.Dispose()}
    & $compiler /Qp (Join-Path $PSScriptRoot 'SplitHaul.iss')
    if($LASTEXITCODE -ne 0){throw 'Installer compilation failed.'}
    $setup=Join-Path $projectRoot ('dist\SplitHaul-Setup-'+$version+'-win64.exe')
    $setupHash=(Get-FileHash -LiteralPath $setup).Hash.ToLowerInvariant()
    ($setupHash+'  '+[IO.Path]::GetFileName($setup)) | Set-Content -LiteralPath ($setup+'.sha256') -Encoding ascii
    Copy-Item -LiteralPath (Join-Path $payload 'package-manifest.json') -Destination (Join-Path $projectRoot ('dist\SplitHaul-'+$version+'-manifest.json'))
    Write-Output ('Installer: '+$setup)
    Write-Output ('SHA-256: '+$setupHash)
    # Keep the user's transfer folder on the same verified release set.
    $localReleaseCopy=Join-Path $projectRoot 'tools\project_backup.py'
    if(Test-Path -LiteralPath $localReleaseCopy){
        & python $localReleaseCopy
        if($LASTEXITCODE -ne 0){throw 'Installer built, but copying the release to M:\Grumpyman Trucking failed. Run tools\project_backup.py again when the drive is available.'}
    }
}finally{Pop-Location}
