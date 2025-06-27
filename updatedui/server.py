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
from fastapi.responses import FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
import stripe
from dotenv import load_dotenv
import sqlite3
from telebot import types

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
        print(f"Error getting user status: {e}")
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
            # Attempt to send confirmation via Telegram bot
            confirmation_sent = False
            try:
                print(f"📱 Attempting to send confirmation to user {user_id}")
                telegram_bot.send_message(
                    user_id,
                    f"✅ Scanner activated! I will ping you {timer_data['daily_count']} time(s) per day between {timer_data['window_start']} and {timer_data['window_end']}.")
                confirmation_sent = True
                print(f"✅ Confirmation message sent successfully to user {user_id}")
            except Exception as e:
                print(f"⚠️  Could not send confirmation message to user {user_id}: {e}")
                print(f"⚠️  Exception type: {type(e).__name__}")
                print(f"⚠️  This likely means the user hasn't started the bot yet")
                confirmation_sent = False

            return JSONResponse({
                "status": "success",
                "message": "Timer updated",
                "confirmation_sent": confirmation_sent
            })
        else:
            raise HTTPException(status_code=500, detail="Failed to update timer")
            
    except Exception as e:
        print(f"Error updating timer: {e}")
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
        print(f"Error muting timer: {e}")
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
        print(f"Error recording anomaly: {e}")
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/api/challenge")
async def get_scanning_challenge():
    """Get random scanning challenge prompt"""
    try:
        challenge = get_challenge()
        return JSONResponse({"challenge": challenge})
    except Exception as e:
        print(f"Error getting challenge: {e}")
        return JSONResponse({"challenge": "Scan your surroundings for anything unusual, unexpected, or meaningful..."})

@app.get("/health")
async def health_check():
    """Health check endpoint"""
    return JSONResponse({"status": "healthy", "message": "Chronomancy Updated UI Server"})

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
        print(f"Error creating checkout session: {e}")
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
    """Configure Telegram webhook on startup (Scott Wilber canonical setup)."""

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