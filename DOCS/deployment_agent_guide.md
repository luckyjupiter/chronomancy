# Chronomancy Deployment Runbook (Ops Agent)

> Scott Wilber canon: "Repeatability beats heroism – write it down so anyone can do it at 3 a.m."

This runbook is for the automation agent (or human SRE) who needs to spin up a fresh production node from scratch.

---

## 0. Inputs

| Secret | Description |
|--------|-------------|
| `TELEGRAM_BOT_TOKEN` | BotFather token for production bot |
| `STRIPE_API_KEY`    | Live Stripe secret key (optional) |
| `CLOUDFLARE_TUNNEL_ID` & creds JSON | Issued after `cloudflared tunnel create` |

The agent must receive these via secure channel (HashiCorp Vault, CI secret store, etc.).

---

## 1. Provision VM

```
# 2 vCPU / 2 GB is fine up to 5 k users
az vm create …   # or doctl, gcloud, etc.
# open inbound 22/tcp (SSH) only – HTTP is proxied via Cloudflare
```

Set hostname `chronomancy-prod-01` and point A-record `api.chronomancy.app` to **no IP** (orange-cloud DNS entry, tunnel only).

---

## 2. Bootstrap script (idempotent)

```bash
#!/usr/bin/env bash
set -euo pipefail

sudo apt update && sudo apt install -y git python3.11 python3.11-venv cloudflared

useradd -m -s /bin/bash chronomancy || true
sudo -u chronomancy bash <<'EOS'
  cd ~
  git clone https://github.com/<org>/chronomancy.git || true
  cd chronomancy && git pull

  python3.11 -m venv .venv
  source .venv/bin/activate
  pip install --upgrade pip
  pip install -r requirements.txt
EOS
```

---

## 3. Drop secrets

```
/opt/chronomancy/.env            # TELEGRAM_BOT_TOKEN, STRIPE_API_KEY, WEBAPP_URL
~/.cloudflared/chronomancy.json  # tunnel creds file
```

Make sure perms are `600` and owner `chronomancy`.

---

## 4. Cloudflare tunnel service

`/etc/cloudflared/config.yml`:
```yaml
tunnel: ${CLOUDFLARE_TUNNEL_ID}
credentials-file: /home/chronomancy/.cloudflared/chronomancy.json
ingress:
  - hostname: api.chronomancy.app
    service: http://localhost:8000
  - hostname: admin.chronomancy.app
    service: http://localhost:9000
  - hostname: ws.chronomancy.app
    service: ws://localhost:8765
  - hostname: api.chronomancy.app
    path: /docs
    service: http://localhost:8000
  - service: http_status:404
no-chunked-encoding: true
```
Enable:
```
sudo systemctl enable --now cloudflared
```
Check:
```
cloudflared tunnel info ${CLOUDFLARE_TUNNEL_ID}
# should show 4 connections
```

### 4.1 Adding additional services on the same tunnel
Need a new sub-service? Just extend `ingress` and restart cloudflared:
```yaml
ingress:
  - hostname: api.chronomancy.app
    service: http://localhost:8000  # main API
  - hostname: admin.chronomancy.app
    service: http://localhost:9000  # admin UI
  - hostname: ws.chronomancy.app
    service: ws://localhost:8765    # WebSocket stream
  - hostname: api.chronomancy.app
    path: /docs
    service: http://localhost:8000  # path-based route
  - service: http_status:404
```
Reload:
```bash
sudo systemctl restart cloudflared
```

---

## 5. Application service

`/etc/systemd/system/chronomancy.service`:
```ini
[Unit]
Description=Chronomancy API & Webhook Server
After=network.target cloudflared.service

[Service]
Type=simple
User=chronomancy
WorkingDirectory=/home/chronomancy/chronomancy
EnvironmentFile=/home/chronomancy/chronomancy/.env
ExecStart=/home/chronomancy/chronomancy/.venv/bin/uvicorn updatedui.server:app --host 0.0.0.0 --port 8000 --workers 2
Restart=always

[Install]
WantedBy=multi-user.target
```
Enable & start:
```
sudo systemctl daemon-reload
sudo systemctl enable --now chronomancy
```

---

## 6. Post-deploy checklist

1. `curl -fsSL http://localhost:8000/health` → 200 OK
2. `curl -fsSL https://api.chronomancy.app/health` → 200 OK
3. `journalctl -u chronomancy -f` – ensure webhook set message.
4. Send `/poke` to bot – should receive ping.

---

## 7. Ongoing operations

| Metric | Threshold | Action |
|--------|-----------|--------|
| Memory | >85 % | Alert fired, consider VM resize |
| DB pool usage | ≥80 % | Plan Postgres migration |
| Telegram CB state | open | Check Telegram status page |

Alerts are auto-pushed to `ADMIN_USER_IDS` via `send_admin_alert` (5-min cooldown).

---

## 8. Zero-downtime update procedure

```
ssh chronomancy-prod-01
sudo systemctl stop chronomancy
cd /home/chronomancy/chronomancy && git pull && source .venv/bin/activate && pip install -r requirements.txt
sudo systemctl start chronomancy
```

Rollback = `git checkout <prev_sha>` + restart.

---

Happy scanning! 🌀 