# Script pentru rularea serviciului Windows cu privilegii Administrator
# Verifica daca se ruleaza cu drepturi admin, daca nu, relaneaza cu admin

param(
    [switch]$IsAdmin = $false
)

function Test-Administrator {
    $currentUser = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($currentUser)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-Administrator)) {
    $scriptPath = $PSCommandPath
    Write-Host "Relansare cu privilegii Administrator..." -ForegroundColor Yellow
    Start-Process powershell.exe -ArgumentList "-ExecutionPolicy Bypass -File `"$scriptPath`" -IsAdmin" -Verb RunAs
    exit
}

$serviceName = "Tema3HelloWorldService"
$exePath = Join-Path $PSScriptRoot "hello_world_service.exe"

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Serviciu Windows: Hello World Service" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

if (-not (Test-Path $exePath)) {
    Write-Host "EROARE: Executabilul nu exista la $exePath" -ForegroundColor Red
    exit 1
}

# Verifica daca serviciul exista deja
$existing = sc.exe query $serviceName 2>&1
if ($existing -notlike "*FAILED*") {
    Write-Host "Serviciu existent detectat. Se sterge..." -ForegroundColor Yellow
    sc.exe stop $serviceName | Out-Null
    Start-Sleep -Seconds 1
    sc.exe delete $serviceName | Out-Null
    Start-Sleep -Seconds 1
}

# Creeaza serviciul
Write-Host "Se creeaza serviciul..." -ForegroundColor Green
$createResult = sc.exe create $serviceName binPath= "`"$exePath`"" start= demand 2>&1
if ($createResult -like "*SUCCESS*") {
    Write-Host "✓ Serviciu creat cu succes" -ForegroundColor Green
} else {
    Write-Host "✗ Eroare la crearea serviciului: $createResult" -ForegroundColor Red
    exit 2
}

# Porneste serviciul
Write-Host "Se porneste serviciul..." -ForegroundColor Green
$startResult = sc.exe start $serviceName 2>&1
Write-Host $startResult -ForegroundColor Gray
Start-Sleep -Seconds 2

# Verifica statusul
Write-Host "`nVerificare status:" -ForegroundColor Cyan
$status = sc.exe query $serviceName
Write-Host $status -ForegroundColor Gray

# Verificare fișier log
$logFile = Join-Path $env:TEMP "Tema3_HelloWorld.log"
Write-Host "Verificare fișier log: $logFile" -ForegroundColor Cyan
Start-Sleep -Seconds 1
if (Test-Path $logFile) {
    Write-Host "✓ Fișier log găsit:" -ForegroundColor Green
    Get-Content $logFile | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
} else {
    Write-Host "⚠ Fișier log nu exista inca" -ForegroundColor Yellow
}

Write-Host "`nOpțiuni:" -ForegroundColor Cyan
Write-Host "  sc.exe stop $serviceName              # Oprire serviciu" -ForegroundColor Gray
Write-Host "  sc.exe delete $serviceName            # Stergere serviciu" -ForegroundColor Gray
Write-Host "  Get-EventLog -LogName Application -Source `"$serviceName`" | Format-List" -ForegroundColor Gray
Write-Host ""

pause
