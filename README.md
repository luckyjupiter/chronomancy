# Chronomancy – Temporal Scanner Bot + WebApp

A **single-screen** Telegram WebApp + bot that pings you a few random times per day and asks you to scan your surroundings for anomalies.

**Production Architecture**: Webhook-based Telegram bot with integrated FastAPI server and Cloudflare tunnel.

---

## ✨ Features

• **Telegram Bot** (`bot/`) – webhook-based processing, user timer management, anomaly recording  
• **Telegram WebApp** (`updatedui/`) – embedded miniapp for settings and anomaly reporting  
• **FastAPI Backend** – unified server handling both webhook and web requests  
• **SQLite Database** – zero-config persistence (`bot/chronomancy.db`)  
• **Cloudflare Tunnel** – production-ready public URLs with security  

---

## Quick Start (Production)

```bash
# 1. Clone and setup
git clone <your-repo> && cd chronomancy
python -m venv .venv && .venv\Scripts\activate
pip install -r requirements.txt

# 2. Environment (.env file)
TELEGRAM_BOT_TOKEN=your_bot_token_from_botfather
WEBAPP_URL=https://api.chronomancy.app/

# 3. Start production server (with webhook support)
$env:WEBAPP_URL = "https://api.chronomancy.app/"
uvicorn updatedui.server:app --host 0.0.0.0 --port 8000

# 4. Start Cloudflare tunnel (separate terminal)
cloudflared tunnel --config config.yml run
```

**Access**: https://api.chronomancy.app/ or send `/start` to your Telegram bot.

For detailed setup instructions, see **[DEPLOYMENT.md](DEPLOYMENT.md)**.

---

## Architecture Overview

### Production Flow
1. **User** → Sends `/start` to Telegram bot
2. **Telegram** → Sends webhook to `https://api.chronomancy.app/telegram/BOT_TOKEN`  
3. **FastAPI Server** → Processes webhook, responds to Telegram
4. **Bot Response** → Shows "🌀 Open Chronomancy App" button
5. **User** → Taps button → Opens WebApp at `https://api.chronomancy.app/`
6. **WebApp** → Timer setup, anomaly reporting, full interface

### Key Components
- **Webhook Endpoint**: `/telegram/{bot_token}` - receives Telegram updates
- **Health Check**: `/health` - server status monitoring  
- **WebApp Root**: `/` - serves the Telegram miniapp interface
- **API Endpoints**: `/api/user/{user_id}/*` - timer and anomaly management

---

## Repo Layout
| Path | Purpose |
|------|---------|
| `bot/main.py` | Telegram bot with webhook handlers |
| `bot/chronomancy.db` | SQLite database (auto-created) |
| `updatedui/server.py` | FastAPI server (webhook + web + API) |
| `updatedui/*.html` | WebApp frontend files |
| `DEPLOYMENT.md` | Complete setup guide |
| `canon.yaml` | Canonical constants (Scott Wilber's framework) |

---

## Development vs Production

### Development (Local Testing)
```bash
# Simple local development
python -m bot.main  # Starts bot in polling mode
python updatedui/start_server.py  # Starts web server on localhost:8000
```

### Production (Webhook + Tunnel)
```bash
# Production deployment  
uvicorn updatedui.server:app --host 0.0.0.0 --port 8000  # Webhook server
cloudflared tunnel run  # Public tunnel
```

**Key Difference**: Production uses webhooks (faster, more reliable) while development uses polling (easier to debug).

---

## Environment Variables

```env
# Required
TELEGRAM_BOT_TOKEN=your_bot_token_from_botfather

# Production
WEBAPP_URL=https://api.chronomancy.app/        # Enables WebApp button
STRIPE_SECRET_KEY=sk_live_xxx                  # If using payment features

# Development  
WEBAPP_URL=http://localhost:8000/              # Local WebApp testing
```

---

## 🪄 Bot Commands & Features

### Personal timers & notifications
• `/window <start> <end> <N>` — Set your daily random-ping window (24-h `HH:MM`) and number of pings. *Scott Wilber:* “Benign unpredictability prevents anticipatory damping of the signal.”  
• Interactive **timezone picker** appears automatically after `/window`; stores offset so pings land in your subjective window.  
• `/schedule` — Show today's generated ping times (debug/dev).  
• (WebApp) **Mute** — Temporarily silence pings for N hours (`/mute` via API).

### Anomaly scanning
• Spontaneous **Beacon** pings ask: "Look around: what feels out-of-place?"; reply with text, photos, voice, or docs. *Scott Wilber:* direct phenomenological logging beats rigid forms.  
• `/future <message>` — Queue a note to be delivered at the next ping (intention setting / self-reminder).

### Profiles & stats
• `/profile` — View your timer settings, lifetime ping count, and anomaly tally.  
• `/activity` — Rolling 24-hour histogram of your responses.  
• `/reports` — List your recent anomaly submissions.  
• `/export` — GDPR-friendly full data export (CSV).

### Group mode
• `/groupwindow HH:MM HH:MM N` — Enable pings for a **group chat**; all members see the same Beacon.  
• `/gdisable` — Turn off group pings.  
• `/groupstats` — Leaderboard of member response rates.  
*Scott Wilber:* Collective rhythmic attention amplifies effect-size in MMI studies.

### Donation & premium
• `/support` or `/pass` — Opens TON transfer deep-link; ≥5 TON mints a **Lifetime PSI Pass** NFT.  
• Inline "💎 Support with TON" footer follows random pings (max 3 skips before hide).  
Justification (Scott Wilber): voluntary reciprocity sustains open-science tooling without paywalls.

### Admin / debugging
• `/poke` — Force-trigger the next alarm for the current chat.  
• `/testall` — Broadcast test ping to all users (rate-limited).  
• `/global` — Network-wide aggregate stats.  
• `/setgroup` — Manually tag a chat as group/private (edge-case recovery).

---

## Troubleshooting

### Bot Not Responding
```bash
# Check webhook status
curl https://api.chronomancy.app/health

# Check server logs for webhook processing
# Should see: "📥 Received webhook data"
```

### WebApp Button Missing
```bash
# Verify environment variable
echo $env:WEBAPP_URL  # Should be set before starting server

# Check bot startup logs
# Should see: "✅ Telegram webhook set → https://api.chronomancy.app/telegram/..."
```

### Tunnel Issues
```bash
# Check tunnel connections
cloudflared tunnel list
# Should show 4 active connections to Cloudflare edge

# Test connectivity
curl https://api.chronomancy.app/health
```

See **[DEPLOYMENT.md](DEPLOYMENT.md)** for complete troubleshooting guide.

---

## Success Indicators

When everything is working correctly:

✅ Server responds: `curl localhost:8000/health` → 200 OK  
✅ Tunnel active: `curl https://api.chronomancy.app/health` → 200 OK  
✅ Bot shows WebApp button when user sends `/start`  
✅ WebApp opens at `https://api.chronomancy.app/` when button tapped  
✅ Users can set timers and receive random pings  
✅ Anomaly reporting works through WebApp interface  

---

## Roadmap

- [x] **v1.0** – Webhook architecture + Cloudflare tunnel (DONE)
- [ ] **v1.1** – Docker containerization 
- [ ] **v1.2** – Automated deployment scripts
- [ ] **v2.0** – Multi-region deployment + CDN optimization

---

> *"Better to annoy some people with recurring alarms than have 20 million people use it once."*  
> — **Scott Wilber**, Chronomancy Canon 