# Chronomancy Deployment Guide

## Architecture Overview

**Production Infrastructure:**
- **Telegram Bot**: Webhook-based processing via Telegram API
- **Web Server**: FastAPI application on port 8000
- **Cloudflare Tunnel**: `chronomancy-backend` exposing server to internet
- **Domains**: `chronomancy.app` and `api.chronomancy.app` (both route to same server)
- **Database**: SQLite (`bot/chronomancy.db`)

## Prerequisites

1. **Telegram Bot Token** from @BotFather
2. **Cloudflare Account** with a domain (e.g., `chronomancy.app`)
3. **Windows/Linux Machine** to run the server
4. **Python 3.8+** and pip

## Step 1: Environment Setup

```bash
# Clone repository
git clone <your-repo>
cd chronomancy

# Create virtual environment
python -m venv .venv
.venv\Scripts\activate  # Windows
# source .venv/bin/activate  # Linux/Mac

# Install dependencies
pip install -r requirements.txt
```

Create `.env` file in project root:
```env
TELEGRAM_BOT_TOKEN=your_bot_token_from_botfather
STRIPE_SECRET_KEY=your_stripe_key_if_needed
WEBAPP_URL=https://api.chronomancy.app/
```

## Step 2: Cloudflare Tunnel Setup

### 2.1 Install Cloudflared
```bash
# Windows: Download from https://github.com/cloudflare/cloudflared/releases
# Linux: sudo apt install cloudflared
```

### 2.2 Create Tunnel
```bash
# Login to Cloudflare
cloudflared tunnel login

# Create tunnel
cloudflared tunnel create chronomancy-backend

# Note the tunnel ID (e.g., 8ef25195-783b-4212-83dd-d65cf7996c13)
```

### 2.3 Configure DNS
```bash
# Route domains to tunnel
cloudflared tunnel route dns chronomancy-backend chronomancy.app
cloudflared tunnel route dns chronomancy-backend api.chronomancy.app
```

### 2.4 Create Tunnel Config
Create `%USERPROFILE%\.cloudflared\config.yml` (Windows) or `~/.cloudflared/config.yml` (Linux):

```yaml
tunnel: chronomancy-backend
credentials-file: C:\Users\[username]\.cloudflared\chronomancy-backend.json
ingress:
  - hostname: chronomancy.app
    service: http://localhost:8000
  - hostname: api.chronomancy.app
    service: http://localhost:8000
  - service: http_status:404
```

## Step 3: Server Deployment

### 3.1 Start Application Server
```bash
# Set environment variable for WebApp
$env:WEBAPP_URL = "https://api.chronomancy.app/"  # Windows PowerShell
# export WEBAPP_URL="https://api.chronomancy.app/"  # Linux/Mac

# Start server (bind to all interfaces for tunnel connectivity)
uvicorn updatedui.server:app --host 0.0.0.0 --port 8000
```

### 3.2 Start Cloudflare Tunnel
```bash
# In separate terminal
cloudflared tunnel --config "%USERPROFILE%\.cloudflared\config.yml" run
```

## Step 4: Verification

### 4.1 Test Local Server
```bash
curl http://localhost:8000/health
# Should return: {"status":"healthy","message":"Chronomancy Updated UI Server"}
```

### 4.2 Test Tunnel Connectivity
```bash
curl https://api.chronomancy.app/health
# Should return same response via Cloudflare
```

### 4.3 Test Telegram Bot
1. Send `/start` to your bot on Telegram
2. Should see "🌀 Open Chronomancy App" button
3. Tapping button opens miniapp at `https://api.chronomancy.app/`

## Production Architecture Details

### Webhook Processing Flow
1. **Telegram → Cloudflare → Server**: Messages sent to `https://api.chronomancy.app/telegram/BOT_TOKEN`
2. **Server Processing**: FastAPI receives webhook, passes to bot handler
3. **Bot Response**: Bot processes command, sends response back to Telegram
4. **WebApp Integration**: Bot shows persistent WebApp button when `WEBAPP_URL` is set

### Domain Strategy
- **api.chronomancy.app**: Primary domain (DNS propagates immediately)
- **chronomancy.app**: Secondary domain (DNS may take up to 48 hours)
- Both domains route to same server, ensuring redundancy

### Key Files
```
bot/
├── main.py              # Telegram bot with webhook support
├── chronomancy.db       # SQLite database
└── __init__.py

updatedui/
├── server.py            # FastAPI server with webhook endpoint
└── start_server.py      # Development startup script

.env                     # Environment variables
config.yml              # Cloudflare tunnel configuration
```

## Troubleshooting

### Server Connection Issues
```bash
# Check if server is running
netstat -ano | findstr :8000

# Test local connectivity
curl http://127.0.0.1:8000/health

# Kill stuck processes
taskkill /F /PID [process_id]  # Windows
# kill -9 [process_id]  # Linux
```

### Tunnel Connection Issues
```bash
# Check tunnel status
cloudflared tunnel list

# View tunnel logs for connection errors
cloudflared tunnel --config config.yml run

# Common fixes:
# 1. Ensure server binds to 0.0.0.0:8000 (not localhost)
# 2. Check credentials file exists
# 3. Verify ingress rules in config.yml
```

### DNS Issues
```bash
# Check DNS propagation
nslookup chronomancy.app
nslookup api.chronomancy.app

# If DNS not propagated, use api.chronomancy.app as primary
```

### Bot Issues
```bash
# Check environment variable
echo $env:WEBAPP_URL  # Windows
# echo $WEBAPP_URL  # Linux

# Verify webhook setup in logs:
# "✅ Telegram webhook set → https://api.chronomancy.app/telegram/BOT_TOKEN"
```

## Production Startup Scripts

### Windows (PowerShell)
```powershell
# start_production.ps1
$env:WEBAPP_URL = "https://api.chronomancy.app/"

# Start server in background
Start-Process powershell -ArgumentList "-Command", "uvicorn updatedui.server:app --host 0.0.0.0 --port 8000"

# Start tunnel
Start-Process powershell -ArgumentList "-Command", "cloudflared tunnel --config `"$Env:USERPROFILE\.cloudflared\config.yml`" run"

Write-Host "✅ Chronomancy production services started!"
Write-Host "🌐 Access: https://api.chronomancy.app/"
Write-Host "🤖 Bot: Ready for /start commands"
```

### Linux (systemd service example)
```bash
# /etc/systemd/system/chronomancy.service
[Unit]
Description=Chronomancy Server
After=network.target

[Service]
Type=simple
User=chronomancy
WorkingDirectory=/opt/chronomancy
Environment=WEBAPP_URL=https://api.chronomancy.app/
ExecStart=/opt/chronomancy/.venv/bin/uvicorn updatedui.server:app --host 0.0.0.0 --port 8000
Restart=always

[Install]
WantedBy=multi-user.target
```

## Security Notes

1. **Environment Variables**: Never commit `.env` files to version control
2. **Cloudflare Protection**: Traffic automatically protected by Cloudflare's security features
3. **Bot Token**: Keep Telegram bot token secure, rotate if compromised
4. **Webhook Security**: Server validates webhook requests from Telegram

## Monitoring

### Health Checks
- **Server**: `GET /health` endpoint
- **Bot**: Monitor webhook processing in server logs
- **Tunnel**: Check connection count in cloudflared output (should show 4 connections)

### Key Metrics
- Webhook response time (should be <500ms)
- Bot command processing success rate
- WebApp button click-through rate
- Domain availability (both chronomancy.app and api.chronomancy.app)

---

## Success Indicators

✅ **Server Running**: `curl localhost:8000/health` returns 200  
✅ **Tunnel Active**: 4 tunnel connections to Cloudflare edge  
✅ **Domain Accessible**: `curl https://api.chronomancy.app/health` returns 200  
✅ **Webhook Working**: Bot logs show "📥 Received webhook data"  
✅ **WebApp Enabled**: Bot shows "🌀 Open Chronomancy App" button  
✅ **Integration Working**: Users can tap button → miniapp opens  

When all indicators are green, the system is fully operational! 