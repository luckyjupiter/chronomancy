# Chronomancy Simple - Temporal Scanner

**Ultra-simplified version implementing Scott Wilber's canonical minimal activation energy framework**

## What This Is

This is the stripped-down core of Chronomancy - just the essential timer and anomaly scanning functionality as described in `canon.yaml`. 

**One screen. One purpose. Zero friction.**

### Core Features (ONLY)

✅ **Timer Setup** - Set your scanning window and frequency  
✅ **Random Pings** - Get notified when it's time to scan  
✅ **Anomaly Reporting** - Quick interface to log what you observe  
✅ **Groups Support** - Works in Telegram groups  
✅ **Telegram Mini App** - Embedded web interface  

### What's REMOVED

❌ Blockchain features  
❌ Betting/gambling systems  
❌ Wallet functionality  
❌ Complex UI with multiple windows  
❌ PCQNG entropy systems  
❌ Mesh networking  
❌ Advanced analytics  

## Quick Start

1. **Set up your Telegram bot token** in `.env`:
   ```
   TELEGRAM_BOT_TOKEN=your_bot_token_here
   ```

2. **Run the simple system**:
   ```bash
   simple_start.bat  # Windows
   ```
   
   Or manually:
   ```bash
   # Terminal 1: Start the bot
   python -m bot.main
   
   # Terminal 2: Start the mini app
   cd miniapp
   python simple_start.py
   ```

3. **Chat with your bot** on Telegram:
   - `/start` - Initial setup
   - Tap "🌀 Open Chronomancy App" button
   - Set your scanning window
   - Wait for pings!

## How It Works

1. **Setup**: User sets active hours (e.g., 8 AM - 10 PM) and frequency (e.g., 3 pings/day)
2. **Random Pings**: Bot sends random notifications during the active window
3. **Anomaly Scanning**: When pinged, user opens the mini app to log observations
4. **That's It**: Simple, focused, effective

## Files Structure

```
/bot/main.py              # Telegram bot (existing, works as-is)
/miniapp/simple_server.py # Ultra-simple FastAPI server (NEW)
/miniapp/simple_index.html # One-screen mini app UI (NEW)
/miniapp/simple_start.py  # Startup script (NEW)
/simple_start.bat         # Windows launcher (NEW)
```

## Philosophy

Following the canon's guidance:

> "Every interaction should be screenshot-worthy and share-ready."  
> "Setup in <60 seconds; no login required for core loop."  
> "Better to annoy some people with recurring alarms than have 20 million people use it once."

This implements the **absolute minimum viable temporal scanning system** as specified by Scott Wilber's editorial collective.

## API Endpoints (Simple Server)

- `GET /` - Serve mini app
- `GET /api/user/{user_id}/status` - Get timer status  
- `POST /api/user/{user_id}/timer` - Set up timer
- `POST /api/user/{user_id}/mute` - Mute for X hours
- `POST /api/user/{user_id}/anomaly` - Report observation
- `GET /api/challenge` - Get random scanning prompt

**That's it.** No blockchain, no complex features, just pure temporal scanning.

---

*"A memetically-optimized system must make the unknown emotionally irresistible while demanding the smallest possible activation energy from the participant."* - Canon Editorial Note 