param([switch]$Debug,[switch]$BuildOnly,[switch]$RunTests,[string]$Compiler)
$ErrorActionPreference = 'Stop'
if ($RunTests -and -not (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'tests/frontend_test.h'))) {
    throw '-RunTests requires the local development test files. Omit it to build the game.'
}
. (Join-Path $PSScriptRoot 'tools/build-version.ps1')
New-Item -ItemType Directory -Path (Join-Path $PSScriptRoot 'build') -Force | Out-Null
$versionResource = Join-Path $PSScriptRoot 'build/game-version.rc'
@"
#include <windows.h>
1 VERSIONINFO
FILEVERSION $resourceVersion
PRODUCTVERSION $resourceVersion
FILEFLAGSMASK 0x3fL
FILEFLAGS 0
FILEOS 0x40004L
FILETYPE 1
BEGIN
 BLOCK "StringFileInfo"
 BEGIN
  BLOCK "040904B0"
  BEGIN
   VALUE "FileDescription", "Tomb Raider Beyond\0"
   VALUE "ProductName", "Tomb Raider Beyond\0"
   VALUE "FileVersion", "$fileVersion\0"
   VALUE "ProductVersion", "$releaseVersion\0"
   VALUE "OriginalFilename", "TombRaiderBeyond.exe\0"
  END
 END
 BLOCK "VarFileInfo"
 BEGIN
  VALUE "Translation", 0x409, 1200
 END
END
"@ | Set-Content -LiteralPath $versionResource -Encoding ascii
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $PSScriptRoot 'work\zig-global-cache'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $PSScriptRoot 'work\zig-local-cache'
if (-not $Compiler) {
    $Compiler = Join-Path $PSScriptRoot 'work\toolchain\zig-x86_64-windows-0.15.2\zig.exe'
    if (-not (Test-Path -LiteralPath $Compiler)) {
        $zigCommand = Get-Command zig -ErrorAction SilentlyContinue
        if (-not $zigCommand) { throw 'Install Zig 0.15.2 and add it to PATH, or pass -Compiler <path to zig.exe>.' }
        $Compiler = $zigCommand.Source
    }
}
$testDefines = @()
if ($RunTests) { $testDefines += '-DTOMB_BUILD_TESTS' }
$optimization = if ($Debug) { '-O0' } else { '-O2' }
$outputName = if ($Debug) { 'tomb_preview_debug.exe' } else { 'tomb_preview.exe' }
$sourceNames = @('enemies.c','creature_move.c','navigation.c','creature.c','pistols.c','hazards.c','dos_camera.c','object_contact.c','objects.c','slide.c','sound_win.c','sound.c','preview.c','room_visibility.c','gym.c','follow_camera.c','air.c','water.c','ledge.c','playtest.c','movement.c','animation.c','ground.c','collision.c','terrain.c','static.c','preview_pose.c','preview_camera.c','level.c','visual.c','lara_start.c','geometry.c','item_height.c')
$sourceNames += 'vr.c','shooter.c','atlantean.c','cinematic.c','movie.c','folly.c','peru.c','render_interpolation.c','lighting.c','ui_dialog.c','save.c','city.c','combat.c','combat_fixture.c','interface.c','ring.c','text.c','inventory.c','pickup.c','pickup_runtime.c','progression.c'
$sources = $sourceNames | ForEach-Object { Join-Path $PSScriptRoot "src\$_" }
& $compiler cc -target x86_64-windows-gnu -std=c11 $optimization -g -Wall -Wextra -Werror -pedantic @sources @testDefines $versionResource -lopengl32 -lgdi32 -luser32 -lwinmm -lavrt '-Wl,--subsystem,windows' -o (Join-Path $PSScriptRoot "build\$outputName")
if ($LASTEXITCODE -ne 0) { throw "Preview compilation failed: $LASTEXITCODE" }
Write-Output "Built $outputName"
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'third_party/openxr/bin/openxr_loader.dll') -Destination (Join-Path $PSScriptRoot 'build/openxr_loader.dll') -Force
if($BuildOnly -or -not $RunTests){return}
$equipmentProcess = Start-Process -FilePath (Join-Path $PSScriptRoot "build/$outputName") -ArgumentList '--consistent-weapons-test' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
if ($equipmentProcess.ExitCode -ne 0) { throw 'Consistent Weapons transition or cinematic regression failed' }
Write-Output 'PASS: Consistent Weapons transitions, confiscation and cinematic equipment'

$reviewProcess = Start-Process -FilePath (Join-Path $PSScriptRoot "build/$outputName") -ArgumentList '--original-review-test' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
if ($reviewProcess.ExitCode -ne 0) { throw 'Original-mode hidden pickups, speech or cabin visibility regression failed' }
Write-Output 'PASS: Original-mode hidden pickups, speech and cabin visibility'
& (Join-Path $PSScriptRoot 'tools/test-vr.ps1') -Debug:$Debug
$vrProcess = Start-Process -FilePath (Join-Path $PSScriptRoot "build/$outputName") -ArgumentList '--vr-render-test' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
if ($vrProcess.ExitCode -ne 0) { throw 'VR framebuffer menu, level loading or stereo rendering regression failed' }
Write-Output 'PASS: VR offscreen menu, hand alignment and Caves/Vilcabamba stereo rendering'
& (Join-Path $PSScriptRoot 'tools/test-midas.ps1') -Debug:$Debug
& (Join-Path $PSScriptRoot 'tools/test-custom-gameplay.ps1') -Debug:$Debug
foreach($renderLevel in @(0,1,13)){
    $renderProcess=Start-Process -FilePath (Join-Path $PSScriptRoot "build/$outputName") -ArgumentList "--level $renderLevel --render-options-test" -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
    if($renderProcess.ExitCode -ne 0){throw "Render options validation failed: level $renderLevel"}
    Write-Output "PASS: black backdrop and maximum distance, level $renderLevel"
}

$midasProcess = Start-Process -FilePath (Join-Path $PSScriptRoot "build/$outputName") -ArgumentList '--level 7 --midas-test' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
if ($midasProcess.ExitCode -ne 0) { throw 'Midas gold animation/render validation failed' }
Write-Output 'PASS: Midas cinematic camera, progressive gold meshes and sparkles'
foreach ($lightingLevel in @(0,1,2,3)) {
    $lightingProcess = Start-Process -FilePath (Join-Path $PSScriptRoot "build\$outputName") -ArgumentList "--level $lightingLevel --lighting-test" -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
    if ($lightingProcess.ExitCode -ne 0) { throw "GPU lighting validation failed: level $lightingLevel" }
    Write-Output "PASS: GPU palette and transparency, level $lightingLevel"
}



$cameraTest = if ($Debug) { 'preview_camera_test_debug.exe' } else { 'preview_camera_test.exe' }
& $compiler cc -target x86_64-windows-gnu -std=c11 $optimization -g -Wall -Wextra -Werror -pedantic -I (Join-Path $PSScriptRoot 'src') (Join-Path $PSScriptRoot 'src\preview_camera.c') (Join-Path $PSScriptRoot 'tests\preview_camera_test.c') -o (Join-Path $PSScriptRoot "build\$cameraTest")
if ($LASTEXITCODE -ne 0) { throw "Camera test compilation failed: $LASTEXITCODE" }
& (Join-Path $PSScriptRoot "build\$cameraTest")
if ($LASTEXITCODE -ne 0) { throw "Camera orientation test failed: $LASTEXITCODE" }

$playtestName = if ($Debug) { 'playtest_test_debug.exe' } else { 'playtest_test.exe' }
$playtestSources = $sources | Where-Object { [IO.Path]::GetFileName($_) -notin @('preview.c','sound_win.c') }
& $compiler cc -target x86_64-windows-gnu -std=c11 $optimization -g -Wall -Wextra -Werror -pedantic -I (Join-Path $PSScriptRoot 'src') @playtestSources (Join-Path $PSScriptRoot 'tests\playtest_test.c') -o (Join-Path $PSScriptRoot "build\$playtestName")
if ($LASTEXITCODE -ne 0) { throw "Ground integration test compilation failed: $LASTEXITCODE" }
& (Join-Path $PSScriptRoot "build\$playtestName") (Join-Path $PSScriptRoot 'work\reference-assets\DATA\GYM.PHD') (Join-Path $PSScriptRoot 'work\reference-assets\sine.bin')
if ($LASTEXITCODE -ne 0) { throw "Ground integration test failed: $LASTEXITCODE" }

$poseTest = if ($Debug) { 'preview_pose_test_debug.exe' } else { 'preview_pose_test.exe' }
& $compiler cc -target x86_64-windows-gnu -std=c11 $optimization -g -Wall -Wextra -Werror -pedantic -I (Join-Path $PSScriptRoot 'src') @playtestSources (Join-Path $PSScriptRoot 'tests\preview_pose_test.c') -o (Join-Path $PSScriptRoot "build\$poseTest")
if ($LASTEXITCODE -ne 0) { throw "Pose test compilation failed: $LASTEXITCODE" }
& (Join-Path $PSScriptRoot "build\$poseTest") (Join-Path $PSScriptRoot 'work\reference-assets\DATA\GYM.PHD') (Join-Path $PSScriptRoot 'work\reference-assets\DATA\LEVEL1.PHD')
if ($LASTEXITCODE -ne 0) { throw "Pose interpolation validation failed: $LASTEXITCODE" }

$followTest = if ($Debug) { 'follow_camera_test_debug.exe' } else { 'follow_camera_test.exe' }
& $compiler cc -target x86_64-windows-gnu -std=c11 $optimization -g -Wall -Wextra -Werror -pedantic -I (Join-Path $PSScriptRoot 'src') @playtestSources (Join-Path $PSScriptRoot 'tests\follow_camera_test.c') -o (Join-Path $PSScriptRoot "build\$followTest")
if ($LASTEXITCODE -ne 0) { throw "Follow camera compilation failed: $LASTEXITCODE" }
& (Join-Path $PSScriptRoot "build\$followTest") (Join-Path $PSScriptRoot 'work\reference-assets\DATA\GYM.PHD') (Join-Path $PSScriptRoot 'work\reference-assets\sine.bin')
if ($LASTEXITCODE -ne 0) { throw "Follow camera validation failed: $LASTEXITCODE" }

$visibilityTest = if ($Debug) { 'room_visibility_test_debug.exe' } else { 'room_visibility_test.exe' }
& $compiler cc -target x86_64-windows-gnu -std=c11 $optimization -Wall -Wextra -Werror -pedantic -I (Join-Path $PSScriptRoot 'src') (Join-Path $PSScriptRoot 'src\room_visibility.c') (Join-Path $PSScriptRoot 'src\preview_camera.c') (Join-Path $PSScriptRoot 'tests\room_visibility_test.c') -o (Join-Path $PSScriptRoot "build\$visibilityTest")
if ($LASTEXITCODE -ne 0) { throw 'Visibility test compilation failed' }
& (Join-Path $PSScriptRoot "build\$visibilityTest")
if ($LASTEXITCODE -ne 0) { throw 'Visibility test failed' }

$soundTest = if ($Debug) { 'sound_test_debug.exe' } else { 'sound_test.exe' }
$soundSuffix = if ($Debug) { '_debug' } else { '' }
& $compiler cc -target x86_64-windows-gnu -std=c11 $optimization -g -Wall -Wextra -Werror -pedantic -I (Join-Path $PSScriptRoot 'src') @playtestSources (Join-Path $PSScriptRoot 'src\sound_win.c') (Join-Path $PSScriptRoot 'tests\sound_test.c') -lwinmm -lavrt -o (Join-Path $PSScriptRoot "build\$soundTest")
if ($LASTEXITCODE -ne 0) { throw 'Sound test compilation failed' }
& (Join-Path $PSScriptRoot "build\$soundTest") (Join-Path $PSScriptRoot 'work\reference-assets\DATA\GYM.PHD') (Join-Path $PSScriptRoot 'work\reference-assets\sine.bin') (Join-Path $PSScriptRoot "build\gym-sounds$soundSuffix.wav") (Join-Path $PSScriptRoot "build\gym-sounds$soundSuffix.txt")
if ($LASTEXITCODE -ne 0) { throw 'Sound integration test failed' }

$cavesTest = if ($Debug) { 'caves_test_debug.exe' } else { 'caves_test.exe' }
& $compiler cc -target x86_64-windows-gnu -std=c11 $optimization -g -Wall -Wextra -Werror -pedantic -I (Join-Path $PSScriptRoot 'src') @playtestSources (Join-Path $PSScriptRoot 'tests\caves_test.c') -o (Join-Path $PSScriptRoot "build\$cavesTest")
if ($LASTEXITCODE -ne 0) { throw 'Caves test compilation failed' }
& (Join-Path $PSScriptRoot "build\$cavesTest") (Join-Path $PSScriptRoot 'work\reference-assets\DATA\LEVEL1.PHD') (Join-Path $PSScriptRoot 'work\reference-assets\sine.bin')
if ($LASTEXITCODE -ne 0) { throw 'Caves movement validation failed' }

# Exercise Windows key-message routing as well as fixture/camera gameplay.
foreach ($inputArgs in @('--window-input-test','--window-input-test --caves')) {
    $inputProcess = Start-Process -FilePath (Join-Path $PSScriptRoot "build\$outputName") -ArgumentList $inputArgs -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
    if ($inputProcess.ExitCode -ne 0) { throw "Window input integration failed: $inputArgs" }
}
Write-Output 'PASS: gym/Caves Windows input and switch camera integration'

$shotgunTest = if ($Debug) { 'shotgun_test_debug.exe' } else { 'shotgun_test.exe' }
& $compiler cc -target x86_64-windows-gnu -std=c11 $optimization -g -Wall -Wextra -Werror -pedantic -I (Join-Path $PSScriptRoot 'src') @playtestSources (Join-Path $PSScriptRoot 'tests\shotgun_test.c') -o (Join-Path $PSScriptRoot "build\$shotgunTest")
if ($LASTEXITCODE -ne 0) { throw 'Shotgun test compilation failed' }
& (Join-Path $PSScriptRoot "build\$shotgunTest") (Join-Path $PSScriptRoot 'work\reference-assets\DATA\LEVEL3A.PHD') (Join-Path $PSScriptRoot 'work\reference-assets\sine.bin')
if ($LASTEXITCODE -ne 0) { throw 'Shotgun integration failed' }

$combatTest = if ($Debug) { 'combat_test_debug.exe' } else { 'combat_test.exe' }
& $compiler cc -target x86_64-windows-gnu -std=c11 $optimization -g -Wall -Wextra -Werror -pedantic -I (Join-Path $PSScriptRoot 'src') @playtestSources (Join-Path $PSScriptRoot 'tests\combat_test.c') -o (Join-Path $PSScriptRoot "build\$combatTest")
if ($LASTEXITCODE -ne 0) { throw 'Combat test compilation failed' }
& (Join-Path $PSScriptRoot "build\$combatTest") (Join-Path $PSScriptRoot 'work\reference-assets\DATA\LEVEL1.PHD') (Join-Path $PSScriptRoot 'work\reference-assets\sine.bin')
if ($LASTEXITCODE -ne 0) { throw 'Combat integration failed' }

$inventoryTest = if ($Debug) { 'inventory_test_debug.exe' } else { 'inventory_test.exe' }
& $compiler cc -target x86_64-windows-gnu -std=c11 $optimization -g -Wall -Wextra -Werror -pedantic -I (Join-Path $PSScriptRoot 'src') @playtestSources (Join-Path $PSScriptRoot 'tests\inventory_test.c') -o (Join-Path $PSScriptRoot "build\$inventoryTest")
if ($LASTEXITCODE -ne 0) { throw 'Inventory test compilation failed' }
& (Join-Path $PSScriptRoot "build\$inventoryTest") (Join-Path $PSScriptRoot 'work\reference-assets\DATA\LEVEL1.PHD') (Join-Path $PSScriptRoot 'work\reference-assets\sine.bin') (Join-Path $PSScriptRoot 'work\reference-assets\DATA\LEVEL2.PHD')
if ($LASTEXITCODE -ne 0) { throw 'Inventory/progression integration failed' }

$cityTest = if ($Debug) { 'city_test_debug.exe' } else { 'city_test.exe' }
& $compiler cc -target x86_64-windows-gnu -std=c11 $optimization -g -Wall -Wextra -Werror -pedantic -I (Join-Path $PSScriptRoot 'src') @playtestSources (Join-Path $PSScriptRoot 'tests\city_test.c') -o (Join-Path $PSScriptRoot "build\$cityTest")
if ($LASTEXITCODE -ne 0) { throw 'City test compilation failed' }
& (Join-Path $PSScriptRoot "build\$cityTest") (Join-Path $PSScriptRoot 'work\reference-assets\DATA\LEVEL2.PHD') (Join-Path $PSScriptRoot 'work\reference-assets\sine.bin')
if ($LASTEXITCODE -ne 0) { throw 'City integration failed' }

$questProcess = Start-Process -FilePath (Join-Path $PSScriptRoot "build\$outputName") -ArgumentList '--city --quest-ui-test' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
if ($questProcess.ExitCode -ne 0) { throw "Quest UI integration failed: $($questProcess.ExitCode)" }
Write-Output 'PASS: City quest menu, S/Enter selection, door completion and main/items ring navigation'

$cityTransition = Start-Process -FilePath (Join-Path $PSScriptRoot "build\$outputName") -ArgumentList '--city --transition-test --capture build/city-transition-check.ppm' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
if ($cityTransition.ExitCode -ne 0) { throw "City to Lost Valley transition failed: $($cityTransition.ExitCode)" }
Write-Output 'PASS: City actual exit, Enter continuation, Lost Valley load, inventory carry and level-state reset'

$valleyTransition = Start-Process -FilePath (Join-Path $PSScriptRoot "build\$outputName") -ArgumentList '--level 3 --transition-test --capture build/valley-transition-check.ppm' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
if ($valleyTransition.ExitCode -ne 0) { throw "Valley to Qualopec transition failed: $($valleyTransition.ExitCode)" }
Write-Output 'PASS: Valley actual exit, Enter continuation, Qualopec load and inventory carry'

$qualopecTransition = Start-Process -FilePath (Join-Path $PSScriptRoot "build\$outputName") -ArgumentList '--level 4 --transition-test --capture build/qualopec-transition-check.ppm' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
if ($qualopecTransition.ExitCode -ne 0) { throw 'Qualopec to Folly transition failed' }
Write-Output 'PASS: Qualopec completion input, Folly load and inventory carry'

$ambientTest = if ($Debug) { 'ambient_sound_test_debug.exe' } else { 'ambient_sound_test.exe' }
& $compiler cc -target x86_64-windows-gnu -std=c11 $optimization -g -Wall -Wextra -Werror -pedantic -I (Join-Path $PSScriptRoot 'src') @playtestSources (Join-Path $PSScriptRoot 'tests/ambient_sound_test.c') -o (Join-Path $PSScriptRoot "build/$ambientTest")
if ($LASTEXITCODE -ne 0) { throw 'Ambient test compilation failed' }
$ambientLevels = Get-ChildItem (Join-Path $PSScriptRoot 'work/reference-assets/DATA') -Filter '*.PHD' | ForEach-Object FullName
& (Join-Path $PSScriptRoot "build/$ambientTest") @ambientLevels
if ($LASTEXITCODE -ne 0) { throw 'Ambient sound checks failed' }

$saveTest = if ($Debug) { 'save_test_debug.exe' } else { 'save_test.exe' }
& $compiler cc -target x86_64-windows-gnu -std=c11 $optimization -g -Wall -Wextra -Werror -pedantic -I (Join-Path $PSScriptRoot 'src') @playtestSources (Join-Path $PSScriptRoot 'tests\save_test.c') -o (Join-Path $PSScriptRoot "build\$saveTest")
if ($LASTEXITCODE -ne 0) { throw 'Save test compilation failed' }
& (Join-Path $PSScriptRoot "build\$saveTest") (Join-Path $PSScriptRoot 'work\reference-assets\DATA\LEVEL1.PHD') (Join-Path $PSScriptRoot 'work\reference-assets\DATA\LEVEL2.PHD') (Join-Path $PSScriptRoot 'work\reference-assets\sine.bin')
if ($LASTEXITCODE -ne 0) { throw 'Save state test failed' }
$frontProcess = Start-Process -FilePath (Join-Path $PSScriptRoot "build\$outputName") -ArgumentList '--frontend-test' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
if ($frontProcess.ExitCode -ne 0) { throw 'Front-end menu integration failed; see build/frontend-test.txt' }
Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build\frontend-test.txt')

$valleyTest = if ($Debug) { 'valley_enemies_test_debug.exe' } else { 'valley_enemies_test.exe' }
& $compiler cc -target x86_64-windows-gnu -std=c11 $optimization -g -Wall -Wextra -Werror -pedantic -I (Join-Path $PSScriptRoot 'src') @playtestSources (Join-Path $PSScriptRoot 'tests\valley_enemies_test.c') -o (Join-Path $PSScriptRoot "build\$valleyTest")
if ($LASTEXITCODE -ne 0) { throw 'Valley enemies compilation failed' }
& (Join-Path $PSScriptRoot "build\$valleyTest") (Join-Path $PSScriptRoot 'work\reference-assets\DATA\LEVEL3A.PHD') (Join-Path $PSScriptRoot 'work\reference-assets\sine.bin')
if ($LASTEXITCODE -ne 0) { throw 'Valley enemies integration failed' }

$interpolationTest = if ($Debug) { 'render_interpolation_test_debug.exe' } else { 'render_interpolation_test.exe' }
& $compiler cc -target x86_64-windows-gnu -std=c11 $optimization -g -Wall -Wextra -Werror -pedantic -I (Join-Path $PSScriptRoot 'src') (Join-Path $PSScriptRoot 'src/render_interpolation.c') (Join-Path $PSScriptRoot 'tests/render_interpolation_test.c') -o (Join-Path $PSScriptRoot "build/$interpolationTest")
if ($LASTEXITCODE -ne 0) { throw 'Interpolation test compilation failed' }
& (Join-Path $PSScriptRoot "build/$interpolationTest")
if ($LASTEXITCODE -ne 0) { throw 'Interpolation test failed' }
foreach ($interpolationLevel in @(0,1,2,3,4,12,13)) {
    $interpolationProcess = Start-Process -FilePath (Join-Path $PSScriptRoot "build/$outputName") -ArgumentList "--level $interpolationLevel --interpolation-test" -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
    if ($interpolationProcess.ExitCode -ne 0) { throw "Live interpolation test failed: level $interpolationLevel" }
    Write-Output "PASS: live render interpolation, level $interpolationLevel"
}

& (Join-Path $PSScriptRoot 'tools/test-peru.ps1') -Debug:$Debug
foreach ($peruLevel in @(3,4)) {
    $peruProcess=Start-Process -FilePath (Join-Path $PSScriptRoot "build/$outputName") -ArgumentList "--level $peruLevel --peru-test" -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
    if($peruProcess.ExitCode -ne 0){throw "Peru render validation failed: $peruLevel"}
    Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build/peru-render.txt')
}

$waterProcess = Start-Process -FilePath (Join-Path $PSScriptRoot "build\$outputName") -ArgumentList '--water-render-test' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
if ($waterProcess.ExitCode -ne 0) { throw 'Water presentation pool integration failed' }
Write-Output 'PASS: pool entry, underwater animation, splash/bubble sprites, pause and reset'

if(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'work/reference-assets/FMV/LIFT.bmv')) {
    $mediaProcess=Start-Process -FilePath (Join-Path $PSScriptRoot "build/$outputName") -ArgumentList '--level 1 --media-test' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
    if($mediaProcess.ExitCode -ne 0){throw "Cinematic/FMV integration failed: $($mediaProcess.ExitCode)"}
    Write-Output 'PASS: 11 movies, CUT1-CUT4 complete animation/render, credits and return to gameplay renderer'
}

$movieTest=if($Debug){'movie_test_debug.exe'}else{'movie_test.exe'}
& $compiler cc -target x86_64-windows-gnu -std=c11 $optimization -Wall -Wextra -Werror -pedantic -I (Join-Path $PSScriptRoot 'src') (Join-Path $PSScriptRoot 'src/movie.c') (Join-Path $PSScriptRoot 'tests/movie_test.c') -o (Join-Path $PSScriptRoot "build/$movieTest")
if($LASTEXITCODE -ne 0){throw 'Movie parser test compilation failed'}
& (Join-Path $PSScriptRoot "build/$movieTest")
if($LASTEXITCODE -ne 0){throw 'Movie parser test failed'}

$blockProcess=Start-Process -FilePath (Join-Path $PSScriptRoot "build/$outputName") -ArgumentList '--level 4 --block-render-test' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
if($blockProcess.ExitCode -ne 0){throw 'Qualopec block render validation failed'}
Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build/block-render.txt')

& (Join-Path $PSScriptRoot 'tools/test-folly.ps1') -Debug:$Debug
$follyProcess=Start-Process -FilePath (Join-Path $PSScriptRoot "build/$outputName") -ArgumentList '--level 5 --folly-test' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
if($follyProcess.ExitCode -ne 0){throw 'Folly render validation failed'}
Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build/folly-render.txt')

& (Join-Path $PSScriptRoot 'tools/test-magnums.ps1') -Debug:$Debug

& (Join-Path $PSScriptRoot 'tools/test-remaining.ps1') -Debug:$Debug
foreach($remainingLevel in 8..15){
    $remainingProcess=Start-Process -FilePath (Join-Path $PSScriptRoot "build/$outputName") -ArgumentList "--level $remainingLevel --remaining-render-test" -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
    if($remainingProcess.ExitCode -ne 0){throw "Remaining-level render validation failed: $remainingLevel"}
}
Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build/remaining-render.txt') -Tail 8

& (Join-Path $PSScriptRoot 'tools/test-shooter.ps1') -Debug:$Debug
