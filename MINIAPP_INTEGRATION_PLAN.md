# Chronomancy: Telegram Bot to Mini App Integration Plan

## Overview

This document outlines the strategy for evolving Chronomancy from a Telegram bot interface to a rich Telegram Mini App experience while maintaining the robust backend scheduling and ping system already implemented. Using self-hosting with Cloudflare Tunnel for secure, simple deployment.

## Current State Analysis

### Existing Bot Capabilities
- ✅ Multi-layered ping system (global sync, group-level, personal)
- ✅ Timezone detection and user-friendly setup
- ✅ Comprehensive database schema with anomaly tracking
- ✅ Group membership management
- ✅ Future-self messaging
- ✅ SQLite persistence with auto-migration
- ✅ Deterministic RNG using π for global synchronization

### Bot Architecture Strengths to Preserve
- **Client-side autonomy**: No scaling costs for ping generation
- **Robust scheduling**: UTC-based with timezone offset handling
- **Data persistence**: Comprehensive anomaly and ping tracking
- **Group coordination**: Multi-level temporal synchronization

## Mini App Architecture

### Hybrid Model: Bot + Web App + Cloudflare Tunnel
```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Telegram      │    │   Mini App      │    │   Self-Hosted   │
│   Client        │◄──►│   (Web UI)      │◄──►│   Bot + API     │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                │                        │
                                │                        │
                        ┌─────────────────┐    ┌─────────────────┐
                        │  Cloudflare     │    │   Local Server  │
                        │  Tunnel         │◄──►│   (Your PC)     │
                        └─────────────────┘    └─────────────────┘
```

**Benefits of Cloudflare Tunnel:**
- No port forwarding or firewall configuration
- Automatic HTTPS with Cloudflare certificates
- DDoS protection and global CDN
- Zero trust security model
- Easy domain setup (e.g., `chronomancy.yourdomain.com`)

## Self-Hosting Setup

### Server Requirements
```bash
# Minimal hardware requirements
- RAM: 1GB (SQLite + Python + static files)
- Storage: 10GB (database growth + media uploads)
- CPU: Single core sufficient (client-side scheduling)
- OS: Windows/Linux/macOS (Python compatible)
```

### Cloudflare Tunnel Configuration
```yaml
# cloudflared.yml
tunnel: chronomancy-tunnel
credentials-file: /path/to/credentials.json

ingress:
  # Mini App static files
  - hostname: chronomancy.yourdomain.com
    path: /app/*
    service: http://localhost:8080
  
  # Bot API endpoints
  - hostname: chronomancy.yourdomain.com
    path: /api/*
    service: http://localhost:5000
  
  # Telegram webhook
  - hostname: chronomancy.yourdomain.com
    path: /webhook
    service: http://localhost:5000
  
  # Catch-all
  - service: http_status:404
```

### Updated Bot Architecture
```python
# Enhanced bot structure for Mini App integration
bot/
├── main.py              # Bot + API server
├── miniapp_api.py       # REST endpoints for Mini App
├── static/              # Mini App files
│   ├── index.html
│   ├── app.js
│   ├── app.css
│   └── assets/
├── database.py          # Existing SQLite logic
├── scheduler.py         # Existing ping scheduling
└── requirements.txt     # Dependencies
```

## User Experience Flow

### 1. Initial Onboarding
**Current Bot Flow:**
```
/start → timezone detection → window selection → ping scheduling
```

**Mini App Enhancement:**
```
Welcome Screen → Animated timezone setup → Visual window picker → Ping preview timeline
```

**Implementation:**
- Bot handles timezone detection logic
- Mini App provides rich visual interface
- Real-time preview of upcoming pings

### 2. Daily Ping Management
**Mini App Interface:**
```
┌─────────────────────────────────────┐
│  Today's Ping Schedule              │
│  ○ Global Sync: 14:23 (in 2h 15m)  │
│  ○ Personal: 17:45 (in 5h 37m)     │
│  ○ Personal: 20:12 (in 8h 4m)      │
│                                     │
│  [- Remove Ping] [+ Add Ping]       │
│                                     │
│  Quick Mute: [1h] [3h] [Until 9AM] │
└─────────────────────────────────────┘
```

### 3. Anomaly Timeline
```
┌─────────────────────────────────────┐
│  Your Temporal Anomalies            │
│                                     │
│  📸 Today 14:23 - Strange shadow    │
│  🎵 Yesterday 17:45 - Bird sounds   │
│  📝 3 days ago - Future message     │
│  🔮 1 week ago - Sync moment        │
│                                     │
│  [Filter] [Search] [Export]         │
└─────────────────────────────────────┘
```

## Implementation Status

✅ **COMPLETED** - All core components have been implemented and are ready for deployment!

### What's Been Built:
- ✅ **Complete Mini App Frontend** (`miniapp/index.html` + `/js/`)
  - Three.js 3D temporal visualizations with animated spirals and particles
  - Full Telegram Mini App SDK integration with theme support
  - Responsive UI with floating panels and haptic feedback
  - Real-time event handling and WebSocket-ready architecture

- ✅ **Flask API Server** (`miniapp/server.py`)
  - RESTful endpoints connecting to existing bot database
  - Telegram WebApp authentication and user management
  - Anomaly reporting with geolocation and media support
  - Global sync time calculations using π-seeded deterministic RNG

- ✅ **Deployment Infrastructure**
  - Cloudflare Tunnel configuration (`cloudflare-tunnel.yml`)
  - Cross-platform startup scripts (`start.sh` + `start.bat`)
  - Production-ready Python dependencies (`requirements.txt`)
  - Comprehensive documentation (`README.md`)

### Quick Deployment:
1. `cd miniapp`
2. Run `./start.sh` (Linux/Mac) or `start.bat` (Windows)
3. Configure Cloudflare Tunnel for public access
4. Set Mini App URL in @BotFather

The Mini App seamlessly integrates with your existing bot database and scheduling system!

## Technical Implementation Reference

### Enhanced Bot Server (Alternative Integration)
```python
from flask import Flask, request, jsonify, send_from_directory
from telebot import TeleBot
import os

app = Flask(__name__, static_folder='static')
bot = TeleBot(os.getenv('BOT_TOKEN'))

# Existing bot functionality
@bot.message_handler(commands=['start'])
def start_handler(message):
    # Existing logic + Mini App link
    markup = telebot.types.InlineKeyboardMarkup()
    webapp_btn = telebot.types.WebAppInfo(url="https://chronomancy.yourdomain.com/app")
    markup.add(telebot.types.InlineKeyboardButton("Open Chronomancy", web_app=webapp_btn))
    bot.send_message(message.chat.id, "Welcome to Chronomancy!", reply_markup=markup)

# Mini App API endpoints
@app.route('/api/user/current')
def get_current_user():
    # Validate Telegram WebApp data
    # Return user info from database
    pass

@app.route('/api/pings/schedule')
def get_ping_schedule():
    # Return upcoming pings for user
    pass

@app.route('/api/anomalies', methods=['GET', 'POST'])
def handle_anomalies():
    # GET: return user's anomalies
    # POST: save new anomaly
    pass

# Serve Mini App
@app.route('/app')
@app.route('/app/<path:filename>')
def serve_miniapp(filename='index.html'):
    return send_from_directory('static', filename)

# Webhook for Telegram
@app.route('/webhook', methods=['POST'])
def webhook():
    bot.process_new_updates([telebot.types.Update.de_json(request.stream.read().decode("utf-8"))])
    return "OK"

if __name__ == '__main__':
    # Start both Flask (for Mini App) and bot polling
    app.run(host='localhost', port=5000)
```

### Mini App Frontend (static/app.js)
```javascript
// Initialize Telegram Mini App
window.Telegram.WebApp.ready();

class ChromonomancyApp {
    constructor() {
        this.initUser = window.Telegram.WebApp.initDataUnsafe.user;
        this.setupEventListeners();
        this.loadUserData();
    }

    async loadUserData() {
        try {
            const response = await fetch('/api/user/current', {
                headers: {
                    'Authorization': window.Telegram.WebApp.initData
                }
            });
            this.userData = await response.json();
            this.renderDashboard();
        } catch (error) {
            console.error('Failed to load user data:', error);
        }
    }

    renderDashboard() {
        // Render ping schedule
        this.loadPingSchedule();
        
        // Render anomaly timeline
        this.loadAnomalies();
        
        // Setup ping controls
        this.setupPingControls();
    }

    async loadPingSchedule() {
        const pings = await fetch('/api/pings/schedule').then(r => r.json());
        const scheduleDiv = document.getElementById('ping-schedule');
        
        scheduleDiv.innerHTML = pings.map(ping => `
            <div class="ping-item ${ping.type}">
                <span class="ping-time">${ping.formatted_time}</span>
                <span class="ping-countdown">(in ${ping.countdown})</span>
                <span class="ping-type">${ping.type}</span>
            </div>
        `).join('');
    }

    setupPingControls() {
        document.getElementById('add-ping').onclick = () => {
            fetch('/api/pings/add', {method: 'POST'})
                .then(() => this.loadPingSchedule());
        };

        document.getElementById('remove-ping').onclick = () => {
            fetch('/api/pings/remove', {method: 'POST'})
                .then(() => this.loadPingSchedule());
        };
    }
}

// Initialize app when page loads
document.addEventListener('DOMContentLoaded', () => {
    new ChromonomancyApp();
});
```

## Deployment Process

### 1. Local Development Setup
```bash
# Clone repository
git clone https://github.com/luckyjupiter/chronomancy.git
cd chronomancy

# Install dependencies
pip install -r requirements.txt

# Install Cloudflare Tunnel
# Windows:
winget install --id Cloudflare.cloudflared
# macOS:
brew install cloudflared
# Linux:
wget https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64.deb
sudo dpkg -i cloudflared-linux-amd64.deb

# Create Mini App directory
mkdir -p bot/static
```

### 2. Cloudflare Tunnel Setup
```bash
# Authenticate with Cloudflare
cloudflared tunnel login

# Create tunnel
cloudflared tunnel create chronomancy-tunnel

# Configure tunnel (save credentials)
cloudflared tunnel route dns chronomancy-tunnel chronomancy.yourdomain.com

# Create configuration file
cat > ~/.cloudflared/config.yml << EOF
tunnel: chronomancy-tunnel
credentials-file: ~/.cloudflared/[tunnel-id].json

ingress:
  - hostname: chronomancy.yourdomain.com
    service: http://localhost:5000
  - service: http_status:404
EOF
```

### 3. Production Deployment
```bash
# Start the bot server
cd bot
python main.py &

# Start Cloudflare Tunnel
cloudflared tunnel run chronomancy-tunnel &

# Optional: Create systemd services for auto-restart
```

### 4. Telegram Bot Configuration
```python
# Set webhook URL
import requests
BOT_TOKEN = "your_bot_token"
WEBHOOK_URL = "https://chronomancy.yourdomain.com/webhook"

requests.post(f"https://api.telegram.org/bot{BOT_TOKEN}/setWebhook", 
              json={"url": WEBHOOK_URL})
```

## Database Schema Extensions

### Mini App Analytics
```sql
CREATE TABLE miniapp_sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    session_start TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    session_end TIMESTAMP,
    pages_visited TEXT, -- JSON array
    actions_performed TEXT, -- JSON array
    FOREIGN KEY (user_id) REFERENCES users(id)
);

CREATE TABLE media_metadata (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    anomaly_id INTEGER NOT NULL,
    file_type TEXT NOT NULL,
    file_size INTEGER,
    location_lat REAL,
    location_lon REAL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (anomaly_id) REFERENCES anomalies(id)
);
```

## Security Considerations

### Telegram WebApp Validation
```python
import hmac
import hashlib
from urllib.parse import parse_qsl

def validate_telegram_data(init_data, bot_token):
    """Validate Telegram Mini App init data"""
    try:
        parsed_data = dict(parse_qsl(init_data))
        hash_value = parsed_data.pop('hash', '')
        
        data_check_string = '\n'.join([f"{k}={v}" for k, v in sorted(parsed_data.items())])
        secret_key = hmac.new(b"WebAppData", bot_token.encode(), hashlib.sha256).digest()
        calculated_hash = hmac.new(secret_key, data_check_string.encode(), hashlib.sha256).hexdigest()
        
        return hmac.compare_digest(calculated_hash, hash_value)
    except Exception:
        return False
```

### Cloudflare Security Features
- Automatic DDoS protection
- SSL/TLS encryption
- Access control rules
- Rate limiting
- Bot detection

## Performance Optimization

### Static Asset Optimization
```javascript
// Service Worker for offline capability
self.addEventListener('install', event => {
    event.waitUntil(
        caches.open('chronomancy-v1').then(cache => {
            return cache.addAll([
                '/app/',
                '/app/app.js',
                '/app/app.css',
                '/app/manifest.json'
            ]);
        })
    );
});
```

### Database Optimization
```sql
-- Indexes for fast queries
CREATE INDEX idx_pings_user_time ON pings(user_id, scheduled_time);
CREATE INDEX idx_anomalies_user_created ON anomalies(user_id, created_at);
CREATE INDEX idx_group_members_chat_user ON group_members(chat_id, user_id);
```

## Monitoring and Maintenance

### Health Checks
```python
@app.route('/health')
def health_check():
    return {
        'status': 'healthy',
        'database': 'connected' if check_db_connection() else 'error',
        'bot': 'active' if bot.get_me() else 'error',
        'timestamp': datetime.utcnow().isoformat()
    }
```

### Backup Strategy
```bash
# Automated daily backups
#!/bin/bash
DATE=$(date +%Y%m%d)
cp bot/chronomancy.db backups/chronomancy_$DATE.db
gzip backups/chronomancy_$DATE.db

# Keep last 30 days
find backups/ -name "*.gz" -mtime +30 -delete
```

## Next Steps

1. **Week 1**: Set up Cloudflare Tunnel and basic Mini App structure
2. **Week 2**: Implement user onboarding flow in Mini App
3. **Week 3**: Add ping management and anomaly capture interfaces
4. **Week 4**: Deploy timeline visualization and test with core users

This self-hosted approach with Cloudflare Tunnel gives you full control over the Chronomancy infrastructure while maintaining enterprise-grade security and performance. 