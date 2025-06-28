#!/usr/bin/env python3
"""
Chronomancy Updated UI Server
Simple server to serve the new minimal interface and proxy API requests.

Scott Wilber's canonical minimal activation energy framework.
"""

import os
import sys
from pathlib import Path
import uvicorn
from fastapi import FastAPI, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, JSONResponse, HTMLResponse, Response
from fastapi.staticfiles import StaticFiles
import stripe
from dotenv import load_dotenv
import sqlite3
from telebot import types
import asyncio
import time
import logging
from typing import Optional, Dict, Any
from contextlib import asynccontextmanager
from collections import deque
import psutil
import threading
from bot.main import send_admin_alert

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Load environment variables
load_dotenv()

# Add project root to path for imports
ROOT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT_DIR))

# Configure Stripe
stripe_key = os.getenv("STRIPE_API_KEY") or os.getenv("stripe_api_key")
if stripe_key:
    stripe.api_key = stripe_key
    print("✅ Stripe API key loaded from environment")
else:
    print("⚠️  Warning: STRIPE_API_KEY not found in environment variables")
    print("📝 Create a .env file in this directory with: STRIPE_API_KEY=sk_test_your_secret_key")

# Circuit Breaker for Telegram API
class TelegramCircuitBreaker:
    def __init__(self, failure_threshold: int = 5, timeout: int = 60):
        self.failure_count = 0
        self.failure_threshold = failure_threshold
        self.timeout = timeout
        self.last_failure: Optional[float] = None
        self.state = "closed"  # closed, open, half-open
        self.lock = asyncio.Lock()

    async def call(self, func, *args, **kwargs):
        async with self.lock:
            if self.state == "open":
                if self.last_failure and time.time() - self.last_failure > self.timeout:
                    self.state = "half-open"
                    logger.info("Circuit breaker moved to half-open state")
                else:
                    raise Exception("Circuit breaker is open - Telegram API unavailable")

            try:
                result = await asyncio.get_event_loop().run_in_executor(
                    None, lambda: func(*args, **kwargs)
                )
                if self.state == "half-open":
                    self.state = "closed"
                    self.failure_count = 0
                    logger.info("Circuit breaker closed - Telegram API restored")
                return result
            except Exception as e:
                self.failure_count += 1
                logger.error(f"Telegram API call failed: {e}")
                if self.failure_count >= self.failure_threshold:
                    self.state = "open"
                    self.last_failure = time.time()
                    logger.error("Circuit breaker opened - Telegram API calls suspended")
                raise

# Global circuit breaker instance
telegram_circuit_breaker = TelegramCircuitBreaker()

# ---------------------------------------------------------------------------
# Alert manager – throttles admin notifications
# ---------------------------------------------------------------------------

class AlertManager:
    """Simple per-key cooldown alert dispatcher to Telegram admins."""

    def __init__(self, cooldown_seconds: int = 300):
        self.cooldown_seconds = cooldown_seconds
        self._last_alert_ts: Dict[str, float] = {}

    async def maybe_alert(self, key: str, message: str):
        now = time.time()
        last = self._last_alert_ts.get(key, 0)
        if now - last < self.cooldown_seconds:
            return  # still cooling down
        try:
            await asyncio.get_event_loop().run_in_executor(None, send_admin_alert, message)
            self._last_alert_ts[key] = now
            logger.warning(f"Admin alert sent [{key}]: {message}")
        except Exception as e:
            logger.error(f"Failed to send admin alert '{key}': {e}")

alert_manager = AlertManager()

# Database connection pool manager
class DatabaseConnectionPool:
    def __init__(self, db_path: str, max_connections: int = 20):
        self.db_path = db_path
        self.max_connections = max_connections
        self.pool = deque()
        self.active_connections = 0
        self.lock = threading.Lock()
        
    def get_connection(self):
        with self.lock:
            if self.pool:
                return self.pool.popleft()
            elif self.active_connections < self.max_connections:
                conn = sqlite3.connect(self.db_path, check_same_thread=False)
                conn.row_factory = sqlite3.Row
                self.active_connections += 1
                return conn
            else:
                raise Exception("Database connection pool exhausted")
    
    def return_connection(self, conn):
        with self.lock:
            if len(self.pool) < self.max_connections // 2:
                self.pool.append(conn)
            else:
                conn.close()
                self.active_connections -= 1

    @asynccontextmanager
    async def get_async_connection(self):
        conn = None
        try:
            conn = await asyncio.get_event_loop().run_in_executor(
                None, self.get_connection
            )
            yield conn
        finally:
            if conn:
                await asyncio.get_event_loop().run_in_executor(
                    None, self.return_connection, conn
                )

# Initialize connection pool
db_pool = DatabaseConnectionPool(os.path.join(ROOT_DIR, "bot", "chronomancy.db"))

# Import backend functionality
bot_functions_available = False
try:
    sys.path.append(str(ROOT_DIR))
    from bot.main import (
        get_user_timer_settings,
        set_user_timer,
        mute_user_timer,
        log_anomaly,
        get_challenge,
        bot as telegram_bot,
    )
    bot_functions_available = True
    print("✅ Successfully imported bot functions")
except ImportError as e:
    print(f"⚠️  Warning: Could not import bot modules: {e}")
    print("📍 Using fallback functions for testing")
    
    # Fallback implementations
    def get_user_timer_settings(user_id):
        return {"active": False, "window_start": "08:00", "window_end": "20:00", "daily_count": 3, "is_muted": False}
    
    def set_user_timer(user_id, **kwargs):
        print(f"📝 Fallback: Setting timer for user {user_id} with {kwargs}")
        return True
    
    def mute_user_timer(user_id, hours):
        print(f"🔇 Fallback: Muting user {user_id} for {hours} hours")
        return True
    
    def log_anomaly(user_id, description):
        print(f"📋 Fallback: Logging anomaly for user {user_id}: {description}")
        return True
    
    def get_challenge():
        return "Scan your surroundings for anything unusual..."

    # Dummy telegram_bot so code referencing it does not crash in fallback mode
    class _DummyBot:
        def process_new_updates(self, updates):
            pass

        def send_message(self, *args, **kwargs):
            print("[DummyBot] send_message", args, kwargs)

    telegram_bot = _DummyBot()

# System health monitoring
class HealthMonitor:
    def __init__(self):
        self.start_time = time.time()
        
    async def check_database_health(self) -> Dict[str, Any]:
        try:
            async with db_pool.get_async_connection() as conn:
                await asyncio.get_event_loop().run_in_executor(
                    None, lambda: conn.execute("SELECT 1").fetchone()
                )
            return {"status": "ok", "message": "Database accessible"}
        except Exception as e:
            return {"status": "error", "message": f"Database error: {str(e)}"}
    
    async def check_telegram_api(self) -> Dict[str, Any]:
        try:
            if telegram_circuit_breaker.state == "open":
                return {"status": "degraded", "message": "Circuit breaker open"}
            else:
                return {"status": "ok", "message": "Telegram API available"}
        except Exception as e:
            return {"status": "error", "message": f"Telegram API error: {str(e)}"}
    
    def get_memory_stats(self) -> Dict[str, Any]:
        try:
            process = psutil.Process()
            memory_info = process.memory_info()
            return {
                "status": "ok" if memory_info.rss < 1024*1024*1024 else "warning",  # 1GB threshold
                "rss_mb": memory_info.rss / 1024 / 1024,
                "vms_mb": memory_info.vms / 1024 / 1024,
                "percent": process.memory_percent()
            }
        except Exception as e:
            return {"status": "error", "message": f"Memory check error: {str(e)}"}
    
    def get_uptime_seconds(self) -> float:
        return time.time() - self.start_time

health_monitor = HealthMonitor()

# Initialize FastAPI app
app = FastAPI(title="Chronomancy Updated UI", version="1.0.0")

# Enable CORS for all origins (adjust for production)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Get base directory for this server
BASE_DIR = Path(__file__).parent

# Mount static files
app.mount("/css", StaticFiles(directory=BASE_DIR / "css"), name="css")
app.mount("/js", StaticFiles(directory=BASE_DIR / "js"), name="js")

@app.get("/")
async def serve_frontend():
    """Serve the new minimal Chronomancy UI"""
    return FileResponse(BASE_DIR / "index.html")

@app.get("/health")
async def health_check():
    """Basic health check endpoint"""
    return JSONResponse({"status": "healthy", "message": "Chronomancy Updated UI Server"})

@app.get("/health/detailed")
async def detailed_health():
    """Comprehensive health check with all system components"""
    health_status = {
        "database": await health_monitor.check_database_health(),
        "telegram_api": await health_monitor.check_telegram_api(),
        "memory_usage": health_monitor.get_memory_stats(),
        "uptime_seconds": health_monitor.get_uptime_seconds(),
        "circuit_breaker_state": telegram_circuit_breaker.state,
        "active_db_connections": db_pool.active_connections
    }
    
    overall_status = "healthy"
    for component, status in health_status.items():
        if isinstance(status, dict) and status.get("status") == "error":
            overall_status = "unhealthy"
            break
        elif isinstance(status, dict) and status.get("status") == "warning":
            overall_status = "degraded"
    
    return JSONResponse({
        "status": overall_status,
        "timestamp": time.time(),
        "checks": health_status
    })

@app.get("/api/user/{user_id}/status")
async def get_user_status(user_id: int):
    """Get current timer status for user"""
    try:
        settings = get_user_timer_settings(user_id)
        
        return JSONResponse({
            "timer_active": settings.get("active", False),
            "window_start": settings.get("window_start", "08:00"),
            "window_end": settings.get("window_end", "20:00"),
            "daily_count": settings.get("daily_count", 3),
            "is_muted": settings.get("is_muted", False),
            "muted_until": settings.get("muted_until"),
            "is_backer": settings.get("is_backer", False)
        })
    except Exception as e:
        logger.error(f"Error getting user status: {e}")
        # Return default status
        return JSONResponse({
            "timer_active": False,
            "window_start": "08:00",
            "window_end": "20:00",
            "daily_count": 3,
            "is_muted": False,
            "muted_until": None,
            "is_backer": False
        })

@app.post("/api/user/{user_id}/timer")
async def update_user_timer(user_id: int, timer_data: dict):
    """Set or update user timer settings"""
    try:
        success = set_user_timer(
            user_id=user_id,
            window_start=timer_data["window_start"],
            window_end=timer_data["window_end"],
            daily_count=timer_data["daily_count"],
            tz_offset=timer_data.get("tz_offset", 0)
        )
        
        if success:
            # Attempt to send confirmation via Telegram bot with circuit breaker
            confirmation_sent = False
            try:
                await telegram_circuit_breaker.call(
                    telegram_bot.send_message,
                    user_id,
                    f"✅ Scanner activated! I will ping you {timer_data['daily_count']} time(s) per day between {timer_data['window_start']} and {timer_data['window_end']}."
                )
                confirmation_sent = True
                logger.info(f"✅ Confirmation message sent successfully to user {user_id}")
            except Exception as e:
                logger.warning(f"⚠️  Could not send confirmation message to user {user_id}: {e}")
                confirmation_sent = False

            return JSONResponse({
                "status": "success",
                "message": "Timer updated",
                "confirmation_sent": confirmation_sent
            })
        else:
            raise HTTPException(status_code=500, detail="Failed to update timer")
            
    except Exception as e:
        logger.error(f"Error updating timer: {e}")
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/user/{user_id}/mute")
async def mute_timer(user_id: int, mute_data: dict):
    """Mute user timer for specified duration"""
    try:
        hours = mute_data.get("hours", 24)
        success = mute_user_timer(user_id, hours)
        
        if success:
            return JSONResponse({"status": "success", "message": f"Timer muted for {hours} hours"})
        else:
            raise HTTPException(status_code=500, detail="Failed to mute timer")
            
    except Exception as e:
        logger.error(f"Error muting timer: {e}")
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/user/{user_id}/anomaly")
async def submit_anomaly(user_id: int, anomaly_data: dict):
    """Submit anomaly observation"""
    try:
        description = anomaly_data["description"]
        timestamp = anomaly_data.get("timestamp")
        
        success = log_anomaly(user_id, description)
        
        if success:
            return JSONResponse({"status": "success", "message": "Anomaly recorded"})
        else:
            raise HTTPException(status_code=500, detail="Failed to record anomaly")
            
    except Exception as e:
        logger.error(f"Error recording anomaly: {e}")
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/api/challenge")
async def get_scanning_challenge():
    """Get random scanning challenge prompt"""
    try:
        challenge = get_challenge()
        return JSONResponse({"challenge": challenge})
    except Exception as e:
        logger.error(f"Error getting challenge: {e}")
        return JSONResponse({"challenge": "Scan your surroundings for anything unusual, unexpected, or meaningful..."})

@app.post("/api/create-checkout-session")
async def create_checkout_session(request_data: dict):
    """Create Stripe checkout session for lifetime pass"""
    try:
        user_info = request_data.get("user_info", {})
        
        # Create checkout session
        checkout_session = stripe.checkout.Session.create(
            payment_method_types=['card'],
            line_items=[{
                'price_data': {
                    'currency': 'usd',
                    'product_data': {
                        'name': 'Chronomancy Lifetime PSI Pass',
                        'description': 'Unlimited premium access to all Chronomancy features forever',
                    },
                    'unit_amount': 5000,  # $50.00 in cents
                },
                'quantity': 1,
            }],
            mode='payment',
            success_url='https://chronomancy.app/lifetime?success=true',
            cancel_url='https://chronomancy.app/lifetime?canceled=true',
            metadata={
                'telegram_id': str(user_info.get('telegram_id', '')),
                'first_name': user_info.get('first_name', ''),
                'last_name': user_info.get('last_name', ''),
                'username': user_info.get('username', ''),
                'product': 'lifetime_pass'
            }
        )
        
        return JSONResponse({"id": checkout_session.id})
        
    except Exception as e:
        logger.error(f"Error creating checkout session: {e}")
        return JSONResponse({"error": str(e)}, status_code=400)

@app.get("/theory")
async def serve_theory():
    """Serve theory primer page"""
    return FileResponse(BASE_DIR / "theory.html")

@app.get("/lifetime")
async def serve_lifetime():
    """Serve Lifetime PSI Pass page"""
    return FileResponse(BASE_DIR / "lifetime.html")

@app.on_event("startup")
async def startup_event():
    """Initialize webhook and start background tasks"""

    print("🏵️ Chronomancy API starting up – configuring Telegram webhook…")

    if not TOKEN:
        print("⚠️  TELEGRAM_BOT_TOKEN missing – webhook not set")
        return

    # Use configured public domain (default api.chronomancy.app)
    public_base = os.getenv("PUBLIC_BASE_URL", "https://api.chronomancy.app")
    webhook_url = f"{public_base}/telegram/{TOKEN}"

    try:
        telegram_bot.remove_webhook()
        telegram_bot.set_webhook(url=webhook_url, max_connections=40)
        print(f"✅ Telegram webhook set → {webhook_url}")
    except Exception as exc:
        print(f"⚠️  Could not set Telegram webhook: {exc}")

    # Start metrics watchdog
    async def metrics_watchdog():
        while True:
            try:
                # Memory usage
                mem = psutil.virtual_memory()
                if mem.percent > 85:
                    await alert_manager.maybe_alert("mem_high", f"Memory usage is {mem.percent}%")

                # DB pool saturation
                if db_pool.active_connections >= int(0.8 * db_pool.max_connections):
                    await alert_manager.maybe_alert(
                        "db_pool_high",
                        f"DB pool high usage: {db_pool.active_connections}/{db_pool.max_connections}",
                    )

                # Telegram API circuit breaker open
                if telegram_circuit_breaker.state == "open":
                    await alert_manager.maybe_alert("tg_circuit_open", "Telegram circuit breaker OPEN – messages blocked")
            except Exception as e:
                logger.error(f"Metrics watchdog error: {e}")
            await asyncio.sleep(30)

    asyncio.create_task(metrics_watchdog())

# ---------------------------------------------------------------------------
# Telegram webhook endpoint
# ---------------------------------------------------------------------------

TOKEN = os.getenv("TELEGRAM_BOT_TOKEN")

if TOKEN:

    @app.post(f"/telegram/{TOKEN}")
    async def telegram_webhook(request: Request):
        """Pass Telegram updates to the shared TeleBot instance.

        As prescribed by Scott Wilber's single-source canon, keep handler logic
        minimal: deserialize, hand off, return *ok*.
        """
        data = await request.body()
        try:
            print(f"📥 Received webhook data: {data.decode()[:500]}...")  # Log first 500 chars
            update = types.Update.de_json(data.decode())
            print(f"🔄 Processing update ID: {update.update_id}")
            telegram_bot.process_new_updates([update])
            print(f"✅ Successfully processed update ID: {update.update_id}")
        except Exception as exc:
            print(f"❌ Webhook error: {exc}")
            print(f"❌ Error type: {type(exc).__name__}")
            print(f"❌ Raw data: {data.decode()}")
            raise HTTPException(status_code=400, detail=str(exc))
        return "ok"

@app.get("/admin")
async def admin_dashboard():
    """Real-time admin dashboard for monitoring Chronomancy system"""
    return HTMLResponse(content="""
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Chronomancy Admin Dashboard</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            min-height: 100vh;
            padding: 20px;
        }
        .dashboard { max-width: 1200px; margin: 0 auto; }
        .header { text-align: center; margin-bottom: 30px; }
        .header h1 { font-size: 2.5em; margin-bottom: 10px; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }
        .header p { opacity: 0.9; font-size: 1.1em; }
        .stats-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; margin-bottom: 30px; }
        .stat-card { background: rgba(255,255,255,0.1); border-radius: 15px; padding: 20px; backdrop-filter: blur(10px); border: 1px solid rgba(255,255,255,0.2); }
        .stat-card h3 { margin-bottom: 15px; color: #ffd700; }
        .stat-value { font-size: 2em; font-weight: bold; margin-bottom: 5px; }
        .stat-label { opacity: 0.8; font-size: 0.9em; }
        .activity-log { background: rgba(0,0,0,0.2); border-radius: 15px; padding: 20px; margin-bottom: 20px; }
        .log-item { padding: 8px 0; border-bottom: 1px solid rgba(255,255,255,0.1); display: flex; justify-content: space-between; }
        .log-item:last-child { border-bottom: none; }
        .controls { display: flex; gap: 15px; flex-wrap: wrap; justify-content: center; margin-top: 20px; }
        .btn { padding: 12px 24px; border: none; border-radius: 8px; cursor: pointer; font-weight: bold; transition: all 0.3s; }
        .btn-primary { background: #4CAF50; color: white; }
        .btn-secondary { background: #2196F3; color: white; }
        .btn-danger { background: #f44336; color: white; }
        .btn:hover { transform: translateY(-2px); box-shadow: 0 4px 12px rgba(0,0,0,0.3); }
        .status-indicator { display: inline-block; width: 12px; height: 12px; border-radius: 50%; margin-right: 8px; }
        .status-online { background: #4CAF50; }
        .status-offline { background: #f44336; }
        .live-data { opacity: 0.7; font-style: italic; }
        .table { width: 100%; border-collapse: collapse; margin-top: 15px; }
        .table th, .table td { padding: 8px 12px; text-align: left; border-bottom: 1px solid rgba(255,255,255,0.1); }
        .table th { background: rgba(255,255,255,0.1); font-weight: bold; }
        .refresh-time { text-align: center; margin-top: 20px; opacity: 0.7; }
    </style>
</head>
<body>
    <div class="dashboard">
        <div class="header">
            <h1>🌀 Chronomancy Admin Dashboard</h1>
            <p>Real-time monitoring of the temporal scanning network</p>
        </div>

        <div class="stats-grid">
            <div class="stat-card">
                <h3>🤖 Bot Status</h3>
                <div class="stat-value" id="bot-status">
                    <span class="status-indicator status-online"></span>ONLINE
                </div>
                <div class="stat-label">Webhook processing active</div>
            </div>

            <div class="stat-card">
                <h3>👥 Active Users</h3>
                <div class="stat-value" id="active-users">Loading...</div>
                <div class="stat-label">Users with timer settings</div>
            </div>

            <div class="stat-card">
                <h3>📤 Pings Today</h3>
                <div class="stat-value" id="pings-today">Loading...</div>
                <div class="stat-label">Messages sent in last 24h</div>
            </div>

            <div class="stat-card">
                <h3>🔍 Anomalies Logged</h3>
                <div class="stat-value" id="anomalies-today">Loading...</div>
                <div class="stat-label">Reports received today</div>
            </div>

            <div class="stat-card">
                <h3>⏰ Next Ping</h3>
                <div class="stat-value" id="next-ping">Loading...</div>
                <div class="stat-label">Scheduled alarm time</div>
            </div>

            <div class="stat-card">
                <h3>🌐 Server Health</h3>
                <div class="stat-value" id="server-health">
                    <span class="status-indicator status-online"></span>HEALTHY
                </div>
                <div class="stat-label">API endpoint status</div>
            </div>
        </div>

        <div class="activity-log">
            <h3>📊 Recent Activity</h3>
            <div id="activity-list">Loading activity feed...</div>
        </div>

        <div class="activity-log">
            <h3>⏱️ Upcoming Schedule</h3>
            <div id="schedule-list">Loading schedule...</div>
        </div>

        <div class="controls">
            <button class="btn btn-primary" onclick="sendTestPing()">🧪 Send Global Test Ping</button>
            <button class="btn btn-secondary" onclick="refreshData()">🔄 Refresh Data</button>
            <button class="btn btn-secondary" onclick="exportLogs()">📥 Export Logs</button>
        </div>

        <div class="refresh-time">
            Last updated: <span id="last-refresh">Never</span> | Auto-refresh: <span id="countdown">30s</span>
        </div>
    </div>

    <script>
        let countdownTimer = 30;
        let intervalId;

        async function fetchStats() {
            try {
                const response = await fetch('/admin/stats');
                const data = await response.json();
                
                document.getElementById('active-users').textContent = data.active_users;
                document.getElementById('pings-today').textContent = data.pings_today;
                document.getElementById('anomalies-today').textContent = data.anomalies_today;
                document.getElementById('next-ping').textContent = data.next_ping || 'None scheduled';
                
                // Update activity log
                const activityHtml = data.recent_activity.map(item => 
                    `<div class="log-item">
                        <span>${item.description}</span>
                        <span class="live-data">${item.timestamp}</span>
                    </div>`
                ).join('');
                document.getElementById('activity-list').innerHTML = activityHtml || '<div class="live-data">No recent activity</div>';
                
                // Update schedule
                const scheduleHtml = data.upcoming_schedule.map(item => 
                    `<div class="log-item">
                        <span>User ${item.user_id}</span>
                        <span class="live-data">${item.time} (${item.countdown})</span>
                    </div>`
                ).join('');
                document.getElementById('schedule-list').innerHTML = scheduleHtml || '<div class="live-data">No upcoming pings</div>';
                
                document.getElementById('last-refresh').textContent = new Date().toLocaleTimeString();
                
            } catch (error) {
                console.error('Failed to fetch stats:', error);
                document.getElementById('server-health').innerHTML = '<span class="status-indicator status-offline"></span>ERROR';
            }
        }

        async function sendTestPing() {
            if (!confirm('Send a test ping to all active users?')) return;
            
            try {
                const response = await fetch('/admin/test-ping', { method: 'POST' });
                const result = await response.json();
                alert(`Test ping sent to ${result.sent_count} users`);
                fetchStats();
            } catch (error) {
                alert('Failed to send test ping: ' + error.message);
            }
        }

        function refreshData() {
            fetchStats();
            countdownTimer = 30;
        }

        function exportLogs() {
            window.open('/admin/export-logs');
        }

        function startCountdown() {
            intervalId = setInterval(() => {
                countdownTimer--;
                document.getElementById('countdown').textContent = countdownTimer + 's';
                
                if (countdownTimer <= 0) {
                    fetchStats();
                    countdownTimer = 30;
                }
            }, 1000);
        }

        // Initialize
        fetchStats();
        startCountdown();
    </script>
</body>
</html>
    """)

@app.get("/admin/stats")
async def admin_stats():
    """API endpoint for admin dashboard statistics"""
    try:
        # Import bot functions
        import sys
        sys.path.append('bot')
        import main as bot_main
        
        conn = bot_main.CONN
        
        # Get active users count
        cur = conn.execute("""
            SELECT COUNT(*) FROM users 
            WHERE window_start IS NOT NULL 
            AND window_end IS NOT NULL 
            AND daily_count > 0
        """)
        active_users = cur.fetchone()[0]
        
        # Get pings today
        cur = conn.execute("""
            SELECT COUNT(*) FROM pings 
            WHERE DATE(sent_at_utc) = DATE('now')
        """)
        pings_today = cur.fetchone()[0]
        
        # Get anomalies today
        cur = conn.execute("""
            SELECT COUNT(*) FROM anomalies 
            WHERE DATE(created_at) = DATE('now')
        """)
        anomalies_today = cur.fetchone()[0]
        
        # Get recent activity
        cur = conn.execute("""
            SELECT 'Ping sent to user ' || user_id as description, 
                   datetime(sent_at_utc, 'localtime') as timestamp
            FROM pings 
            ORDER BY sent_at_utc DESC 
            LIMIT 10
        """)
        recent_activity = [{"description": row[0], "timestamp": row[1]} for row in cur.fetchall()]
        
        # Get next scheduled ping
        next_ping = "Loading..."
        try:
            import datetime as dt
            now = dt.datetime.utcnow()
            next_alarms = []
            
            for user_id, cfg in bot_main.USERS.items():
                if cfg.todays_alarms:
                    future_alarms = [alarm for alarm in cfg.todays_alarms if alarm > now]
                    if future_alarms:
                        next_alarms.extend([(alarm, user_id) for alarm in future_alarms])
            
            if next_alarms:
                next_alarms.sort()
                next_alarm_time, next_user = next_alarms[0]
                time_diff = next_alarm_time - now
                hours = int(time_diff.total_seconds() // 3600)
                minutes = int((time_diff.total_seconds() % 3600) // 60)
                next_ping = f"{next_alarm_time.strftime('%H:%M:%S')} (in {hours}h {minutes}m)"
            else:
                next_ping = "None scheduled"
                
        except Exception as e:
            next_ping = f"Error: {str(e)}"
        
        # Get upcoming schedule
        upcoming_schedule = []
        try:
            for user_id, cfg in bot_main.USERS.items():
                if cfg.todays_alarms:
                    future_alarms = [alarm for alarm in cfg.todays_alarms if alarm > now]
                    if future_alarms:
                        alarm_time = future_alarms[0]
                        time_diff = alarm_time - now
                        hours = int(time_diff.total_seconds() // 3600)
                        minutes = int((time_diff.total_seconds() % 3600) // 60)
                        upcoming_schedule.append({
                            "user_id": user_id,
                            "time": alarm_time.strftime('%H:%M:%S'),
                            "countdown": f"in {hours}h {minutes}m"
                        })
            
            upcoming_schedule.sort(key=lambda x: x["time"])
            upcoming_schedule = upcoming_schedule[:10]  # Top 10
            
        except Exception as e:
            upcoming_schedule = [{"user_id": "Error", "time": str(e), "countdown": ""}]
        
        return {
            "active_users": active_users,
            "pings_today": pings_today,
            "anomalies_today": anomalies_today,
            "next_ping": next_ping,
            "recent_activity": recent_activity,
            "upcoming_schedule": upcoming_schedule
        }
        
    except Exception as e:
        logger.error(f"Error fetching admin stats: {e}")
        return {
            "active_users": "Error",
            "pings_today": "Error", 
            "anomalies_today": "Error",
            "next_ping": "Error",
            "recent_activity": [],
            "upcoming_schedule": []
        }

@app.post("/admin/test-ping")
async def admin_test_ping():
    """Send test ping to all active users (admin only)"""
    try:
        # Import bot functions
        import sys
        sys.path.append('bot')
        import main as bot_main
        
        conn = bot_main.CONN
        
        # Get all active users
        cur = conn.execute("""
            SELECT chat_id FROM users 
            WHERE window_start IS NOT NULL 
            AND window_end IS NOT NULL 
            AND daily_count > 0
        """)
        active_users = [row[0] for row in cur.fetchall()]
        
        sent_count = 0
        for user_id in active_users:
            try:
                test_msg = f"""🧪 <b>Admin Test Ping!</b>

This is a global test from the admin dashboard.

🎯 <b>Instructions:</b>
1. Look around your current environment
2. Notice anything unusual or anomalous  
3. Reply to this message with your observations

<b>Challenge:</b> {bot_main.get_challenge()}

<i>This was a test - regular scheduled pings continue as normal.</i>"""
                
                bot_main.send_ping(user_id, test_msg, ping_type="admin_test", user_id=user_id)
                sent_count += 1
            except Exception as e:
                logger.error(f"Failed to send admin test ping to {user_id}: {e}")
        
        return {"sent_count": sent_count, "total_users": len(active_users)}
        
    except Exception as e:
        logger.error(f"Error sending admin test ping: {e}")
        return {"error": str(e), "sent_count": 0}

@app.get("/admin/export-logs")
async def export_logs():
    """Export system logs as CSV"""
    try:
        import sys
        sys.path.append('bot')
        import main as bot_main
        import io
        import csv
        
        conn = bot_main.CONN
        
        # Get comprehensive log data
        cur = conn.execute("""
            SELECT 
                p.sent_at_utc,
                p.chat_id,
                p.user_id, 
                p.ping_type,
                CASE WHEN a.id IS NOT NULL THEN 'Yes' ELSE 'No' END as responded,
                a.text as anomaly_text,
                a.media_type
            FROM pings p
            LEFT JOIN anomalies a ON p.id = a.ping_id
            ORDER BY p.sent_at_utc DESC
            LIMIT 1000
        """)
        
        output = io.StringIO()
        writer = csv.writer(output)
        writer.writerow(['Timestamp', 'Chat ID', 'User ID', 'Ping Type', 'Responded', 'Anomaly Text', 'Media Type'])
        
        for row in cur.fetchall():
            writer.writerow(row)
        
        content = output.getvalue()
        output.close()
        
        return Response(
            content=content,
            media_type="text/csv",
            headers={"Content-Disposition": "attachment; filename=chronomancy_logs.csv"}
        )
        
    except Exception as e:
        logger.error(f"Error exporting logs: {e}")
        return {"error": str(e)}

if __name__ == "__main__":
    print("🏵️ Starting Chronomancy Updated UI Server...")
    print("🎨 Scott Wilber's minimal activation energy framework")
    print("📍 Serving at http://localhost:8000")
    
    # Run server
    uvicorn.run(
        app,
        host="0.0.0.0",
        port=8000,
        log_level="info",
        access_log=True
    ) 