$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $repoRoot

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  KHOI PHUC FILE DUNG LUONG LON (>100MB) CHO LANGTOLUA" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$part001Files = Get-ChildItem -Path $repoRoot -Filter "*.part001" -Recurse -File -ErrorAction SilentlyContinue

if (-not $part001Files -or $part001Files.Count -eq 0) {
    Write-Host "[OK] Khong tim thay file .part001 nao can ghep (hoac tat ca da duoc khoi phuc!)." -ForegroundColor Green
    exit 0
}

Write-Host "Tim thay $($part001Files.Count) file lon can ghep..." -ForegroundColor Yellow

$buffer = New-Object byte[] (16 * 1024 * 1024) # 16MB buffer
$restoredCount = 0

foreach ($firstPart in $part001Files) {
    $targetFullPath = $firstPart.FullName -replace '\.part001$', ''
    $relTarget = $targetFullPath.Substring($repoRoot.Length + 1).Replace('\', '/')
    $dir = $firstPart.DirectoryName
    $baseName = [System.IO.Path]::GetFileName($targetFullPath)

    # Find all parts for this file
    $allParts = Get-ChildItem -LiteralPath $dir -Filter "$baseName.part*" -File |
        Where-Object { $_.Name -match '^.+\.part\d{3}$' } |
        Sort-Object Name

    $expectedSize = ($allParts | Measure-Object -Property Length -Sum).Sum
    $expectedMB = [math]::Round($expectedSize / 1MB, 2)

    Write-Host "-> Dang ghep: $relTarget ($expectedMB MB tu $($allParts.Count) manh)..." -ForegroundColor White

    $tempTarget = "$targetFullPath.assembling"
    $outStream = [System.IO.File]::Open($tempTarget, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    try {
        foreach ($part in $allParts) {
            $inStream = [System.IO.File]::OpenRead($part.FullName)
            try {
                while (($read = $inStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
                    $outStream.Write($buffer, 0, $read)
                }
            }
            finally {
                $inStream.Close()
            }
        }
    }
    finally {
        $outStream.Close()
    }

    if (Test-Path -LiteralPath $targetFullPath) {
        Remove-Item -LiteralPath $targetFullPath -Force
    }
    Move-Item -LiteralPath $tempTarget -Destination $targetFullPath -Force

    # Hide part files from git status and remove them to save disk space
    foreach ($part in $allParts) {
        $relPart = $part.FullName.Substring($repoRoot.Length + 1).Replace('\', '/')
        git update-index --skip-worktree -- $relPart 2>$null
        Remove-Item -LiteralPath $part.FullName -Force -ErrorAction SilentlyContinue
    }

    $restoredCount++
    Write-Host "   [OK] Da khoi phuc: $relTarget ($expectedMB MB)" -ForegroundColor Green
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "[HOAN TAT] Da khoi phuc thanh cong $restoredCount file!" -ForegroundColor Green
Write-Host "- De CHAY GAME NGAY: Mo Packaged\Windows\LangToLua.exe" -ForegroundColor Yellow
Write-Host "- De MO UNREAL EDITOR: Mo LangToLua.uproject" -ForegroundColor Yellow
Write-Host "==========================================================" -ForegroundColor Cyan
