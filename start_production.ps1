# Chronomancy Production Startup Script
# Starts webhook server + Cloudflare tunnel for production deployment

Write-Host "🌀 Starting Chronomancy Production Environment..." -ForegroundColor Cyan

# Check if .env exists
if (-not (Test-Path ".env")) {
    Write-Host "❌ Error: .env file not found!" -ForegroundColor Red
    Write-Host "Create .env with:" -ForegroundColor Yellow
    Write-Host "TELEGRAM_BOT_TOKEN=your_bot_token_here" -ForegroundColor Yellow
    Write-Host "WEBAPP_URL=https://api.chronomancy.app/" -ForegroundColor Yellow
    exit 1
}

# Set environment variables
$env:WEBAPP_URL = "https://api.chronomancy.app/"
Write-Host "✅ Environment: WEBAPP_URL = $($env:WEBAPP_URL)" -ForegroundColor Green

# Check if virtual environment exists
if (-not (Test-Path ".venv\Scripts\activate.ps1")) {
    Write-Host "⚠️  Virtual environment not found. Creating..." -ForegroundColor Yellow
    python -m venv .venv
    .\.venv\Scripts\activate
    pip install -r requirements.txt
} else {
    Write-Host "✅ Activating virtual environment..." -ForegroundColor Green
    .\.venv\Scripts\activate
}

# Check if cloudflared config exists
$configPath = "$Env:USERPROFILE\.cloudflared\config.yml"
if (-not (Test-Path $configPath)) {
    Write-Host "❌ Error: Cloudflare tunnel config not found at $configPath" -ForegroundColor Red
    Write-Host "Run setup first: see DEPLOYMENT.md" -ForegroundColor Yellow
    exit 1
}

Write-Host "🚀 Starting production services..." -ForegroundColor Cyan

# Start FastAPI server in background
Write-Host "📡 Starting webhook server (port 8000)..." -ForegroundColor Blue
$serverJob = Start-Job -ScriptBlock {
    param($workingDir, $webappUrl)
    Set-Location $workingDir
    $env:WEBAPP_URL = $webappUrl
    & .\.venv\Scripts\uvicorn.exe updatedui.server:app --host 0.0.0.0 --port 8000
} -ArgumentList (Get-Location), $env:WEBAPP_URL

# Wait a moment for server to start
Start-Sleep 3

# Test local server
try {
    $response = Invoke-RestMethod -Uri "http://localhost:8000/health" -TimeoutSec 5
    Write-Host "✅ Server health check: $($response.status)" -ForegroundColor Green
} catch {
    Write-Host "⚠️  Server may still be starting..." -ForegroundColor Yellow
}

# Start Cloudflare tunnel
Write-Host "🌐 Starting Cloudflare tunnel..." -ForegroundColor Blue
$tunnelJob = Start-Job -ScriptBlock {
    param($configPath)
    & cloudflared tunnel --config $configPath run
} -ArgumentList $configPath

# Wait for tunnel to establish
Start-Sleep 5

Write-Host ""
Write-Host "🎉 Chronomancy Production Started!" -ForegroundColor Green
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "🌐 Public URL: https://api.chronomancy.app/" -ForegroundColor White
Write-Host "🤖 Telegram Bot: Ready for /start commands" -ForegroundColor White
Write-Host "📊 Health Check: https://api.chronomancy.app/health" -ForegroundColor White
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""
Write-Host "Background Jobs Running:" -ForegroundColor Yellow
Write-Host "  Server Job ID: $($serverJob.Id)" -ForegroundColor Gray
Write-Host "  Tunnel Job ID: $($tunnelJob.Id)" -ForegroundColor Gray
Write-Host ""
Write-Host "To stop services:" -ForegroundColor Yellow
Write-Host "  Stop-Job $($serverJob.Id), $($tunnelJob.Id)" -ForegroundColor Gray
Write-Host "  Remove-Job $($serverJob.Id), $($tunnelJob.Id)" -ForegroundColor Gray
Write-Host ""

# Monitor services
Write-Host "Monitoring services (Ctrl+C to exit)..." -ForegroundColor Cyan
Write-Host "Press any key to check service status, 'q' to quit monitoring" -ForegroundColor Gray

try {
    while ($true) {
        $key = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
        
        if ($key.Character -eq 'q') {
            break
        }
        
        Write-Host "`n🔍 Service Status Check:" -ForegroundColor Cyan
        
        # Check server job
        $serverStatus = Get-Job -Id $serverJob.Id
        Write-Host "  📡 Server: $($serverStatus.State)" -ForegroundColor $(if ($serverStatus.State -eq "Running") { "Green" } else { "Red" })
        
        # Check tunnel job  
        $tunnelStatus = Get-Job -Id $tunnelJob.Id
        Write-Host "  🌐 Tunnel: $($tunnelStatus.State)" -ForegroundColor $(if ($tunnelStatus.State -eq "Running") { "Green" } else { "Red" })
        
        # Check external connectivity
        try {
            $healthCheck = Invoke-RestMethod -Uri "https://api.chronomancy.app/health" -TimeoutSec 10
            Write-Host "  ✅ External Access: OK ($($healthCheck.status))" -ForegroundColor Green
        } catch {
            Write-Host "  ❌ External Access: Failed" -ForegroundColor Red
        }
        
        Write-Host ""
    }
} catch {
    # Ctrl+C pressed
}

Write-Host "`n👋 Monitoring stopped. Services are still running in background." -ForegroundColor Yellow
Write-Host "Use the Stop-Job commands above to shut down when ready." -ForegroundColor Gray 