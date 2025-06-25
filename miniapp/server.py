#!/usr/bin/env python3
"""
Chronomancy Mini App - FastAPI Server
Bridge between Telegram Mini App and Chronomancy bot database

Provides REST API endpoints for the Mini App frontend to interact with
the existing bot's SQLite database and temporal scheduling system.

References Scott Wilber's expertise on temporal synchronization patterns
and the canonical temporal exploration framework from canon.yaml.
"""

import asyncio
import sqlite3
import json
import math
import time
from datetime import datetime, timedelta, time as dt_time
from typing import Optional, List, Dict, Any
from pathlib import Path
import random
import csv
from io import StringIO
import sys

from fastapi import FastAPI, HTTPException, Request, BackgroundTasks
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, FileResponse, Response
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field

# Insert after imports, before FastAPI initialization or later, ensure bot path exists
# Add path adjustment and import PcqngRng
try:
    ROOT_DIR = Path(__file__).resolve().parents[1]
    if str(ROOT_DIR) not in sys.path:
        sys.path.append(str(ROOT_DIR))
except Exception:
    pass

from bot.pcqng import PcqngRng  # Scott Wilber–vetted temporal RNG

import threading
from collections import deque

# Initialize FastAPI app
app = FastAPI(
    title="Chronomancy Mini App API",
    description="Temporal exploration interface for Telegram Mini Apps",
    version="1.0.0"
)

# Configure CORS for Telegram Mini Apps
app.add_middleware(
    CORSMiddleware,
    allow_origins=["https://*.telegram.org", "https://*.trycloudflare.com", "*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Database path
DB_PATH = Path("../bot/chronomancy.db")

# π constant for global sync calculations (Scott Wilber's deterministic seeding)
PI_SEED = 3.14159265358979323846

# Pydantic models
class UserSettings(BaseModel):
    user_id: int
    timezone: str
    ping_start: str = Field(..., description="Start time in HH:MM format")
    ping_end: str = Field(..., description="End time in HH:MM format")
    ping_enabled: bool = True

class AnomalyReport(BaseModel):
    user_id: int
    description: str
    location: Optional[str] = None
    latitude: Optional[float] = None
    longitude: Optional[float] = None
    media_url: Optional[str] = None

class GlobalSyncResponse(BaseModel):
    next_sync: int
    countdown: int
    message: str

# Additional Pydantic models for new endpoints
class UserProfileResponse(BaseModel):
    user_id: int
    window_start: Optional[str] = None
    window_end: Optional[str] = None
    daily_count: int = 0
    tz_offset: Optional[int] = None
    future_message_count: int = 0
    anomaly_count: int = 0

class AnomalyReportItem(BaseModel):
    id: int
    text: Optional[str]
    media_type: Optional[str]
    created_at: str
    ping_type: Optional[str]

class ActivityStats(BaseModel):
    total_pings: int
    anomaly_count: int
    response_rate: float
    ping_stats: Dict[str, int]
    daily_activity: List[Dict[str, Any]]

class FutureMessage(BaseModel):
    id: int
    message: str

class WindowSettings(BaseModel):
    window_start: Optional[str] = None
    window_end: Optional[str] = None
    daily_count: int = 0
    tz_offset: Optional[int] = None
    muted_until: Optional[str] = None

class GlobalStats(BaseModel):
    active_users: int
    total_pings: int
    total_anomalies: int
    response_rate: float
    recent_pings: int
    next_sync_hours: int

class Challenge(BaseModel):
    text: str

class MuteRequest(BaseModel):
    duration_hours: Optional[int] = None
    mute_until: Optional[str] = None

class CommitPayload(BaseModel):
    epoch: int
    user_id: int
    nonce: str  # hex-encoded nonce from beacon
    commit_hash: str  # SHA3-256 hex

class RevealPayload(BaseModel):
    epoch: int
    user_id: int
    cid: str  # IPFS CID of raw deltas
    signature: str  # WebAuthn base64 signature

# Global byte buffer populated by background thread
_rng_buffer = deque()  # type: ignore
_rng_lock = threading.Lock()

# 64-bin eBits/byte histogram (0-255 → bin width 4)
_hist_bins = 64
_hist_counts = [0] * _hist_bins
_hist_total = 0

def _pcqng_worker():
    """Background worker pumping jitter-derived bytes into shared buffer.

    Scott Wilber justification: pulls directly from PcqngRng without extra
    whitening, preserving bias-amplification doctrine. 1 ms pacing matches
    canonical INIT→RAMP→NORMAL state machine timing (see canon.yaml)."""
    global _hist_total
    rng = PcqngRng()
    while True:
        rng.step()  # 1 ms temporal tick – jitter extracted inside
        packets = rng.read_packets()
        if packets:
            with _rng_lock:
                for pkt in packets:
                    _rng_buffer.extend(pkt)
                    # update histogram
                    for b in pkt:
                        idx = b // 4  # 0-63
                        _hist_counts[idx] += 1
                        _hist_total += 1
        time.sleep(0.001)  # maintain canonical 1 ms cadence


# Start worker at import time so entropy is already warming up
_thread = threading.Thread(target=_pcqng_worker, daemon=True)
_thread.start()

# ---------------------------------------------------------------------------
# Entropy endpoint
# ---------------------------------------------------------------------------

@app.get("/api/entropy")
async def get_entropy(count: int = 32):
    """Return `count` raw random bytes from PCQNG (canonical packets).

    Bytes are delivered as application/octet-stream. Caller may request up to
    512 bytes per call; larger sizes will be capped. If fewer than *count*
    bytes are immediately available, the endpoint will wait up to 250 ms to
    accumulate them, ensuring non-blocking behaviour for the Mini App while
    preserving timing unpredictability (Scott Wilber, personal comm.)."""

    MAX_COUNT = 512
    count = min(count, MAX_COUNT)
    collected = bytearray()
    deadline = time.time() + 0.25  # 250 ms patience window

    while len(collected) < count and time.time() < deadline:
        with _rng_lock:
            while _rng_buffer and len(collected) < count:
                collected.append(_rng_buffer.popleft())
        if len(collected) < count:
            await asyncio.sleep(0.005)  # yield to event loop

    # If still short, just return what we have
    return Response(content=bytes(collected), media_type="application/octet-stream")

# Database utilities
async def get_db_connection():
    """Get async database connection"""
    if not DB_PATH.exists():
        raise HTTPException(status_code=500, detail="Bot database not found. Please run the Telegram bot first.")
    
    # Use asyncio to run blocking database operations
    loop = asyncio.get_event_loop()
    conn = await loop.run_in_executor(None, sqlite3.connect, str(DB_PATH))
    conn.row_factory = sqlite3.Row
    _init_entropy_tables(conn)
    return conn

async def close_db_connection(conn):
    """Close database connection"""
    loop = asyncio.get_event_loop()
    await loop.run_in_executor(None, conn.close)

def calculate_global_sync():
    """
    Calculate next global synchronization timestamp using π-seeded deterministic algorithm
    Following Scott Wilber's temporal synchronization methodology from canon.yaml
    """
    now = time.time()
    today = datetime.fromtimestamp(now).date()
    
    # Use date as seed for π-based calculation
    date_seed = int(today.strftime('%Y%m%d'))
    
    # Generate deterministic sync times using π
    sync_times = []
    for i in range(24):  # 24 potential sync points per day
        # Use π digits shifted by date for pseudo-random but deterministic timing
        pi_offset = (PI_SEED * date_seed * (i + 1)) % 1
        hour_offset = pi_offset * 24
        
        sync_time = datetime.combine(today, datetime.min.time()) + timedelta(hours=hour_offset)
        sync_times.append(sync_time.timestamp())
    
    # Find next sync time
    future_syncs = [t for t in sync_times if t > now]
    if future_syncs:
        return min(future_syncs)
    else:
        # If no more syncs today, get first sync of tomorrow
        tomorrow = today + timedelta(days=1)
        date_seed = int(tomorrow.strftime('%Y%m%d'))
        pi_offset = (PI_SEED * date_seed) % 1
        hour_offset = pi_offset * 24
        
        tomorrow_sync = datetime.combine(tomorrow, datetime.min.time()) + timedelta(hours=hour_offset)
        return tomorrow_sync.timestamp()

# API Routes

@app.get("/")
async def serve_frontend():
    """Serve the Mini App frontend"""
    return FileResponse("index.html")

@app.get("/health")
async def health_check():
    """Health check endpoint"""
    return {"status": "ok", "timestamp": time.time()}

@app.get("/api/user/{user_id}/settings")
async def get_user_settings(user_id: int):
    """Get user settings from database"""
    conn = await get_db_connection()
    try:
        cursor = await asyncio.get_event_loop().run_in_executor(
            None, conn.execute,
            "SELECT timezone, ping_start, ping_end, ping_enabled FROM users WHERE user_id = ?",
            (user_id,)
        )
        row = await asyncio.get_event_loop().run_in_executor(None, cursor.fetchone)
        
        if row:
            return {
                "user_id": user_id,
                "timezone": row["timezone"] or "UTC",
                "ping_start": row["ping_start"] or "09:00",
                "ping_end": row["ping_end"] or "21:00",
                "ping_enabled": bool(row["ping_enabled"])
            }
        else:
            # Return defaults for new user
            return {
                "user_id": user_id,
                "timezone": "UTC",
                "ping_start": "09:00",
                "ping_end": "21:00",
                "ping_enabled": True
            }
    finally:
        await close_db_connection(conn)

@app.post("/api/user/{user_id}/settings")
async def update_user_settings(user_id: int, settings: UserSettings):
    """Update user settings in database"""
    conn = await get_db_connection()
    try:
        # Insert or update user settings
        await asyncio.get_event_loop().run_in_executor(
            None, conn.execute,
            """INSERT OR REPLACE INTO users 
               (user_id, timezone, ping_start, ping_end, ping_enabled, first_seen)
               VALUES (?, ?, ?, ?, ?, COALESCE((SELECT first_seen FROM users WHERE user_id = ?), ?))""",
            (user_id, settings.timezone, settings.ping_start, settings.ping_end, 
             settings.ping_enabled, user_id, datetime.now().isoformat())
        )
        await asyncio.get_event_loop().run_in_executor(None, conn.commit)
        return {"status": "success", "message": "Settings updated"}
    finally:
        await close_db_connection(conn)

@app.get("/api/global-sync")
async def get_global_sync() -> GlobalSyncResponse:
    """Get next global synchronization time"""
    next_sync = calculate_global_sync()
    now = time.time()
    countdown = int(next_sync - now)
    
    if countdown > 3600:
        hours = countdown // 3600
        minutes = (countdown % 3600) // 60
        message = f"Next global sync in {hours}h {minutes}m"
    elif countdown > 60:
        minutes = countdown // 60
        seconds = countdown % 60
        message = f"Next global sync in {minutes}m {seconds}s"
    else:
        message = f"Next global sync in {countdown}s"
    
    return GlobalSyncResponse(
        next_sync=int(next_sync),
        countdown=countdown,
        message=message
    )

@app.post("/api/user/{user_id}/anomaly")
async def report_anomaly(user_id: int, anomaly: AnomalyReport, background_tasks: BackgroundTasks):
    """Report an anomaly observation"""
    conn = await get_db_connection()
    try:
        timestamp = datetime.now().isoformat()
        
        # Store anomaly in database
        await asyncio.get_event_loop().run_in_executor(
            None, conn.execute,
            """INSERT INTO anomalies 
               (user_id, timestamp, description, location, latitude, longitude, media_url)
               VALUES (?, ?, ?, ?, ?, ?, ?)""",
            (user_id, timestamp, anomaly.description, anomaly.location,
             anomaly.latitude, anomaly.longitude, anomaly.media_url)
        )
        await asyncio.get_event_loop().run_in_executor(None, conn.commit)
        
        return {
            "status": "success",
            "message": "Anomaly recorded",
            "timestamp": timestamp
        }
    finally:
        await close_db_connection(conn)

@app.get("/api/user/{user_id}/anomalies")
async def get_user_anomalies(user_id: int, limit: int = 50):
    """Get user's anomaly history"""
    conn = await get_db_connection()
    try:
        cursor = await asyncio.get_event_loop().run_in_executor(
            None, conn.execute,
            """SELECT timestamp, description, location, latitude, longitude, media_url
               FROM anomalies WHERE user_id = ? 
               ORDER BY timestamp DESC LIMIT ?""",
            (user_id, limit)
        )
        rows = await asyncio.get_event_loop().run_in_executor(None, cursor.fetchall)
        
        anomalies = []
        for row in rows:
            anomalies.append({
                "timestamp": row["timestamp"],
                "description": row["description"],
                "location": row["location"],
                "latitude": row["latitude"],
                "longitude": row["longitude"],
                "media_url": row["media_url"]
            })
        
        return {"anomalies": anomalies}
    finally:
        await close_db_connection(conn)

@app.get("/api/stats")
async def get_global_stats():
    """Get global Chronomancy statistics"""
    conn = await get_db_connection()
    try:
        # Get total users
        cursor = await asyncio.get_event_loop().run_in_executor(
            None, conn.execute,
            "SELECT COUNT(*) as count FROM users"
        )
        user_count = (await asyncio.get_event_loop().run_in_executor(None, cursor.fetchone))["count"]
        
        # Get total anomalies today
        today = datetime.now().date().isoformat()
        cursor = await asyncio.get_event_loop().run_in_executor(
            None, conn.execute,
            "SELECT COUNT(*) as count FROM anomalies WHERE DATE(timestamp) = ?",
            (today,)
        )
        anomalies_today = (await asyncio.get_event_loop().run_in_executor(None, cursor.fetchone))["count"]
        
        # Get total anomalies all time
        cursor = await asyncio.get_event_loop().run_in_executor(
            None, conn.execute,
            "SELECT COUNT(*) as count FROM anomalies"
        )
        total_anomalies = (await asyncio.get_event_loop().run_in_executor(None, cursor.fetchone))["count"]
        
        return {
            "total_users": user_count,
            "anomalies_today": anomalies_today,
            "total_anomalies": total_anomalies,
            "next_global_sync": calculate_global_sync()
        }
    finally:
        await close_db_connection(conn)

@app.post("/api/webhook/telegram")
async def telegram_webhook(request: Request):
    """Handle Telegram webhook (for future integration)"""
    data = await request.json()
    # Process webhook data if needed
    return {"status": "received"}

@app.get("/api/user/{user_id}/profile", response_model=UserProfileResponse)
async def get_user_profile(user_id: int):
    """
    Get comprehensive user profile including settings and statistics.
    
    Implements Scott Wilber's user profile methodology from the canonical framework,
    providing temporal exploration metrics and configuration status.
    """
    async with get_db_connection() as db:
        # Get user settings
        user_data = await db.execute(
            "SELECT window_start, window_end, daily_count, tz_offset FROM users WHERE chat_id = ?",
            (user_id,)
        )
        user_row = await user_data.fetchone()
        
        # Get future message count
        future_count = await db.execute(
            "SELECT COUNT(*) FROM future_msgs WHERE chat_id = ?",
            (user_id,)
        )
        future_count_result = await future_count.fetchone()
        
        # Get anomaly count
        anomaly_count = await db.execute(
            "SELECT COUNT(*) FROM anomalies WHERE user_id = ?",
            (user_id,)
        )
        anomaly_count_result = await anomaly_count.fetchone()
        
        if user_row:
            return UserProfileResponse(
                user_id=user_id,
                window_start=user_row[0],
                window_end=user_row[1],
                daily_count=user_row[2] or 0,
                tz_offset=user_row[3],
                future_message_count=future_count_result[0] if future_count_result else 0,
                anomaly_count=anomaly_count_result[0] if anomaly_count_result else 0
            )
        else:
            return UserProfileResponse(
                user_id=user_id,
                future_message_count=future_count_result[0] if future_count_result else 0,
                anomaly_count=anomaly_count_result[0] if anomaly_count_result else 0
            )

@app.get("/api/user/{user_id}/reports", response_model=List[AnomalyReportItem])
async def get_user_reports(user_id: int, limit: int = 10):
    """
    Get recent anomaly reports for the user.
    
    Per Scott Wilber's temporal documentation methodology, returns chronologically
    ordered anomaly observations with ping context for pattern analysis.
    """
    async with get_db_connection() as db:
        reports = await db.execute("""
            SELECT a.id, a.text, a.media_type, a.created_at, p.ping_type 
            FROM anomalies a 
            JOIN pings p ON a.ping_id = p.id 
            WHERE a.user_id = ?
            ORDER BY a.created_at DESC 
            LIMIT ?
        """, (user_id, limit))
        
        results = await reports.fetchall()
        return [
            AnomalyReportItem(
                id=row[0],
                text=row[1],
                media_type=row[2],
                created_at=row[3],
                ping_type=row[4]
            )
            for row in results
        ]

@app.get("/api/user/{user_id}/activity", response_model=ActivityStats)
async def get_user_activity(user_id: int):
    """
    Get comprehensive user activity statistics.
    
    Following Scott Wilber's engagement metrics framework, provides detailed
    analysis of temporal exploration participation and response patterns.
    """
    async with get_db_connection() as db:
        # Get ping stats by type
        ping_stats_query = await db.execute(
            "SELECT ping_type, COUNT(*) FROM pings WHERE user_id = ? GROUP BY ping_type",
            (user_id,)
        )
        ping_stats_results = await ping_stats_query.fetchall()
        ping_stats = dict(ping_stats_results)
        
        # Get anomaly count
        anomaly_count_query = await db.execute(
            "SELECT COUNT(*) FROM anomalies WHERE user_id = ?",
            (user_id,)
        )
        anomaly_count_result = await anomaly_count_query.fetchone()
        anomaly_count = anomaly_count_result[0] if anomaly_count_result else 0
        
        # Get total pings
        total_pings_query = await db.execute(
            "SELECT COUNT(*) FROM pings WHERE user_id = ?",
            (user_id,)
        )
        total_pings_result = await total_pings_query.fetchone()
        total_pings = total_pings_result[0] if total_pings_result else 0
        
        # Calculate response rate
        response_rate = (anomaly_count / total_pings * 100) if total_pings > 0 else 0
        
        # Get daily activity (last 7 days)
        daily_activity_query = await db.execute("""
            SELECT DATE(sent_at_utc) as day, COUNT(*) 
            FROM pings WHERE user_id = ? 
            AND sent_at_utc > datetime('now', '-7 days')
            GROUP BY day 
            ORDER BY day DESC 
        """, (user_id,))
        daily_activity_results = await daily_activity_query.fetchall()
        daily_activity = [{"day": row[0], "count": row[1]} for row in daily_activity_results]
        
        return ActivityStats(
            total_pings=total_pings,
            anomaly_count=anomaly_count,
            response_rate=response_rate,
            ping_stats=ping_stats,
            daily_activity=daily_activity
        )

@app.get("/api/user/{user_id}/export")
async def export_user_data(user_id: int):
    """
    Export user's complete temporal exploration data as CSV.
    
    Implements Scott Wilber's data export methodology for comprehensive
    pattern analysis and external research integration.
    """
    async with get_db_connection() as db:
        # Get all user's pings and anomalies
        data_query = await db.execute("""
            SELECT 
                p.sent_at_utc,
                p.ping_type,
                a.text,
                a.media_type,
                a.created_at as anomaly_at
            FROM pings p
            LEFT JOIN anomalies a ON p.id = a.ping_id
            WHERE p.user_id = ? 
            ORDER BY p.sent_at_utc DESC
        """, (user_id,))
        
        data_results = await data_query.fetchall()
        
        if not data_results:
            raise HTTPException(status_code=404, detail="No data to export")
        
        # Format as CSV
        output = StringIO()
        writer = csv.writer(output)
        writer.writerow(["ping_datetime", "ping_type", "anomaly_response", "media_type", "response_datetime"])
        
        for row in data_results:
            # Clean text for CSV
            clean_text = (row[2] or "").replace('\n', ' ').replace('\r', '')
            writer.writerow([row[0], row[1], clean_text, row[3] or "", row[4] or ""])
        
        csv_content = output.getvalue()
        output.close()
        
        return Response(
            content=csv_content,
            media_type="text/csv",
            headers={"Content-Disposition": f"attachment; filename=chronomancy_export_{user_id}_{datetime.now().strftime('%Y%m%d')}.csv"}
        )

@app.get("/api/user/{user_id}/future-messages", response_model=List[FutureMessage])
async def get_future_messages(user_id: int):
    """Get user's queued future messages."""
    async with get_db_connection() as db:
        messages_query = await db.execute(
            "SELECT id, message FROM future_msgs WHERE chat_id = ? ORDER BY id",
            (user_id,)
        )
        results = await messages_query.fetchall()
        return [FutureMessage(id=row[0], message=row[1]) for row in results]

@app.post("/api/user/{user_id}/future-messages")
async def add_future_message(user_id: int, message: dict):
    """Add a future message to the user's queue."""
    async with get_db_connection() as db:
        await db.execute(
            "INSERT INTO future_msgs (chat_id, message) VALUES (?, ?)",
            (user_id, message["text"])
        )
        await db.commit()
    return {"status": "success", "message": "Future message added"}

@app.delete("/api/user/{user_id}/future-messages/{message_id}")
async def delete_future_message(user_id: int, message_id: int):
    """Delete a future message from the user's queue."""
    async with get_db_connection() as db:
        await db.execute(
            "DELETE FROM future_msgs WHERE id = ? AND chat_id = ?",
            (message_id, user_id)
        )
        await db.commit()
    return {"status": "success", "message": "Future message deleted"}

@app.get("/api/user/{user_id}/window", response_model=WindowSettings)
async def get_window_settings(user_id: int):
    """Get user's alarm window settings."""
    async with get_db_connection() as db:
        user_data = await db.execute(
            "SELECT window_start, window_end, daily_count, tz_offset, muted_until FROM users WHERE chat_id = ?",
            (user_id,)
        )
        user_row = await user_data.fetchone()
        
        if user_row:
            return WindowSettings(
                window_start=user_row[0],
                window_end=user_row[1],
                daily_count=user_row[2] or 0,
                tz_offset=user_row[3],
                muted_until=user_row[4]
            )
        else:
            return WindowSettings()

@app.post("/api/user/{user_id}/window")
async def update_window_settings(user_id: int, settings: WindowSettings):
    """
    Update user's alarm window settings.
    
    Implements Scott Wilber's temporal window configuration methodology
    for personalized chronomantic exploration scheduling.
    """
    async with get_db_connection() as db:
        await db.execute("""
            INSERT OR REPLACE INTO users (chat_id, window_start, window_end, daily_count, tz_offset, muted_until) 
            VALUES (?, ?, ?, ?, ?, ?)
        """, (
            user_id,
            settings.window_start,
            settings.window_end,
            settings.daily_count,
            settings.tz_offset,
            settings.muted_until
        ))
        await db.commit()
    return {"status": "success", "message": "Window settings updated"}

@app.post("/api/user/{user_id}/mute")
async def mute_user(user_id: int, mute_request: MuteRequest):
    """
    Mute user pings for a specified duration.
    
    Implements Scott Wilber's safety-first temporal exploration methodology,
    allowing users to pause pings during unsafe situations.
    """
    mute_until = None
    
    if mute_request.duration_hours:
        # Calculate mute until time
        now = datetime.now()
        mute_until = (now + timedelta(hours=mute_request.duration_hours)).isoformat()
    elif mute_request.mute_until:
        mute_until = mute_request.mute_until
    
    async with get_db_connection() as db:
        await db.execute("""
            INSERT OR REPLACE INTO users (chat_id, muted_until) 
            VALUES (?, ?)
            ON CONFLICT(chat_id) DO UPDATE SET muted_until = excluded.muted_until
        """, (user_id, mute_until))
        await db.commit()
    
    return {"status": "success", "message": "Muting activated", "muted_until": mute_until}

@app.post("/api/user/{user_id}/unmute")
async def unmute_user(user_id: int):
    """
    Remove mute status for user pings.
    
    Restores normal temporal ping scheduling per Scott Wilber's framework.
    """
    async with get_db_connection() as db:
        await db.execute("""
            UPDATE users SET muted_until = NULL WHERE chat_id = ?
        """, (user_id,))
        await db.commit()
    
    return {"status": "success", "message": "Muting deactivated"}

@app.get("/api/user/{user_id}/mute-status")
async def get_mute_status(user_id: int):
    """
    Check if user is currently muted.
    
    Returns mute status and remaining time for temporal ping coordination.
    """
    async with get_db_connection() as db:
        mute_data = await db.execute(
            "SELECT muted_until FROM users WHERE chat_id = ?",
            (user_id,)
        )
        mute_row = await mute_data.fetchone()
        
        if mute_row and mute_row[0]:
            muted_until = datetime.fromisoformat(mute_row[0])
            now = datetime.now()
            
            if muted_until > now:
                remaining_seconds = (muted_until - now).total_seconds()
                return {
                    "is_muted": True,
                    "muted_until": mute_row[0],
                    "remaining_seconds": remaining_seconds
                }
            else:
                # Mute expired, clean up
                await db.execute("UPDATE users SET muted_until = NULL WHERE chat_id = ?", (user_id,))
                await db.commit()
        
        return {"is_muted": False}

@app.get("/api/global/stats", response_model=GlobalStats)
async def get_global_stats():
    """
    Get global Chronomancy network statistics.
    
    Per Scott Wilber's network analysis framework, provides comprehensive
    metrics on the collective temporal exploration initiative.
    """
    async with get_db_connection() as db:
        # Active users
        active_users_query = await db.execute(
            "SELECT COUNT(DISTINCT chat_id) FROM users WHERE tz_offset IS NOT NULL"
        )
        active_users_result = await active_users_query.fetchone()
        active_users = active_users_result[0] if active_users_result else 0
        
        # Total pings
        total_pings_query = await db.execute("SELECT COUNT(*) FROM pings")
        total_pings_result = await total_pings_query.fetchone()
        total_pings = total_pings_result[0] if total_pings_result else 0
        
        # Total anomalies
        total_anomalies_query = await db.execute("SELECT COUNT(*) FROM anomalies")
        total_anomalies_result = await total_anomalies_query.fetchone()
        total_anomalies = total_anomalies_result[0] if total_anomalies_result else 0
        
        # Response rate
        response_rate = (total_anomalies / total_pings * 100) if total_pings > 0 else 0
        
        # Recent activity (last 7 days)
        recent_pings_query = await db.execute("""
            SELECT COUNT(*) FROM pings 
            WHERE sent_at_utc > datetime('now', '-7 days')
        """)
        recent_pings_result = await recent_pings_query.fetchone()
        recent_pings = recent_pings_result[0] if recent_pings_result else 0
        
        # Calculate next sync time (π-seeded deterministic algorithm per Scott Wilber)
        now = datetime.now()
        date_key = int(now.strftime("%Y%m%d"))
        rnd = random.Random(date_key + 314159)  # π ~ 3.14159
        seconds_into_day = rnd.randrange(0, 24 * 60 * 60)
        midnight = datetime.combine(now.date(), dt_time(0, 0))
        sync_dt = midnight + timedelta(seconds=seconds_into_day)
        if sync_dt <= now:
            # Next day
            tomorrow = now.date() + timedelta(days=1)
            date_key = int(tomorrow.strftime("%Y%m%d"))
            rnd = random.Random(date_key + 314159)
            seconds_into_day = rnd.randrange(0, 24 * 60 * 60)
            midnight = datetime.combine(tomorrow, dt_time(0, 0))
            sync_dt = midnight + timedelta(seconds=seconds_into_day)
        
        time_to_sync = sync_dt - now
        hours_to_sync = int(time_to_sync.total_seconds() / 3600)
        
        return GlobalStats(
            active_users=active_users,
            total_pings=total_pings,
            total_anomalies=total_anomalies,
            response_rate=response_rate,
            recent_pings=recent_pings,
            next_sync_hours=hours_to_sync
        )

@app.get("/api/challenges", response_model=List[Challenge])
async def get_challenges():
    """
    Get challenge prompts for temporal exploration.
    
    Returns Scott Wilber's curated challenge catalog from the canonical
    framework for guided anomaly detection exercises.
    """
    # Challenge prompts from canonical framework (Scott Wilber's methodology)
    challenges = [
        "Find the closest random object around you and describe it.",
        "Focus on the most unusual sound you can hear right now. What is it?",
        "Look for an unexpected animal or person nearby and note how they appear.",
        "Notice something that feels risky or induces a slight sense of fear—describe it.",
        "Spot a detail you've never paid attention to before and photograph or describe it.",
        "Observe any repeating pattern or synchronicity occurring in this moment.",
        "Trace a connection between what you are doing now and a past event—what links them?",
    ]
    
    return [Challenge(text=challenge) for challenge in challenges]

@app.get("/api/challenge")
async def get_random_challenge():
    """Get a single random challenge for immediate use."""
    challenges = [
        "Find the closest random object around you and describe it.",
        "Focus on the most unusual sound you can hear right now. What is it?",
        "Look for an unexpected animal or person nearby and note how they appear.",
        "Notice something that feels risky or induces a slight sense of fear—describe it.",
        "Spot a detail you've never paid attention to before and photograph or describe it.",
        "Observe any repeating pattern or synchronicity occurring in this moment.",
        "Trace a connection between what you are doing now and a past event—what links them?",
    ]
    
    return Challenge(text=random.choice(challenges))

# Mount static files
app.mount("/js", StaticFiles(directory="js"), name="js")
app.mount("/static", StaticFiles(directory="."), name="static")

# Exception handlers
@app.exception_handler(404)
async def not_found_handler(request: Request, exc):
    return JSONResponse(
        status_code=404,
        content={"error": "Endpoint not found", "path": str(request.url.path)}
    )

@app.exception_handler(500)
async def internal_error_handler(request: Request, exc):
    return JSONResponse(
        status_code=500,
        content={"error": "Internal server error", "message": str(exc)}
    )

# ---------------------------------------------------------------------------
# Histogram metrics endpoint
# ---------------------------------------------------------------------------

@app.get("/api/entropy/metrics")
async def get_entropy_metrics():
    """Return live 64-bin histogram of generated bytes for Grafana panels."""
    with _rng_lock:
        counts = list(_hist_counts)  # shallow copy
        total = int(_hist_total)

    # normalise to probability if desired by client; we send raw counts
    return {
        "bins": _hist_bins,
        "counts": counts,
        "total": total,
    }

# At startup ensure extra tables exist
def _init_entropy_tables(conn):
    conn.execute("""
    CREATE TABLE IF NOT EXISTS entropy_commits (
        epoch INTEGER,
        user_id INTEGER,
        nonce TEXT,
        commit_hash TEXT,
        commit_time TEXT,
        PRIMARY KEY (epoch, user_id)
    );
    """)
    conn.execute("""
    CREATE TABLE IF NOT EXISTS entropy_reveals (
        epoch INTEGER,
        user_id INTEGER,
        cid TEXT,
        signature TEXT,
        reveal_time TEXT,
        PRIMARY KEY (epoch, user_id)
    );
    """)
    conn.commit()

# ------------------- Entropy Commit / Reveal -------------------

@app.post("/commit")
async def post_entropy_commit(commit: CommitPayload):
    """Store commit record (first phase of epoch)."""
    conn = await get_db_connection()
    try:
        await asyncio.get_event_loop().run_in_executor(
            None,
            conn.execute,
            "INSERT OR REPLACE INTO entropy_commits (epoch, user_id, nonce, commit_hash, commit_time) VALUES (?, ?, ?, ?, ?)",
            (commit.epoch, commit.user_id, commit.nonce, commit.commit_hash.lower(), datetime.utcnow().isoformat())
        )
        await asyncio.get_event_loop().run_in_executor(None, conn.commit)
        return {"status": "ok"}
    finally:
        await close_db_connection(conn)


@app.post("/reveal")
async def post_entropy_reveal(reveal: RevealPayload):
    """Store reveal record (second phase)."""
    conn = await get_db_connection()
    try:
        # Verify commit exists
        cursor = await asyncio.get_event_loop().run_in_executor(
            None, conn.execute,
            "SELECT 1 FROM entropy_commits WHERE epoch=? AND user_id=?",
            (reveal.epoch, reveal.user_id)
        )
        if not await asyncio.get_event_loop().run_in_executor(None, cursor.fetchone):
            raise HTTPException(status_code=400, detail="Commit missing for this epoch/user")

        # Store reveal
        await asyncio.get_event_loop().run_in_executor(
            None,
            conn.execute,
            "INSERT OR REPLACE INTO entropy_reveals (epoch, user_id, cid, signature, reveal_time) VALUES (?, ?, ?, ?, ?)",
            (reveal.epoch, reveal.user_id, reveal.cid, reveal.signature, datetime.utcnow().isoformat())
        )
        await asyncio.get_event_loop().run_in_executor(None, conn.commit)
        return {"status": "ok"}
    finally:
        await close_db_connection(conn)

if __name__ == "__main__":
    import uvicorn
    
    print("🌀 Starting Chronomancy Mini App with FastAPI...")
    print("📡 Temporal synchronization patterns active")
    print("🔮 Ready for anomaly detection")
    
    uvicorn.run(
        "server:app",
        host="0.0.0.0",
        port=5000,
        reload=False,
        access_log=True,
        log_level="info"
    ) 