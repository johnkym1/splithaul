$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$compiler=Join-Path $projectRoot 'tools\inno-setup\ISCC.exe'
$run=Join-Path $PSScriptRoot ('build\bundle-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $run | Out-Null
$checks=0
function Check([bool]$condition,[string]$message){
    if(!$condition){throw $message}
    $script:checks++
}
function Hash([string]$path){(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()}
function RunSetup([string]$exe,[string[]]$arguments,[bool]$expectSuccess=$true){
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
    if($expectSuccess){Check ($process.ExitCode -eq 0) ('Setup failed: '+$exe+' exit '+$process.ExitCode)}
    else {Check ($process.ExitCode -ne 0) 'Invalid setup unexpectedly succeeded'}
}
foreach($name in @('PluginTests','ModTests')){
    & $compiler /Qp (Join-Path $PSScriptRoot ($name+'.iss')) | Out-File (Join-Path $run ($name+'-compile.log'))
    if($LASTEXITCODE -ne 0){throw ($name+' compile failed')}
    $fixture=Join-Path $run $name
    RunSetup (Join-Path $PSScriptRoot ('build\'+$name+'.exe')) @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/DIR="'+$fixture+'"'),('/LOG="'+$run+'\'+$name+'.log"'))
    $result=Get-Content -LiteralPath (Join-Path $fixture 'test-results.txt') -Raw
    Check ($result.StartsWith('PASS:')) ($name+' checks did not finish')
    Write-Output $result
    RunSetup (Join-Path $fixture 'unins000.exe') @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART')
}
# The same installer code and payload, with only an isolated uninstall AppId.
& $compiler /Qp /DInstallerTestBuild (('/O'+$run)) /FBundleTestSetup (Join-Path $PSScriptRoot 'DispatchCompanion.iss') | Out-File (Join-Path $run 'bundle-compile.log')
if($LASTEXITCODE -ne 0){throw 'Bundle test installer compile failed'}
$setup=Join-Path $run 'BundleTestSetup.exe'
$app=Join-Path $run 'app'
$ats=Join-Path $run 'ATS data'
$ets=Join-Path $run 'ETS2 data'
$payload=Join-Path $PSScriptRoot 'build\payload'
$names=@('GT_Tanker_Combinations_ATS.scs','GT_Tanker_Combinations_ETS2.scs')
$homes=@($ats,$ets)
$before=@{}
for($i=0;$i -lt 2;$i++){
    $gameHome=$homes[$i]
    New-Item -ItemType Directory -Path (Join-Path $gameHome 'mod'),(Join-Path $gameHome 'profiles\fixture\save') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $gameHome 'config.cfg') -Value 'test config only'
    Set-Content -LiteralPath (Join-Path $gameHome 'mod\unrelated.scs') -Value 'unrelated mod'
    Set-Content -LiteralPath (Join-Path $gameHome 'profiles\fixture\save\game.sii') -Value 'save fixture must remain untouched'
    Set-Content -LiteralPath (Join-Path $gameHome ('mod\'+$names[$i])) -Value 'previous GT mod fixture'
    foreach($relative in @('config.cfg','mod\unrelated.scs','profiles\fixture\save\game.sii',('mod\'+$names[$i]))){$before[(Join-Path $gameHome $relative)]=Hash (Join-Path $gameHome $relative)}
}
$common=@('/CURRENTUSER','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/NOICONS',('/DIR="'+$app+'"'),('/ATSHOME="'+$ats+'"'),('/ETSHOME="'+$ets+'"'))
# Validate both selected homes before copying any application or game file.
RunSetup $setup (@($common | Where-Object {$_ -notlike '/ETSHOME=*'})+@('/TASKS=atsmod,etsmod',('/ETSHOME="'+$run+'\missing"'),('/LOG="'+$run+'\invalid-home.log"'))) $false
Check (!(Test-Path -LiteralPath (Join-Path $app 'DispatchPlanner.exe'))) 'Invalid home copied the app before preflight finished'
for($i=0;$i -lt 2;$i++){
    $target=Join-Path $homes[$i] ('mod\'+$names[$i])
    Check ((Hash $target) -eq $before[$target]) 'Invalid home changed an existing mod'
}
RunSetup $setup ($common+@('/TASKS=atsmod,etsmod',('/LOG="'+$run+'\install.log"')))
Check ((Hash (Join-Path $app 'DispatchPlanner.exe')) -eq (Hash (Join-Path $payload 'DispatchPlanner.exe'))) 'Installed application differs from payload'
for($i=0;$i -lt 2;$i++){
    $target=Join-Path $homes[$i] ('mod\'+$names[$i])
    Check ((Hash $target) -eq (Hash (Join-Path $payload ('mods\'+$names[$i])))) 'Installed game mod differs from payload'
    $backup=Join-Path $homes[$i] ('mod-backups\DispatchCompanion\'+$names[$i]+'.'+$before[$target]+'.bak')
    Check ((Hash $backup) -eq $before[$target]) 'Previous mod backup differs'
}
RunSetup $setup ($common+@('/TASKS=atsmod,etsmod',('/LOG="'+$run+'\update.log"')))
for($i=0;$i -lt 2;$i++){
    Check (@(Get-ChildItem -LiteralPath (Join-Path $homes[$i] 'mod-backups\DispatchCompanion') -File).Count -eq 1) 'Repeated install created duplicate backups'
    Check (@(Get-ChildItem -LiteralPath (Join-Path $homes[$i] 'mod') -File).Count -eq 2) 'Extra mod selection entries or staging files appeared'
    foreach($relative in @('config.cfg','mod\unrelated.scs','profiles\fixture\save\game.sii')){
        $path=Join-Path $homes[$i] $relative
        Check ((Hash $path) -eq $before[$path]) ('Installation changed '+$relative)
    }
}
RunSetup (Join-Path $app 'unins000.exe') @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/LOG="'+$run+'\uninstall.log"'))
Check (!(Test-Path -LiteralPath (Join-Path $app 'DispatchPlanner.exe'))) 'Uninstall left the app executable'
for($i=0;$i -lt 2;$i++){
    $target=Join-Path $homes[$i] ('mod\'+$names[$i])
    Check ((Hash $target) -eq (Hash (Join-Path $payload ('mods\'+$names[$i])))) 'App uninstall removed or changed an installed game mod'
}
# Neither plugin nor mod installation is forced on an app-only installation.
$appOnly=Join-Path $run 'app-only'
RunSetup $setup @('/CURRENTUSER','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/NOICONS','/TASKS=',('/DIR="'+$appOnly+'"'),('/ATSHOME="'+$run+'\not-a-game"'),('/ETSHOME="'+$run+'\not-a-game"'),('/LOG="'+$run+'\app-only.log"'))
Check (Test-Path -LiteralPath (Join-Path $appOnly 'mods\GT_Tanker_Combinations_ATS.scs')) 'App-only install did not retain the optional ATS archive'
Check (Test-Path -LiteralPath (Join-Path $appOnly 'mods\GT_Tanker_Combinations_ETS2.scs')) 'App-only install did not retain the optional ETS2 archive'
Check (!(Test-Path -LiteralPath (Join-Path $run 'not-a-game'))) 'Unchecked mod tasks wrote game files'
RunSetup (Join-Path $appOnly 'unins000.exe') @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART')
$manifest=Get-Content -LiteralPath (Join-Path $payload 'package-manifest.json') -Raw | ConvertFrom-Json
Check (($manifest.trailer_mods -is [array]) -and ($manifest.trailer_mods.Count -eq 2)) 'Manifest must list both trailer mods as a JSON array'
foreach($entry in $manifest.files){Check ((Hash (Join-Path $payload $entry.path)) -eq $entry.sha256) ('Manifest hash mismatch: '+$entry.path)}
[ordered]@{version=$manifest.version;checks=$checks;mods=$manifest.trailer_mods;real_game_files_changed=$false;logs=$run;plugin_function_checks=8;mod_function_checks=13;in_game_road_train_verification_pending=$true} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $run 'verification.json')
Write-Output ('PASS: '+$checks+' bundle integration checks. Evidence: '+$run)
