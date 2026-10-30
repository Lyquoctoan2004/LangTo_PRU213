# Live progress of the packaging run (log written by RunUAT BuildCookRun to %TEMP%\ue_stage\package.log).
$log = Join-Path $env:TEMP 'ue_stage\package.log'
$start = Get-Date
while ($true) {
    $lines = Get-Content $log -Tail 4000
    $cook = $lines | Select-String 'Cooked packages (\d+) Packages Remain (\d+) Total (\d+)' | Select-Object -Last 1
    $stage = 'Dang cook'
    if ($lines | Select-String 'Running UnrealPak|Creating pak|IoStore') { $stage = 'Dang dong goi pak' }
    if ($lines | Select-String 'Archiving to') { $stage = 'Dang copy ra Packaged' }
    $done = $lines | Select-String 'BUILD SUCCESSFUL|BUILD FAILED|AutomationTool exiting with ExitCode' | Select-Object -Last 1
    Clear-Host
    Write-Host ('Build LangToLua.exe   ' + (Get-Date -Format 'HH:mm:ss') + '   (theo doi ' + [int]((Get-Date) - $start).TotalMinutes + ' phut)')
    if ($cook) {
        $n = [int]$cook.Matches[0].Groups[1].Value; $t = [int]$cook.Matches[0].Groups[3].Value
        $pct = [int](100 * $n / [Math]::Max(1, $t))
        Write-Host ('Cook: ' + $n + ' / ' + $t + ' goi  [' + ('#' * [int]($pct / 2.5)).PadRight(40, '.') + '] ' + $pct + '%')
    }
    Write-Host ('Giai doan: ' + $stage)
    Write-Host ''
    Write-Host 'Dong log moi nhat:'
    $lines | Select-Object -Last 6 | ForEach-Object { Write-Host ('  ' + ($_.ToString().Substring(0, [Math]::Min(150, $_.ToString().Length)))) }
    if ($done) {
        Write-Host ''
        Write-Host ('KET THUC: ' + $done.Line)
        if (Test-Path 'Packaged\Windows\LangToLua.exe') { Write-Host 'File game: Packaged\Windows\LangToLua.exe' }
        break
    }
    Start-Sleep 3
}
