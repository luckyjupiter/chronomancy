"""Chronomancy Telegram Prototype

Usage:
    1. Install dependencies:  `pip install -r requirements.txt`
    2. Create a `.env` file in project root with TELEGRAM_BOT_TOKEN=<your token>.
    3. Run: `python -m bot.main`

Features (MVP):
    /start          – Welcome & brief help
    /window s e n   – Set active alarm window (start & end in 24-h HH:MM) and
                      number of random alarms (n) for the day.
    /future msg     – Store a message to deliver at the next alarm.
    /poke           – Force-trigger the next alarm (dev/testing only).

Data is stored per-user in an in-memory dict; persistence is TODO.
"""
from __future__ import annotations

import datetime as dt
import os
import random
import sqlite3
import threading
import time
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Any

from dotenv import load_dotenv
import telebot
from telebot.types import (
    Message,
    InlineKeyboardMarkup,
    InlineKeyboardButton,
    CallbackQuery,
    ChatMemberUpdated,
    WebAppInfo,
    ReplyKeyboardMarkup,
    KeyboardButton,
)
import logging

# ---------------------------------------------------------------------------
# Environment & Bot setup
# ---------------------------------------------------------------------------

# Admin configuration
ADMIN_USER_IDS = {1880904790}  # @JoshuaLengfelder

# Try to load .env from multiple possible locations
load_dotenv()  # Current directory
load_dotenv(os.path.join(os.path.dirname(__file__), '..', 'updatedui', '.env'))  # updatedui directory
load_dotenv(os.path.join(os.path.dirname(__file__), '..', '.env'))  # Project root

TOKEN = os.getenv("TELEGRAM_BOT_TOKEN")
if not TOKEN:
    raise RuntimeError("TELEGRAM_BOT_TOKEN missing from environment; create .env file")

logging.basicConfig(
    level=os.getenv("LOG_LEVEL", "INFO"),
    format="%(asctime)s [%(levelname)s] %(message)s",
)
logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Configurable URLs
# ---------------------------------------------------------------------------

WEBAPP_URL = os.getenv("WEBAPP_URL")  # e.g. "https://api.chronomancy.app/"

# -------------------------------------
# Persistence (SQLite)
# -------------------------------------

DB_PATH = os.path.join(os.path.dirname(__file__), "chronomancy.db")


def init_db():
    # Allow connection reuse across threads (API + bot polling threads)
    con = sqlite3.connect(DB_PATH, check_same_thread=False)
    cur = con.cursor()
    cur.execute(
        """CREATE TABLE IF NOT EXISTS users (
                chat_id INTEGER PRIMARY KEY,
                window_start TEXT,
                window_end TEXT,
                daily_count INTEGER DEFAULT 0,
                tz_offset INTEGER DEFAULT NULL,
                muted_until TEXT DEFAULT NULL,
                is_backer INTEGER NOT NULL DEFAULT 0,
                donate_skip INTEGER NOT NULL DEFAULT 0,
                nft_id TEXT DEFAULT NULL
            )"""
    )
    # Ensure tz_offset exists even on older DBs
    try:
        cur.execute("ALTER TABLE users ADD COLUMN tz_offset INTEGER DEFAULT NULL")
    except sqlite3.OperationalError:
        pass  # column already exists
    
    # Ensure muted_until exists even on older DBs
    try:
        cur.execute("ALTER TABLE users ADD COLUMN muted_until TEXT DEFAULT NULL")
    except sqlite3.OperationalError:
        pass  # column already exists
    cur.execute(
        """CREATE TABLE IF NOT EXISTS future_msgs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                chat_id INTEGER,
                message TEXT
            )"""
    )
    cur.execute(
        """CREATE TABLE IF NOT EXISTS config (
                key TEXT PRIMARY KEY,
                value TEXT
            )"""
    )
    # Group membership
    cur.execute(
        """CREATE TABLE IF NOT EXISTS group_members (
                chat_id INTEGER,
                user_id INTEGER,
                PRIMARY KEY(chat_id, user_id)
            )"""
    )

    # Recorded pings (any type)
    cur.execute(
        """CREATE TABLE IF NOT EXISTS pings (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                chat_id INTEGER,
                user_id INTEGER,
                ping_type TEXT,
                sent_msg_id INTEGER,
                sent_at_utc TEXT
            )"""
    )

    # Anomaly responses
    cur.execute(
        """CREATE TABLE IF NOT EXISTS anomalies (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                ping_id INTEGER,
                user_id INTEGER,
                chat_id INTEGER,
                text TEXT,
                file_id TEXT,
                media_type TEXT,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP
            )"""
    )

    # Add is_backer column
    try:
        cur.execute("ALTER TABLE users ADD COLUMN is_backer INTEGER DEFAULT 0")
    except sqlite3.OperationalError:
        pass

    # Ensure NULLs -> 0 (older rows)
    cur.execute("UPDATE users SET is_backer = 0 WHERE is_backer IS NULL")

    # Add donate_skip column
    try:
        cur.execute("ALTER TABLE users ADD COLUMN donate_skip INTEGER DEFAULT 0")
    except sqlite3.OperationalError:
        pass

    cur.execute("UPDATE users SET donate_skip = 0 WHERE donate_skip IS NULL")

    # Add nft_id column for Lifetime PSI NFT mapping
    try:
        cur.execute("ALTER TABLE users ADD COLUMN nft_id TEXT DEFAULT NULL")
    except sqlite3.OperationalError:
        pass

    con.commit()
    return con


CONN = init_db()

def get_config(key: str) -> Optional[str]:
    cur = CONN.execute("SELECT value FROM config WHERE key=?", (key,))
    row = cur.fetchone()
    return row[0] if row else None


def set_config(key: str, value: str):
    with CONN:
        CONN.execute("INSERT OR REPLACE INTO config(key,value) VALUES (?,?)", (key, value))


bot = telebot.TeleBot(TOKEN, parse_mode="HTML")

# ---------------------------------------------------------------------------
# Data Structures (in-memory, per-run)
# ---------------------------------------------------------------------------

@dataclass
class UserConfig:
    chat_id: int
    window_start: Optional[dt.time] = None
    window_end: Optional[dt.time] = None
    daily_count: int = 0
    tz_offset: Optional[int] = None  # hours offset from UTC
    future_msgs: List[str] = field(default_factory=list)  # in-memory cache
    todays_alarms: List[dt.datetime] = field(default_factory=list)

    def schedule_alarms(self) -> None:
        """Generate today's alarm datetimes according to current settings."""
        if not (self.window_start and self.window_end and self.daily_count > 0):
            return
        offset = dt.timedelta(hours=self.tz_offset or 0)
        now_utc = dt.datetime.utcnow()
        local_now = now_utc + offset
        local_today = local_now.date()

        start_local = dt.datetime.combine(local_today, self.window_start)
        end_local = dt.datetime.combine(local_today, self.window_end)
        if end_local <= start_local:
            end_local += dt.timedelta(days=1)

        span_seconds = int((end_local - start_local).total_seconds())
        future_times: List[dt.datetime] = []
        attempts = 0
        # Generate random times, but ensure they are in the future. If a random
        # point falls in the past, bump it to the next day's same local time.
        while len(future_times) < self.daily_count and attempts < self.daily_count * 10:
            t_local = start_local + dt.timedelta(seconds=random.randint(0, span_seconds))
            t_utc = t_local - offset

            if t_utc <= now_utc:
                # Already passed today → schedule for next day
                t_local += dt.timedelta(days=1)
                t_utc = t_local - offset

            future_times.append(t_utc)
            attempts += 1

        future_times.sort()
        self.todays_alarms = future_times

        # persist settings
        with CONN:
            CONN.execute(
                "INSERT OR REPLACE INTO users(chat_id, window_start, window_end, daily_count, tz_offset) VALUES (?,?,?,?,?)",
                (
                    self.chat_id,
                    self.window_start.strftime("%H:%M") if self.window_start else None,
                    self.window_end.strftime("%H:%M") if self.window_end else None,
                    self.daily_count,
                    self.tz_offset,
                ),
            )


# Load users from DB at startup
def load_users() -> Dict[int, UserConfig]:
    cur = CONN.execute("SELECT chat_id, window_start, window_end, daily_count, tz_offset FROM users")
    users: Dict[int, UserConfig] = {}
    for chat_id, ws, we, count, tz in cur.fetchall():
        cfg = UserConfig(chat_id=chat_id)
        if ws and we:
            cfg.window_start = dt.datetime.strptime(ws, "%H:%M").time()
            cfg.window_end = dt.datetime.strptime(we, "%H:%M").time()
            cfg.daily_count = count or 0
            cfg.tz_offset = tz
            cfg.schedule_alarms()
        # load future msgs
        cur2 = CONN.execute("SELECT message FROM future_msgs WHERE chat_id=? ORDER BY id", (chat_id,))
        cfg.future_msgs = [r[0] for r in cur2.fetchall()]
        users[chat_id] = cfg
    return users


USERS: Dict[int, UserConfig] = load_users()

# Background polling thread checks every second
CHECK_INTERVAL = 1.0  # seconds

def alarm_loop():
    while True:
        now = dt.datetime.utcnow()
        for cfg in list(USERS.values()):
            # Deliver any alarms whose time has passed
            while cfg.todays_alarms and cfg.todays_alarms[0] <= now:
                cfg.todays_alarms.pop(0)
                deliver_alarm(cfg)
        time.sleep(CHECK_INTERVAL)

def deliver_alarm(cfg: UserConfig):
    msg_lines = ["⏰ <b>Chronomancy Ping!</b>"]
    if cfg.future_msgs:
        # Pop first future message (FIFO)
        user_note = cfg.future_msgs.pop(0)
        with CONN:
            # remove from DB
            CONN.execute(
                """DELETE FROM future_msgs
                    WHERE rowid = (
                        SELECT id FROM future_msgs
                        WHERE chat_id=? ORDER BY id LIMIT 1
                    )""",
                (cfg.chat_id,),
            )
        msg_lines.append(f"💌 <i>Message from your past-self:</i> {user_note}")
    else:
        msg_lines.append("Take a breath and observe something unusual around you.")
    # Always include a challenge prompt per canon
    msg_lines.append(f"🧩 <b>Challenge:</b> {get_challenge()}")
    send_ping(cfg.chat_id, "\n".join(msg_lines), ping_type="user", user_id=cfg.chat_id)

# ---------------------------------------------------------------------------
# Command Handlers
# ---------------------------------------------------------------------------

TZ_OFFSETS_COMMON = [-8, -5, -3, 0, 1, 3, 8]


def tz_keyboard():
    kb = InlineKeyboardMarkup(row_width=3)
    for off in TZ_OFFSETS_COMMON:
        label = f"UTC{off:+d}"
        kb.add(InlineKeyboardButton(label, callback_data=f"tz_{off}"))
    kb.add(InlineKeyboardButton("Other", callback_data="tz_other"))
    return kb


@bot.message_handler(commands=["start", "help"])
def handle_start(m: Message):
    cfg = USERS.setdefault(m.chat.id, UserConfig(chat_id=m.chat.id))

    # Always greet user personally
    first_name = (m.from_user.first_name or "there") if m.from_user else "there"

    # Mini-app button if URL configured
    webapp_keyboard = None
    if WEBAPP_URL:
        webapp_keyboard = ReplyKeyboardMarkup(
            [[KeyboardButton(text="🌀 Open Chronomancy App", web_app=WebAppInfo(url=WEBAPP_URL))]]
        )

    if cfg.tz_offset is None:
        # No timezone yet – encourage user to open the Mini App which will auto-detect it
        bot.reply_to(
            m,
            f"Hi, <b>{first_name}</b>! 👋\n\nWelcome to <b>Chronomancy</b>. Tap the button below to complete setup. "
            "We'll automatically detect your timezone and start sending pings.",
            reply_markup=webapp_keyboard,
        )
    else:
        # Create Mini App button
        bot.reply_to(
            m,
            f"""<b>Chronomancy is running, {first_name}!</b> 🌀

<b>🎯 Quick Access:</b>
👆 Tap the button above to open the full Chronomancy interface

<b>Bot Commands:</b>
/window - Configure alarm schedule
/profile - Your settings and stats  
/reports - Recent anomaly reports
/activity - Engagement analytics
/future - Store message for next alarm
/poke - Force trigger alarm (testing)

<b>Group Commands:</b>
/setgroup - Register group for sync pings
/groupstats - Group activity stats

<b>Data Commands:</b>
/export - Export your data (CSV)
/global - View network statistics

<b>Admin/Testing Commands:</b>
/testall - Send test ping to all active users
/schedule - View upcoming ping schedule

Reply to any ping message to log anomalies!""",
            reply_markup=webapp_keyboard,
        )


# Time-zone selection callbacks


@bot.callback_query_handler(func=lambda c: c.data and c.data.startswith("tz_"))
def cb_tz_select(call: CallbackQuery):
    data = call.data
    chat_id = call.message.chat.id
    if data == "tz_other":
        # Build extended keyboard
        ext_kb = InlineKeyboardMarkup(row_width=4)
        for off in range(-12, 15):
            ext_kb.add(InlineKeyboardButton(f"UTC{off:+d}", callback_data=f"tz_{off}"))
        bot.edit_message_text(
            "Select your precise UTC offset:",
            chat_id,
            call.message.message_id,
            reply_markup=ext_kb,
        )
        return

    try:
        offset = int(data.split("_", 1)[1])
    except ValueError:
        bot.answer_callback_query(call.id, text="Invalid offset")
        return

    cfg = USERS.setdefault(chat_id, UserConfig(chat_id=chat_id))
    cfg.tz_offset = offset
    # default schedule if none
    if cfg.window_start is None:
        cfg.window_start, cfg.window_end, cfg.daily_count = dt.time(8, 0), dt.time(22, 0), 3
    cfg.schedule_alarms()
    bot.answer_callback_query(call.id, text=f"Timezone set to UTC{offset:+d}")
    bot.edit_message_reply_markup(chat_id, call.message.message_id, reply_markup=None)
    
    # Create Mini App button for new users
    webapp_keyboard = None
    if WEBAPP_URL:
        webapp_keyboard = ReplyKeyboardMarkup(
            [[KeyboardButton(text="🌀 Open Chronomancy App", web_app=WebAppInfo(url=WEBAPP_URL))]]
        )
    
    bot.send_message(
        chat_id, 
        "✅ All set! I'll start sending pings.\n\n🎯 Tap the button above to access the full Chronomancy interface with advanced features!",
        reply_markup=webapp_keyboard
    )

@bot.message_handler(commands=["window"])
def handle_window(m: Message):
    """Interactive window setter with inline keyboards"""
    parts = m.text.split()
    if len(parts) == 4:
        # quick path remains
        cfg = USERS.setdefault(m.chat.id, UserConfig(chat_id=m.chat.id))
        try:
            _, start_str, end_str, count_str = parts
            start = dt.datetime.strptime(start_str, "%H:%M").time()
            end = dt.datetime.strptime(end_str, "%H:%M").time()
            count = int(count_str)
            if count <= 0:
                raise ValueError
        except Exception:
            bot.reply_to(m, "Invalid format. Use /window HH:MM HH:MM N or /window to open picker.")
            return
        cfg.window_start = start
        cfg.window_end = end
        cfg.daily_count = count
        cfg.schedule_alarms()
        bot.reply_to(m, f"Scheduled {count} alarms between {start_str} and {end_str} for today.")
        return

    # Inline keyboard picker
    kb = InlineKeyboardMarkup(row_width=2)
    kb.add(
        InlineKeyboardButton("Day (08–22)", callback_data="preset_day"),
        InlineKeyboardButton("Evening (18–02)", callback_data="preset_evening"),
        InlineKeyboardButton("Night (22–06)", callback_data="preset_night"),
        InlineKeyboardButton("Custom", callback_data="preset_custom"),
    )
    bot.send_message(m.chat.id, "Choose a time window preset or Custom:", reply_markup=kb)


# Store wizard temporary data keyed by chat
_WIZARD: Dict[int, Dict[str, Any]] = {}


# Callback handler for presets

@bot.callback_query_handler(func=lambda c: c.data and c.data.startswith("preset_"))
def cb_handle_preset(call: CallbackQuery):
    data_key = call.data
    chat_id = call.message.chat.id
    if data_key == "preset_day":
        start, end = dt.time(8, 0), dt.time(22, 0)
    elif data_key == "preset_evening":
        start, end = dt.time(18, 0), dt.time(2, 0)
    elif data_key == "preset_night":
        start, end = dt.time(22, 0), dt.time(6, 0)
    else:  # custom path triggers old wizard
        bot.answer_callback_query(call.id)
        msg = bot.send_message(chat_id, "Send <b>start time</b> HH:MM", parse_mode="HTML")
        bot.register_next_step_handler(msg, _window_step_start)
        return

    _WIZARD[chat_id] = {"start": start, "end": end}
    bot.answer_callback_query(call.id, text="Preset selected!")
    ask_count(chat_id)


def ask_count(chat_id: int):
    kb = InlineKeyboardMarkup(row_width=3)
    kb.add(*(InlineKeyboardButton(str(n), callback_data=f"count_{n}") for n in (1, 2, 3, 4, 5)))
    bot.send_message(chat_id, "How many pings per day?", reply_markup=kb)


@bot.callback_query_handler(func=lambda c: c.data and c.data.startswith("count_"))
def cb_handle_count(call: CallbackQuery):
    chat_id = call.message.chat.id
    try:
        n = int(call.data.split("_", 1)[1])
    except ValueError:
        bot.answer_callback_query(call.id, text="Invalid")
        return
    data = _WIZARD.pop(chat_id, {})
    if "start" not in data or "end" not in data:
        bot.answer_callback_query(call.id, text="Session expired. Run /window again.")
        return
    cfg = USERS.setdefault(chat_id, UserConfig(chat_id=chat_id))
    cfg.window_start = data["start"]
    cfg.window_end = data["end"]
    cfg.daily_count = n
    cfg.schedule_alarms()
    bot.answer_callback_query(call.id)
    bot.edit_message_reply_markup(chat_id, call.message.message_id, reply_markup=None)
    bot.send_message(chat_id, f"✅ Scheduled {n} alarms between {cfg.window_start.strftime('%H:%M')} and {cfg.window_end.strftime('%H:%M')} daily.")

@bot.message_handler(commands=["future"])
def handle_future(m: Message):
    cfg = USERS.setdefault(m.chat.id, UserConfig(chat_id=m.chat.id))
    msg = m.text.partition(" ")[2].strip()
    if not msg:
        bot.reply_to(m, "Usage: /future your message here")
        return
    cfg.future_msgs.append(msg)
    with CONN:
        CONN.execute("INSERT INTO future_msgs(chat_id, message) VALUES (?,?)", (cfg.chat_id, msg))
    bot.reply_to(m, "Saved! Your note will appear at the next alarm.")

@bot.message_handler(commands=["poke"])
def handle_poke(m: Message):
    cfg = USERS.setdefault(m.chat.id, UserConfig(chat_id=m.chat.id))
    deliver_alarm(cfg)

@bot.message_handler(commands=["testall"])
def handle_test_all(m: Message):
    """Send a test ping to all users with active timers (admin only)."""
    # Check if user is admin
    if m.from_user.id not in ADMIN_USER_IDS:
        bot.reply_to(m, "❌ This command is restricted to administrators.")
        return
    
    if m.chat.type != "private":
        bot.reply_to(m, "This command only works in private messages.")
        return
    
    # Get all users with active timer settings
    cur = CONN.execute("""
        SELECT chat_id FROM users 
        WHERE window_start IS NOT NULL 
        AND window_end IS NOT NULL 
        AND daily_count > 0
    """)
    active_users = [row[0] for row in cur.fetchall()]
    
    if not active_users:
        bot.reply_to(m, "No users with active timers found.")
        return
    
    # Send test ping to all active users
    sent_count = 0
    for user_id in active_users:
        try:
            test_msg = f"""🧪 <b>Test Ping!</b>

This is a global test to verify the Chronomancy system is working.

🎯 <b>Instructions:</b>
1. Look around your current environment
2. Notice anything unusual or anomalous  
3. Reply to this message with your observations

<b>Challenge:</b> {get_challenge()}

<i>This was a test - regular scheduled pings will continue as normal.</i>"""
            
            send_ping(user_id, test_msg, ping_type="test_global", user_id=user_id)
            sent_count += 1
        except Exception as e:
            logger.error(f"Failed to send test ping to user {user_id}: {e}")
    
    bot.reply_to(m, f"✅ Global test ping sent to {sent_count} users with active timers.")

@bot.message_handler(commands=["schedule"])
def handle_schedule(m: Message):
    """Show upcoming ping schedule for all active users (admin only)."""
    # Check if user is admin
    if m.from_user.id not in ADMIN_USER_IDS:
        bot.reply_to(m, "❌ This command is restricted to administrators.")
        return
        
    if m.chat.type != "private":
        bot.reply_to(m, "This command only works in private messages.")
        return
    
    lines = ["<b>📅 Upcoming Ping Schedule</b>\n"]
    
    now = dt.datetime.utcnow()
    user_schedules = []
    
    for user_id, cfg in USERS.items():
        if cfg.todays_alarms:
            # Get next alarm for this user
            next_alarms = [alarm for alarm in cfg.todays_alarms if alarm > now]
            if next_alarms:
                next_alarm = next_alarms[0]
                user_schedules.append((next_alarm, user_id, len(next_alarms)))
    
    if not user_schedules:
        bot.reply_to(m, "No upcoming pings scheduled. Users may need to set their timers or wait for next day's schedule.")
        return
    
    # Sort by next alarm time
    user_schedules.sort(key=lambda x: x[0])
    
    lines.append(f"Current time: {now.strftime('%Y-%m-%d %H:%M:%S')} UTC\n")
    
    for alarm_time, user_id, remaining_count in user_schedules[:10]:  # Show next 10
        time_diff = alarm_time - now
        hours = int(time_diff.total_seconds() // 3600)
        minutes = int((time_diff.total_seconds() % 3600) // 60)
        
        lines.append(f"User {user_id}: <code>{alarm_time.strftime('%H:%M:%S')}</code> (in {hours}h {minutes}m, {remaining_count} left today)")
    
    if len(user_schedules) > 10:
        lines.append(f"\n... and {len(user_schedules) - 10} more")
    
    bot.reply_to(m, "\n".join(lines))

@bot.message_handler(commands=["profile"])
def handle_profile(m: Message):
    cfg = USERS.setdefault(m.chat.id, UserConfig(chat_id=m.chat.id))
    lines = ["<b>Your Chronomancy Profile</b>"]
    if cfg.window_start and cfg.window_end:
        lines.append(f"Alarm window: {cfg.window_start.strftime('%H:%M')}–{cfg.window_end.strftime('%H:%M')} x{cfg.daily_count}")
    else:
        lines.append("Alarm window not set.")
    lines.append(f"Queued future messages: {len(cfg.future_msgs)}")
    
    # Add anomaly stats
    cur = CONN.execute("SELECT COUNT(*) FROM anomalies WHERE user_id=?", (m.from_user.id,))
    anomaly_count = cur.fetchone()[0]
    lines.append(f"Anomalies logged: {anomaly_count}")
    
    bot.reply_to(m, "\n".join(lines))


@bot.message_handler(commands=["reports"])
def handle_reports(m: Message):
    """Show recent anomaly reports for this user."""
    cur = CONN.execute("""
        SELECT a.text, a.media_type, a.created_at, p.ping_type 
        FROM anomalies a 
        JOIN pings p ON a.ping_id = p.id 
        WHERE a.user_id = ? AND a.chat_id = ?
        ORDER BY a.created_at DESC 
        LIMIT 10
    """, (m.from_user.id, m.chat.id))
    
    reports = cur.fetchall()
    if not reports:
        bot.reply_to(m, "No anomaly reports found. Reply to ping messages to log observations!")
        return
    
    lines = ["<b>📊 Your Recent Anomaly Reports</b>\n"]
    for text, media_type, created_at, ping_type in reports:
        timestamp = dt.datetime.fromisoformat(created_at).strftime("%m/%d %H:%M")
        media_info = f" [{media_type}]" if media_type else ""
        ping_info = f" ({ping_type})" if ping_type else ""
        lines.append(f"• {timestamp}{ping_info}{media_info}")
        if text:
            preview = text[:100] + "..." if len(text) > 100 else text
            lines.append(f"  <i>{preview}</i>")
        lines.append("")
    
    bot.reply_to(m, "\n".join(lines))


@bot.message_handler(commands=["activity"])  
def handle_activity(m: Message):
    """Show user activity overview."""
    user_id = m.from_user.id
    
    # Get ping stats
    cur = CONN.execute("SELECT ping_type, COUNT(*) FROM pings WHERE user_id=? GROUP BY ping_type", (user_id,))
    ping_stats = dict(cur.fetchall())
    
    # Get anomaly stats  
    cur = CONN.execute("SELECT COUNT(*) FROM anomalies WHERE user_id=?", (user_id,))
    anomaly_count = cur.fetchone()[0]
    
    # Get response rate
    cur = CONN.execute("SELECT COUNT(*) FROM pings WHERE user_id=?", (user_id,))
    total_pings = cur.fetchone()[0]
    response_rate = (anomaly_count / total_pings * 100) if total_pings > 0 else 0
    
    # Get recent activity
    cur = CONN.execute("""
        SELECT DATE(sent_at_utc) as day, COUNT(*) 
        FROM pings WHERE user_id=? 
        GROUP BY day 
        ORDER BY day DESC 
        LIMIT 7
    """, (user_id,))
    daily_activity = cur.fetchall()
    
    lines = ["<b>📈 Your Chronomancy Activity</b>\n"]
    lines.append(f"Total pings received: {total_pings}")
    lines.append(f"Anomalies logged: {anomaly_count}")
    lines.append(f"Response rate: {response_rate:.1f}%\n")
    
    if ping_stats:
        lines.append("<b>Ping types:</b>")
        for ping_type, count in ping_stats.items():
            lines.append(f"• {ping_type}: {count}")
        lines.append("")
    
    if daily_activity:
        lines.append("<b>Recent daily activity:</b>")
        for day, count in daily_activity:
            lines.append(f"• {day}: {count} pings")
    
    bot.reply_to(m, "\n".join(lines))


@bot.message_handler(commands=["groupstats"])
def handle_groupstats(m: Message):
    """Show group statistics (only works in groups)."""
    if not m.chat.type in ("group", "supergroup"):
        bot.reply_to(m, "This command only works in group chats.")
        return
    
    chat_id = m.chat.id
    
    # Get member count
    cur = CONN.execute("SELECT COUNT(DISTINCT user_id) FROM group_members WHERE chat_id=?", (chat_id,))
    member_count = cur.fetchone()[0]
    
    # Get total group pings
    cur = CONN.execute("SELECT COUNT(*) FROM pings WHERE chat_id=? AND ping_type LIKE '%group%'", (chat_id,))
    group_pings = cur.fetchone()[0]
    
    # Get total anomalies in this group
    cur = CONN.execute("SELECT COUNT(*) FROM anomalies WHERE chat_id=?", (chat_id,))
    group_anomalies = cur.fetchone()[0]
    
    # Get most active contributors
    cur = CONN.execute("""
        SELECT user_id, COUNT(*) as anomaly_count 
        FROM anomalies 
        WHERE chat_id=? 
        GROUP BY user_id 
        ORDER BY anomaly_count DESC 
        LIMIT 5
    """, (chat_id,))
    top_contributors = cur.fetchall()
    
    lines = ["<b>🏘️ Group Chronomancy Stats</b>\n"]
    lines.append(f"Members tracked: {member_count}")
    lines.append(f"Group pings sent: {group_pings}")
    lines.append(f"Anomalies logged: {group_anomalies}")
    
    if group_anomalies > 0:
        response_rate = (group_anomalies / group_pings * 100) if group_pings > 0 else 0
        lines.append(f"Group response rate: {response_rate:.1f}%\n")
    
    if top_contributors:
        lines.append("<b>Top anomaly reporters:</b>")
        for user_id, count in top_contributors:
            try:
                member = bot.get_chat_member(chat_id, user_id)
                name = member.user.first_name or f"User {user_id}"
            except:
                name = f"User {user_id}"
            lines.append(f"• {name}: {count} reports")
    
    bot.reply_to(m, "\n".join(lines))


@bot.message_handler(commands=["export"])
def handle_export(m: Message):
    """Export user's data as CSV format."""
    user_id = m.from_user.id
    chat_id = m.chat.id
    
    # Get all user's pings and anomalies
    cur = CONN.execute("""
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
    
    data = cur.fetchall()
    if not data:
        bot.reply_to(m, "No data to export yet. Wait for some pings first!")
        return
    
    # Format as CSV
    lines = ["ping_datetime,ping_type,anomaly_response,media_type,response_datetime"]
    for ping_time, ping_type, anomaly_text, media_type, anomaly_time in data:
        # Clean text for CSV (escape quotes, remove newlines)
        clean_text = (anomaly_text or "").replace('"', '""').replace('\n', ' ').replace('\r', '')
        lines.append(f'"{ping_time}","{ping_type}","{clean_text}","{media_type or ""}","{anomaly_time or ""}"')
    
    csv_content = "\n".join(lines)
    
    # Send as document
    from io import BytesIO
    csv_file = BytesIO(csv_content.encode('utf-8'))
    csv_file.name = f"chronomancy_export_{user_id}_{dt.datetime.now().strftime('%Y%m%d')}.csv"
    
    try:
        bot.send_document(
            chat_id,
            csv_file,
            caption=f"🗂️ Your Chronomancy data export\n\nTotal records: {len(data)}\nGenerated: {dt.datetime.now().strftime('%Y-%m-%d %H:%M UTC')}"
        )
    except Exception as e:
        logger.error(f"Failed to send export file: {e}")
        # Fallback: send as text if file is small
        if len(csv_content) < 4000:
            bot.reply_to(m, f"<pre>{csv_content}</pre>")
        else:
            bot.reply_to(m, "Export file too large to send directly. Please contact admin.")


@bot.message_handler(commands=["global"])
def handle_global_stats(m: Message):
    """Show global Chronomancy network statistics."""
    # Total users
    cur = CONN.execute("SELECT COUNT(DISTINCT chat_id) FROM users WHERE tz_offset IS NOT NULL")
    active_users = cur.fetchone()[0]
    
    # Total pings sent
    cur = CONN.execute("SELECT COUNT(*) FROM pings")
    total_pings = cur.fetchone()[0]
    
    # Total anomalies logged
    cur = CONN.execute("SELECT COUNT(*) FROM anomalies")
    total_anomalies = cur.fetchone()[0]
    
    # Global response rate
    response_rate = (total_anomalies / total_pings * 100) if total_pings > 0 else 0
    
    # Active groups
    cur = CONN.execute("SELECT COUNT(DISTINCT chat_id) FROM group_members")
    active_groups = cur.fetchone()[0]
    
    # Recent activity (last 7 days)
    cur = CONN.execute("""
        SELECT COUNT(*) FROM pings 
        WHERE sent_at_utc > datetime('now', '-7 days')
    """)
    recent_pings = cur.fetchone()[0]
    
    # Today's sync ping status
    now = dt.datetime.now()
    next_sync = next_sync_time(now)
    time_to_sync = next_sync - now
    hours_to_sync = int(time_to_sync.total_seconds() / 3600)
    
    lines = ["<b>🌐 Global Chronomancy Network</b>\n"]
    lines.append(f"Active explorers: {active_users}")
    lines.append(f"Connected groups: {active_groups}")
    lines.append(f"Total pings sent: {total_pings:,}")
    lines.append(f"Anomalies logged: {total_anomalies:,}")
    lines.append(f"Network response rate: {response_rate:.1f}%")
    lines.append(f"Activity (7 days): {recent_pings} pings\n")
    lines.append(f"⏰ Next global sync: ~{hours_to_sync}h")
    lines.append("\n<i>Join the temporal exploration - reply to any ping to contribute!</i>")
    
    bot.reply_to(m, "\n".join(lines))


# Group registration for daily synchronous pings

@bot.message_handler(commands=["setgroup"])
def handle_setgroup(m: Message):
    if not m.chat.type in ("group", "supergroup"):
        bot.reply_to(m, "Run /setgroup inside the target group chat.")
        return
    set_config("group_chat_id", str(m.chat.id))
    bot.reply_to(m, "This group registered for daily synchronous Chronomancy ping.")


# Daily synchronous ping scheduling

def next_sync_time(now: dt.datetime) -> dt.datetime:
    """Return next sync ping datetime after `now` (local timezone)."""
    # Seed RNG with date + pi digits (simplified deterministic)
    date_key = int(now.strftime("%Y%m%d"))
    rnd = random.Random(date_key + 314159)  # π ~ 3.14159
    seconds_into_day = rnd.randrange(0, 24 * 60 * 60)
    midnight = dt.datetime.combine(now.date(), dt.time(0, 0))
    sync_dt = midnight + dt.timedelta(seconds=seconds_into_day)
    if sync_dt <= now:
        # compute for next day
        return next_sync_time(now + dt.timedelta(days=1))
    return sync_dt


def sync_loop():
    scheduled = next_sync_time(dt.datetime.now())
    while True:
        now = dt.datetime.now()
        if now >= scheduled:
            deliver_sync_ping()
            scheduled = next_sync_time(now + dt.timedelta(seconds=1))
        time.sleep(5)


def deliver_sync_ping():
    group_id = get_config("group_chat_id")
    text = (
        "🌐 <b>Global Chronomancy Sync Ping!</b>\n"
        "Pause and observe the moment—then share what you notice.\n\n"
        f"🧩 <b>Challenge:</b> {get_challenge()}"
    )
    for cfg in USERS.values():
        send_ping(cfg.chat_id, text, ping_type="global_user", user_id=cfg.chat_id)
    if group_id:
        send_ping(int(group_id), text, ping_type="global_group")

# ---------------------------------------------------------------------------
# Challenge Prompt Catalog (from canon)
# ---------------------------------------------------------------------------

CHALLENGES = [
    "Find the closest random object around you and describe it.",
    "Focus on the most unusual sound you can hear right now.  What is it?",
    "Look for an unexpected animal or person nearby and note how they appear.",
    "Notice something that feels risky or induces a slight sense of fear—describe it.",
    "Spot a detail you've never paid attention to before and photograph or describe it.",
    "Observe any repeating pattern or synchronicity occurring in this moment.",
    "Trace a connection between what you are doing now and a past event—what links them?",
]

def get_challenge() -> str:
    return random.choice(CHALLENGES)

# ---------------------------------------------------------------------------
# Ping cache: maps (chat_id, msg_id) -> ping_id for quick anomaly linking
# Keep it small (latest 1000 entries)
# ---------------------------------------------------------------------------

from collections import OrderedDict

PING_CACHE: "OrderedDict[tuple[int,int], int]" = OrderedDict()

def _cache_ping(chat_id: int, msg_id: int, ping_id: int):
    PING_CACHE[(chat_id, msg_id)] = ping_id
    # trim cache
    if len(PING_CACHE) > 1000:
        PING_CACHE.popitem(last=False)


# ---------------------------------------------------------------------------
# Helper to send a ping and log it
# ---------------------------------------------------------------------------

def send_ping(chat_id: int, text: str, ping_type: str, user_id: Optional[int] = None) -> None:
    """Send a ping message and record it in the database.

    According to Scott Wilber's methodology, pings should provide immediate
    access to anomaly reporting interfaces for optimal temporal synchronization.

    NOTE:  A single global SQLite connection object (`CONN`) caused *thread-affinity*
    errors once the alarm loop (background thread) and the FastAPI thread both
    started sending pings.  Even with `check_same_thread=False` SQLite will still
    complain if the connection is *used* concurrently by two threads.  To keep
    things rock-solid we now open a **fresh connection per call**.  The overhead
    is negligible at a handful of pings per day, and we avoid the race entirely
    (Scott Wilber recommends isolated handles for each async execution context
    anyway).
    """
    try:
        # Create Mini App button for quick access to anomaly reporting
        webapp_keyboard = None
        if WEBAPP_URL:
            webapp_keyboard = ReplyKeyboardMarkup(
                [[KeyboardButton(text="📝 Report Anomaly", web_app=WebAppInfo(url=f"{WEBAPP_URL}?mode=anomaly"))]]
            )
        
        sent_msg = bot.send_message(chat_id, text, reply_markup=webapp_keyboard)
        
        # Record in database (thread-local connection)
        with sqlite3.connect(DB_PATH, check_same_thread=False) as _con:
            cur = _con.execute(
                "INSERT INTO pings(chat_id, user_id, ping_type, sent_msg_id, sent_at_utc) VALUES (?,?,?,?,?)",
                (chat_id, user_id or chat_id, ping_type, sent_msg.message_id, dt.datetime.utcnow().isoformat())
            )
            ping_id = cur.lastrowid
            _cache_ping(chat_id, sent_msg.message_id, ping_id)

        logger.info(f"Ping sent to {chat_id} (msg_id={sent_msg.message_id})")

        # Donation prompt every 5th ping
        if not is_backer(chat_id):
            cur = CONN.execute(
                "SELECT COUNT(*) FROM pings WHERE chat_id=?", (chat_id,)
            )
            total_pings = cur.fetchone()[0] or 0
            if total_pings % 5 == 0:
                show_donation_footer(chat_id)
    except Exception as e:
        logger.error(f"Failed to send ping to {chat_id}: {e}")
        # Fallback without keyboard
        try:
            sent_msg = bot.send_message(chat_id, text)
            with sqlite3.connect(DB_PATH, check_same_thread=False) as _con:
                cur = _con.execute(
                    "INSERT INTO pings(chat_id, user_id, ping_type, sent_msg_id, sent_at_utc) VALUES (?,?,?,?,?)",
                    (chat_id, user_id or chat_id, ping_type, sent_msg.message_id, dt.datetime.utcnow().isoformat())
                )
                ping_id = cur.lastrowid
                _cache_ping(chat_id, sent_msg.message_id, ping_id)
        except Exception as e2:
            logger.error(f"Failed to send fallback ping to {chat_id}: {e2}")

# ---------------------------------------------------------------------------
# Utility to track group membership
# ---------------------------------------------------------------------------

def track_member(chat_id: int, user_id: int):
    with CONN:
        CONN.execute(
            "INSERT OR IGNORE INTO group_members(chat_id, user_id) VALUES (?,?)",
            (chat_id, user_id),
        )

# ---------------------------------------------------------------------------
# Reply handler for anomaly capture and membership tracking
# ---------------------------------------------------------------------------


@bot.message_handler(content_types=["text", "photo", "audio", "voice", "document", "video", "sticker"])
def handle_any_message(m: Message):
    # Track membership if in group
    if m.chat.type in ("group", "supergroup"):
        track_member(m.chat.id, m.from_user.id)

    if not m.reply_to_message:
        return  # not an anomaly response

    key = (m.chat.id, m.reply_to_message.message_id)
    ping_id = PING_CACHE.get(key)
    if not ping_id:
        return  # reply not to a ping we know

    media_type = None
    file_id = None
    if m.content_type == "text":
        txt = m.text
    else:
        txt = m.caption or ""
        file_id = getattr(m, m.content_type).file_id if hasattr(m, m.content_type) else None
        media_type = m.content_type

    with CONN:
        CONN.execute(
            "INSERT INTO anomalies(ping_id, user_id, chat_id, text, file_id, media_type) VALUES (?,?,?,?,?,?)",
            (
                ping_id,
                m.from_user.id,
                m.chat.id,
                txt,
                file_id,
                media_type,
            ),
        )
    bot.reply_to(m, "🌀 Anomaly logged. Thanks!")

# ---------------------------------------------------------------------------
# Main entry
# ---------------------------------------------------------------------------

def run_polling():
    """Start alarm/sync threads then enter long-polling loop.

    Scott Wilber guidance: keep the public webhook and long-poll endpoints mutually
    exclusive.  Therefore we explicitly *remove* any webhook before starting
    `infinity_polling()` to avoid Telegram 409 errors when a previous webhook is
    still set.
    """

    # Ensure this process owns the getUpdates queue
    try:
        bot.remove_webhook()
    except Exception:
        pass

    threading.Thread(target=alarm_loop, daemon=True).start()
    threading.Thread(target=sync_loop, daemon=True).start()

    logger.info("Chronomancy Telegram bot polling…")

    # Robust polling loop with auto-restart
    while True:
        try:
            bot.infinity_polling(skip_pending=True, timeout=30, long_polling_timeout=40)
        except Exception as exc:
            logger.error("Bot polling crashed: %s", exc, exc_info=True)
            time.sleep(5)  # brief back-off then restart

# ---------------------------------------------------------------------------
# Helper functions for external access (Mini App server integration)
# ---------------------------------------------------------------------------

def get_user_timer_settings(user_id: int) -> dict:
    """Get timer settings for a user (for Mini App API)"""
    try:
        cur = CONN.execute(
            "SELECT window_start, window_end, daily_count, tz_offset, muted_until, is_backer, donate_skip FROM users WHERE chat_id=?", 
            (user_id,)
        )
        row = cur.fetchone()
        if row:
            ws, we, count, tz, muted_until, is_backer, donate_skip = row
            
            # Check if muted
            is_muted = False
            if muted_until:
                try:
                    muted_dt = dt.datetime.fromisoformat(muted_until)
                    is_muted = dt.datetime.utcnow() < muted_dt
                except:
                    pass
            
            backer_row = CONN.execute("SELECT is_backer FROM users WHERE chat_id=?", (user_id,)).fetchone()
            is_backer_flag = bool(backer_row and backer_row[0])

            return {
                "active": bool(ws and we and count and count > 0),
                "window_start": ws,
                "window_end": we,
                "daily_count": count or 0,
                "tz_offset": tz,
                "is_muted": is_muted,
                "muted_until": muted_until,
                "is_backer": is_backer_flag,
                "donate_skip": donate_skip
            }
        else:
            return {
                "active": False,
                "window_start": None,
                "window_end": None,
                "daily_count": 0,
                "tz_offset": None,
                "is_muted": False,
                "muted_until": None,
                "is_backer": False,
                "donate_skip": 0
            }
    except Exception as e:
        logger.error(f"Error getting user timer settings: {e}")
        return {"active": False, "window_start": None, "window_end": None, "daily_count": 0, "tz_offset": None, "is_muted": False, "muted_until": None, "is_backer": False, "donate_skip": 0}

def set_user_timer(user_id: int, window_start: str, window_end: str, daily_count: int, tz_offset: int = 0) -> bool:
    """Set timer settings for a user (for Mini App API)"""
    try:
        # Validate times
        start_time = dt.datetime.strptime(window_start, "%H:%M").time()
        end_time = dt.datetime.strptime(window_end, "%H:%M").time()
        
        # Update database
        with CONN:
            CONN.execute(
                "INSERT OR REPLACE INTO users(chat_id, window_start, window_end, daily_count, tz_offset) VALUES (?,?,?,?,?)",
                (user_id, window_start, window_end, daily_count, tz_offset)
            )
        
        # Update in-memory config if user exists
        if user_id in USERS:
            cfg = USERS[user_id]
            cfg.window_start = start_time
            cfg.window_end = end_time
            cfg.daily_count = daily_count
            cfg.tz_offset = tz_offset
            cfg.schedule_alarms()
        else:
            # Create new user config
            cfg = UserConfig(
                chat_id=user_id,
                window_start=start_time,
                window_end=end_time,
                daily_count=daily_count,
                tz_offset=tz_offset
            )
            cfg.schedule_alarms()
            USERS[user_id] = cfg
        
        logger.info(f"Timer set for user {user_id}: {window_start}-{window_end}, {daily_count} pings/day")
        return True
        
    except Exception as e:
        logger.error(f"Error setting user timer: {e}")
        return False

def mute_user_timer(user_id: int, hours: int = 24) -> bool:
    """Mute user timer for specified hours (for Mini App API)"""
    try:
        muted_until = (dt.datetime.utcnow() + dt.timedelta(hours=hours)).isoformat()
        
        with CONN:
            CONN.execute(
                "UPDATE users SET muted_until=? WHERE chat_id=?",
                (muted_until, user_id)
            )
        
        logger.info(f"User {user_id} muted for {hours} hours")
        return True
        
    except Exception as e:
        logger.error(f"Error muting user timer: {e}")
        return False

def log_anomaly(user_id: int, description: str) -> bool:
    """Log anomaly observation (for Mini App API)"""
    try:
        with CONN:
            CONN.execute(
                "INSERT INTO anomalies(ping_id, user_id, chat_id, text, file_id, media_type) VALUES (?,?,?,?,?,?)",
                (None, user_id, user_id, description, None, None)
            )
        
        logger.info(f"Anomaly logged for user {user_id}")
        return True
        
    except Exception as e:
        logger.error(f"Error logging anomaly: {e}")
        return False

# Expose database path for external access
db_path = DB_PATH

# ---------------------------------------------------------------------------
# Donation / Backer helpers (Scott Wilber 2025-06-27)
# ---------------------------------------------------------------------------

TON_WALLET = "UQCsOuQxtjxWHJ4U8D4G8CGo9DGtpp5MPvkJLXIegibVie2I"


def is_backer(chat_id: int) -> bool:
    row = CONN.execute("SELECT is_backer FROM users WHERE chat_id=?", (chat_id,)).fetchone()
    return bool(row and row[0])


def inc_donate_skip(chat_id: int) -> None:
    with CONN:
        CONN.execute(
            "UPDATE users SET donate_skip = COALESCE(donate_skip,0) + 1 WHERE chat_id=?",
            (chat_id,),
        )


def donate_skips(chat_id: int) -> int:
    row = CONN.execute("SELECT donate_skip FROM users WHERE chat_id=?", (chat_id,)).fetchone()
    return row[0] if row else 0


def show_donation_footer(chat_id: int):
    """Prompt the user with a TON donation footer inline-keyboard."""
    if is_backer(chat_id):
        return
    if donate_skips(chat_id) >= 3:
        return

    url = (
        f"ton://transfer/{TON_WALLET}?amount=5000000000&text={chat_id}"
    )

    kb = InlineKeyboardMarkup(row_width=2)
    kb.row(InlineKeyboardButton("💎 Support with TON", url=url), InlineKeyboardButton("Skip", callback_data="skip_donate"))

    bot.send_message(
        chat_id,
        "✨ Liked that ping? Fuel the Beacon\nDonate ≥5 TON once for lifetime premium (Lifetime PSI Pass).\n🔒 Do **not** edit the 'comment' field; it links your payment to your account.",
        reply_markup=kb,
    )

# ---------------------------------------------------------------------------
# Callback handlers
# ---------------------------------------------------------------------------

@bot.callback_query_handler(func=lambda c: c.data == "skip_donate")
def cb_skip_donate(call: CallbackQuery):
    inc_donate_skip(call.message.chat.id)
    bot.answer_callback_query(call.id, "Got it – no problem!")
    bot.edit_message_reply_markup(
        chat_id=call.message.chat.id,
        message_id=call.message.message_id,
        reply_markup=None,
    )

# ---------------------------------------------------------------------------
# Support / Lifetime pass command
# ---------------------------------------------------------------------------

@bot.message_handler(commands=["support", "pass"])
def handle_support(m: Message):
    chat_id = m.chat.id
    if is_backer(chat_id):
        bot.reply_to(m, "🔑 You already own a Lifetime PSI Pass — thank you!")
        return

    url = f"ton://transfer/{TON_WALLET}?amount=5000000000&text={chat_id}"
    kb = InlineKeyboardMarkup()
    kb.add(InlineKeyboardButton("💎 Buy Lifetime PSI Pass", url=url))
    bot.reply_to(
        m,
        "Lifetime PSI Pass — unlock unlimited premium entropy forever (one-time ≥5 TON, never pay again).",
        reply_markup=kb,
    )

# ---------------------------------------------------------------------------
# Backer minting stub (would run in payment monitor)
# ---------------------------------------------------------------------------

def mark_backer(chat_id: int, nft_id: str):
    """Mark user as backer and store NFT id (stub)."""
    with CONN:
        CONN.execute(
            "UPDATE users SET is_backer=1, nft_id=? WHERE chat_id=?",
            (nft_id, chat_id),
        )

# Backward-compat CLI entry-point
def main():
    run_polling()

if __name__ == "__main__":
    main() 