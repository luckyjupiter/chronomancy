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
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles

# Add project root to path for imports
ROOT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT_DIR))

# Import backend functionality
bot_functions_available = False
try:
    sys.path.append(str(ROOT_DIR))
    from bot.main import get_user_timer_settings, set_user_timer, mute_user_timer, log_anomaly, get_challenge
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
            "muted_until": settings.get("muted_until")
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
            "muted_until": None
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
            # Send confirmation via Telegram bot
            try:
                from bot.main import bot as telegram_bot
                telegram_bot.send_message(
                    user_id,
                    f"✅ Scanner activated! I will ping you {timer_data['daily_count']} time(s) per day between {timer_data['window_start']} and {timer_data['window_end']}.")
            except Exception as e:
                print(f"⚠️  Could not send confirmation message: {e}")
            return JSONResponse({"status": "success", "message": "Timer updated"})
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

@app.on_event("startup")
async def startup_event():
    """Start background Telegram bot polling when API server starts"""
    try:
        import threading
        from bot import main as bot_main
        if not getattr(bot_main, "_api_thread_started", False):
            t = threading.Thread(target=bot_main.main, daemon=True)
            t.start()
            bot_main._api_thread_started = True
            print("🚀 Telegram bot polling thread started")
    except Exception as e:
        print(f"⚠️  Failed to start Telegram bot thread: {e}")

if __name__ == "__main__":
    print("🏵️ Starting Chronomancy Updated UI Server...")
    print("🎨 Scott Wilber's minimal activation energy framework")
    print("📍 Serving at http://localhost:5001")
    
    # Run server
    uvicorn.run(
        app,
        host="0.0.0.0",
        port=5001,
        log_level="info",
        access_log=True
    ) 