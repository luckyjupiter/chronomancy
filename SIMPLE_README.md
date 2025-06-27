# Chronomancy Simple - Temporal Scanner

**Ultra-simplified version implementing Scott Wilber's canonical minimal activation energy framework**

## What This Is

This is the production-ready core of Chronomancy - just the essential timer and anomaly scanning functionality as described in `canon.yaml`. 

**One screen. One purpose. Zero friction.**

### Core Features (ONLY)

✅ **Webhook-Based Bot** - Production-grade Telegram integration  
✅ **Timer Setup** - Set your scanning window and frequency  
✅ **Random Pings** - Get notified when it's time to scan  
✅ **Anomaly Reporting** - Quick interface to log what you observe  
✅ **Groups Support** - Works in Telegram groups  
✅ **Telegram Mini App** - Embedded web interface  
✅ **Cloudflare Tunnel** - Secure public access via api.chronomancy.app

### What's REMOVED

❌ Blockchain features  
❌ Betting/gambling systems  
❌ Wallet functionality  
❌ Complex UI with multiple windows  
❌ PCQNG entropy systems  
❌ Mesh networking  
❌ Advanced analytics  

## Quick Start (Production)

1. **Set up your environment** in `.env`:
   ```env
   TELEGRAM_BOT_TOKEN=your_bot_token_from_botfather
   WEBAPP_URL=https://api.chronomancy.app/
   ```

2. **Run the production system**:
   ```bash
   # Automated startup (Windows)
   .\start_production.ps1
   
   # Manual startup
   uvicorn updatedui.server:app --host 0.0.0.0 --port 8000  # Terminal 1
   cloudflared tunnel --config config.yml run               # Terminal 2
   ```

3. **Chat with your bot** on Telegram:
   - `/start` - Shows "🌀 Open Chronomancy App" button
   - Tap button → Opens miniapp at https://api.chronomancy.app/
   - Set your scanning window and frequency
   - Wait for random pings!

## Architecture (Production)

### Webhook Flow
1. **User sends `/start`** → Telegram API
2. **Telegram webhook** → `https://api.chronomancy.app/telegram/BOT_TOKEN`
3. **FastAPI processes** → Bot responds with WebApp button
4. **User taps button** → Opens `https://api.chronomancy.app/`
5. **Timer setup** → WebApp saves to SQLite database  
6. **Random pings** → Bot sends notifications during active window

### Key Differences from Dev Mode
- **Webhooks** instead of polling (faster, more reliable)
- **Public URL** via Cloudflare tunnel (no port forwarding needed)
- **Integrated server** (webhook + web + API in single process)
- **Production-grade** security and monitoring

## How It Works

1. **Setup**: User sets active hours (e.g., 8 AM - 10 PM) and frequency (e.g., 3 pings/day)
2. **Random Pings**: Bot sends random notifications during the active window
3. **Anomaly Scanning**: When pinged, user opens the mini app to log observations
4. **Persistence**: All data stored in SQLite (`bot/chronomancy.db`)

## Files Structure (Production)

```
/bot/main.py                 # Telegram bot with webhook handlers
/bot/chronomancy.db         # SQLite database (auto-created)
/updatedui/server.py        # FastAPI server (webhook + web + API)
/updatedui/*.html           # WebApp frontend files
/start_production.ps1       # Automated startup script (Windows)
/DEPLOYMENT.md             # Complete setup guide
/cloudflared_config_template.yml  # Tunnel configuration
```

## Development vs Production

### Development (Local Testing)
```bash
python -m bot.main                    # Polling mode
python updatedui/start_server.py     # Local web server
# Access: http://localhost:8000/
```

### Production (This Version)
```bash
uvicorn updatedui.server:app --host 0.0.0.0 --port 8000  # Webhook mode
cloudflared tunnel run                                     # Public tunnel
# Access: https://api.chronomancy.app/
```

## Philosophy

Following the canon's guidance:

> "Every interaction should be screenshot-worthy and share-ready."  
> "Setup in <60 seconds; no login required for core loop."  
> "Better to annoy some people with recurring alarms than have 20 million people use it once."

This implements the **absolute minimum viable temporal scanning system** as specified by Scott Wilber's editorial collective, now with production-grade infrastructure.

## API Endpoints (WebApp Server)

- `POST /telegram/{bot_token}` - Telegram webhook endpoint
- `GET /` - Serve miniapp interface
- `GET /health` - Server health check
- `GET /api/user/{user_id}/status` - Get timer status  
- `POST /api/user/{user_id}/timer` - Set up timer
- `POST /api/user/{user_id}/mute` - Mute for X hours
- `POST /api/user/{user_id}/anomaly` - Report observation
- `GET /api/challenge` - Get random scanning prompt

**That's it.** No blockchain, no complex features, just pure temporal scanning with production-grade delivery.

## Success Indicators

When fully operational:

✅ **Local Health**: `curl localhost:8000/health` → 200 OK  
✅ **Public Access**: `curl https://api.chronomancy.app/health` → 200 OK  
✅ **Webhook Active**: Server logs show "📥 Received webhook data"  
✅ **Bot Responsive**: `/start` shows WebApp button  
✅ **Miniapp Opens**: Button opens https://api.chronomancy.app/  
✅ **Timers Work**: Users receive random pings  
✅ **Anomaly Reports**: Users can log observations  

---

*"A memetically-optimized system must make the unknown emotionally irresistible while demanding the smallest possible activation energy from the participant."* - Canon Editorial Note 