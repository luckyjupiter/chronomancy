# Chronomancy – Minimal Telegram Time-Scanner

A **single-screen** Telegram WebApp + bot that pings you a few random times per day and asks you to scan your surroundings for anomalies.

This branch is the **clean, minimal core**.  Everything unrelated to the time-scanner has been moved out of the Git history (still on your disk, now ignored).

---

## ✨ Features

• Telegram bot (`bot/`) – stores each user's preferred window + daily ping count and delivers the pings.
• Web mini-app (`updatedui/`) – lets you pick times, frequency, and record anomaly observations.
• FastAPI backend – serves static assets and proxies helper APIs to the bot.
• SQLite persistence – zero-config (DB file lives in `bot/chronomancy.db`).
• Cloudflare Tunnel sample config – expose your local server at `chronomancy.app`.

---

## Quick Start

```bash
# 1. Python virtual-env (PowerShell / cmd)
python -m venv .venv && .venv\Scripts\activate
#    source .venv/bin/activate        # bash / zsh

# 2. Install deps
pip install -r requirements.txt

# 3. Telegram bot token (.env file)
copy .env.example .env   # then edit TELEGRAM_BOT_TOKEN=<your token>

# 4. Run bot + UI (port 5000)
python updatedui/start_server.py

# 5. (Optional) public URL via Cloudflare Tunnel
cloudflared tunnel run --token <your-tunnel-token>
```
Open http://localhost:5000 (or your tunnel URL) and hit **🚀 Start Temporal Scanning**.

---

## Repo Layout
| Path | Purpose |
|------|---------|
| `bot/` | Telegram bot + helper APIs |
| `updatedui/` | UI (HTML/JS/CSS) + FastAPI server |
| `canon.yaml` | Canonical constants (single source-of-truth) |
| `requirements.txt` | Consolidated dependencies |
| `.gitignore` | Ignores legacy folders that remain on disk |

---

## Roadmap
1. v0 – local demo + Cloudflare tunnel (DONE)
2. v1 – Docker image + Fly.io one-command deploy
3. v2 – OAuth login + Postgres option

---

> "Randomness is just unmodelled causality."
