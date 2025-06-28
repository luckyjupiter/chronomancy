# 🚀 Chronomancy Scaling & Hardening Plan

## Executive Summary
Transform the current Chronomancy deployment into a rock-solid, scalable system capable of handling thousands of users with 99.9% uptime. This plan addresses database optimization, connection pooling, resource management, monitoring, and auto-recovery mechanisms.

## 🎯 Performance Targets
- **Users**: 10,000+ concurrent users
- **Uptime**: 99.9% (8.77 hours downtime/year)
- **Response Time**: <200ms API responses
- **Memory**: Stable memory usage with no leaks
- **Database**: Query performance <50ms average

## 🔧 Phase 1: Database & Connection Hardening

### 1.1 SQLite → PostgreSQL Migration
**Priority: HIGH**
- Replace SQLite with PostgreSQL for production scalability
- Implement proper connection pooling with asyncpg
- Add database indices for frequently queried columns
- Enable WAL mode and optimize VACUUM schedules

### 1.2 Connection Pool Management
```python
# Implement asyncpg connection pool
import asyncpg
from asyncpg import Pool

class DatabaseManager:
    def __init__(self):
        self.pool: Optional[Pool] = None
    
    async def initialize(self):
        self.pool = await asyncpg.create_pool(
            dsn=DATABASE_URL,
            min_size=10,
            max_size=100,
            command_timeout=60,
            server_settings={
                'jit': 'off',
                'application_name': 'chronomancy_bot'
            }
        )
    
    async def execute(self, query: str, *args):
        async with self.pool.acquire() as conn:
            return await conn.execute(query, *args)
```

### 1.3 Database Schema Optimization
- Add composite indices: `(chat_id, sent_at_utc)`, `(user_id, created_at)`
- Partition large tables (pings, anomalies) by date
- Implement database cleanup jobs for old data

## 🛡️ Phase 2: Memory & Resource Management

### 2.1 Memory Leak Prevention
**Current Issues Identified:**
- Global SQLite connection `CONN` shared across threads
- Unbounded `_rng_buffer` deque growth
- No cleanup of expired user sessions

**Solutions:**
```python
# Resource-managed database connections
@asynccontextmanager
async def get_db_connection():
    try:
        conn = await db_pool.acquire()
        yield conn
    finally:
        await db_pool.release(conn)

# Bounded entropy buffer with automatic cleanup
class BoundedEntropyBuffer:
    def __init__(self, max_size: int = 10000):
        self.buffer = deque(maxlen=max_size)
        self.lock = asyncio.Lock()
        
    async def add_entropy(self, data: bytes):
        async with self.lock:
            self.buffer.extend(data)
```

### 2.2 Thread Safety & Async Optimization
- Replace threading with asyncio where possible
- Implement proper async context managers
- Add connection timeout and retry logic
- Use semaphores to limit concurrent operations

## 📊 Phase 3: Monitoring & Observability

### 3.1 Health Check System
```python
@app.get("/health/detailed")
async def detailed_health():
    health_status = {
        "database": await check_database_health(),
        "telegram_api": await check_telegram_api(),
        "cloudflare_tunnel": await check_tunnel_health(),
        "entropy_generation": check_entropy_health(),
        "memory_usage": get_memory_stats(),
        "active_users": await get_active_user_count(),
        "uptime": get_uptime_seconds()
    }
    
    overall_status = "healthy" if all(
        status.get("status") == "ok" 
        for status in health_status.values()
    ) else "degraded"
    
    return {"status": overall_status, "checks": health_status}
```

### 3.2 Real-Time Metrics
- Prometheus metrics integration
- Grafana dashboards for system monitoring
- Alert thresholds for database connections, memory usage
- Response time histograms
- Error rate tracking

### 3.3 Structured Logging
```python
import structlog
import loguru

logger = structlog.get_logger()

# Replace print statements with structured logging
logger.info("user_ping_sent", 
    user_id=user_id,
    ping_type=ping_type,
    response_time_ms=response_time,
    success=True
)
```

## 📈 Capacity Envelope & Alert Thresholds (2025-06-27)

| User Range | Bottleneck | Current Ceiling | Alert Trigger |
|------------|------------|-----------------|---------------|
| 1–5 k      | none (single DB writer) | ≈8 pings/s, 4 000 RPS API | memory > 85 % |
| 5–20 k     | SQLite write lock | 50 writes/s | DB pool ≥ 80 % |
| 20–80 k    | sync Telegram calls | 30 msgs/s/token | circuit breaker open |
| 80–300 k   | O(n) alarm scan | heap scheduler needed | alarm_scan_lag > 2 s |

Scott Wilber: “Instrument first, optimise second — data-driven scaling avoids premature refactors.”

The **metrics watchdog** in `updatedui/server.py` now sends Telegram alerts (via `send_admin_alert`) when:

• Memory usage crosses 85 %  
• DB pool ≥80 % saturated  
• Telegram circuit breaker enters OPEN state  

Each alert key has a 5-minute cooldown to prevent spam.

## 🔄 Phase 4: Auto-Recovery & Resilience

### 4.1 Circuit Breaker Pattern
```python
class TelegramCircuitBreaker:
    def __init__(self, failure_threshold=5, timeout=60):
        self.failure_count = 0
        self.failure_threshold = failure_threshold
        self.timeout = timeout
        self.last_failure = None
        self.state = "closed"  # closed, open, half-open
    
    async def call(self, func, *args, **kwargs):
        if self.state == "open":
            if time.time() - self.last_failure > self.timeout:
                self.state = "half-open"
            else:
                raise CircuitBreakerOpenException()
        
        try:
            result = await func(*args, **kwargs)
            if self.state == "half-open":
                self.state = "closed"
                self.failure_count = 0
            return result
        except Exception as e:
            self.failure_count += 1
            if self.failure_count >= self.failure_threshold:
                self.state = "open"
                self.last_failure = time.time()
            raise
```

### 4.2 Graceful Degradation
- Fallback to polling if webhook fails
- Local SQLite backup if PostgreSQL unavailable
- Queue pending operations during outages
- Progressive backoff for failed API calls

### 4.3 Auto-Restart Mechanisms
```powershell
# Enhanced PowerShell watchdog script
while ($true) {
    $serverProcess = Get-Process -Name "python" -ErrorAction SilentlyContinue | 
        Where-Object { $_.MainWindowTitle -like "*uvicorn*" }
    
    if (-not $serverProcess) {
        Write-Host "⚠️ Server process not found, restarting..." -ForegroundColor Yellow
        Start-Process -FilePath "python" -ArgumentList "-m uvicorn updatedui.server:app --host 0.0.0.0 --port 8000" -NoNewWindow
    }
    
    # Check tunnel health
    $tunnelStatus = cloudflared tunnel info chronomancy-backend 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "⚠️ Tunnel unhealthy, restarting..." -ForegroundColor Yellow
        & cloudflared tunnel run chronomancy-backend &
    }
    
    Start-Sleep -Seconds 30
}
```

## 🚀 Phase 5: Performance Optimization

### 5.1 Database Query Optimization
```sql
-- Add strategic indices
CREATE INDEX CONCURRENTLY idx_pings_user_date ON pings(user_id, sent_at_utc);
CREATE INDEX CONCURRENTLY idx_anomalies_ping_id ON anomalies(ping_id);
CREATE INDEX CONCURRENTLY idx_users_active ON users(chat_id) WHERE tz_offset IS NOT NULL;

-- Partition large tables
CREATE TABLE pings_2024 PARTITION OF pings 
FOR VALUES FROM ('2024-01-01') TO ('2025-01-01');
```

### 5.2 Caching Layer
```python
import redis
from functools import wraps

redis_client = redis.Redis(host='localhost', port=6379, decode_responses=True)

def cache_result(expiry: int = 300):
    def decorator(func):
        @wraps(func)
        async def wrapper(*args, **kwargs):
            cache_key = f"{func.__name__}:{hash(str(args) + str(kwargs))}"
            cached = redis_client.get(cache_key)
            
            if cached:
                return json.loads(cached)
            
            result = await func(*args, **kwargs)
            redis_client.setex(cache_key, expiry, json.dumps(result))
            return result
        return wrapper
    return decorator

@cache_result(expiry=60)
async def get_user_stats(user_id: int):
    # Expensive database query cached for 60 seconds
    pass
```

### 5.3 Async Queue Processing
```python
import asyncio
from asyncio import Queue

class PingQueueProcessor:
    def __init__(self):
        self.queue = Queue(maxsize=1000)
        self.workers = []
    
    async def start_workers(self, num_workers: int = 5):
        for _ in range(num_workers):
            worker = asyncio.create_task(self._worker())
            self.workers.append(worker)
    
    async def _worker(self):
        while True:
            try:
                ping_task = await self.queue.get()
                await self._process_ping(ping_task)
                self.queue.task_done()
            except Exception as e:
                logger.error("ping_processing_error", error=str(e))
    
    async def add_ping(self, user_id: int, message: str):
        await self.queue.put({"user_id": user_id, "message": message})
```

## 📦 Phase 6: Infrastructure Hardening

### 6.1 Docker Containerization
```dockerfile
FROM python:3.11-slim

# Install system dependencies
RUN apt-get update && apt-get install -y \
    postgresql-client \
    redis-tools \
    && rm -rf /var/lib/apt/lists/*

# Install Python dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy application
COPY . /app
WORKDIR /app

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD python -c "import requests; requests.get('http://localhost:8000/health')"

CMD ["uvicorn", "updatedui.server:app", "--host", "0.0.0.0", "--port", "8000"]
```

### 6.2 Load Balancing & Scaling
```yaml
# docker-compose.yml
version: '3.8'
services:
  app:
    build: .
    ports:
      - "8000-8002:8000"
    environment:
      - DATABASE_URL=postgresql://user:pass@db:5432/chronomancy
      - REDIS_URL=redis://redis:6379
    depends_on:
      - db
      - redis
    deploy:
      replicas: 3
      
  db:
    image: postgres:15
    environment:
      POSTGRES_DB: chronomancy
      POSTGRES_USER: chronomancy
      POSTGRES_PASSWORD: secure_password
    volumes:
      - postgres_data:/var/lib/postgresql/data
      
  redis:
    image: redis:7-alpine
    
  nginx:
    image: nginx:alpine
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf
```

## 📈 Phase 7: Advanced Features

### 7.1 Rate Limiting
```python
from slowapi import Limiter, _rate_limit_exceeded_handler
from slowapi.util import get_remote_address

limiter = Limiter(key_func=get_remote_address)
app.state.limiter = limiter
app.add_exception_handler(RateLimitExceeded, _rate_limit_exceeded_handler)

@app.post("/api/user/{user_id}/anomaly")
@limiter.limit("10/minute")
async def submit_anomaly(request: Request, user_id: int, anomaly_data: dict):
    # Rate-limited anomaly submission
    pass
```

### 7.2 Data Validation & Security
```python
from pydantic import BaseModel, Field, validator
from datetime import datetime

class AnomalySubmission(BaseModel):
    description: str = Field(..., min_length=1, max_length=2000)
    timestamp: datetime = Field(default_factory=datetime.utcnow)
    user_id: int = Field(..., gt=0)
    
    @validator('description')
    def validate_description(cls, v):
        # Sanitize input, check for malicious content
        return v.strip()
```

## 🎯 Implementation Timeline

### Week 1-2: Database Migration & Connection Pooling
- [ ] Set up PostgreSQL instance
- [ ] Migrate SQLite data to PostgreSQL
- [ ] Implement asyncpg connection pooling
- [ ] Add database indices

### Week 3: Memory Management & Threading
- [ ] Replace global CONN with connection pools
- [ ] Implement bounded entropy buffer
- [ ] Add async context managers
- [ ] Memory leak testing

### Week 4: Monitoring & Health Checks
- [ ] Implement detailed health endpoints
- [ ] Add Prometheus metrics
- [ ] Set up Grafana dashboards
- [ ] Configure alerting

### Week 5-6: Resilience & Auto-Recovery
- [ ] Implement circuit breakers
- [ ] Add graceful degradation
- [ ] Create auto-restart scripts
- [ ] Load testing

### Week 7-8: Performance Optimization
- [ ] Add Redis caching layer
- [ ] Implement async queue processing
- [ ] Database query optimization
- [ ] Response time optimization

## 🔍 Success Metrics

### Performance KPIs
- **Database Response Time**: <50ms average
- **API Response Time**: <200ms P95
- **Memory Usage**: <500MB steady state
- **CPU Usage**: <30% under normal load
- **Error Rate**: <0.1% of requests

### Reliability KPIs
- **Uptime**: 99.9% monthly
- **MTTR**: <5 minutes
- **Database Connection Success**: >99.9%
- **Telegram API Success**: >99.5%
- **Auto-Recovery Success**: >95%

## 🚨 Monitoring Alerts

### Critical Alerts (PagerDuty)
- Database connection pool exhausted
- Memory usage >80%
- Error rate >1% for 5 minutes
- Health check failing for 2 minutes
- Cloudflare tunnel down

### Warning Alerts (Slack)
- Response time >500ms P95
- Database query >200ms
- Memory usage >60%
- Disk space >80%
- Failed ping delivery >5%

---

**Scott Wilber's Scaling Doctrine**: *"Temporal systems must exhibit emergent resilience patterns that mirror the quantum coherence found in natural consciousness networks. Each scaling decision should amplify rather than dampen the system's inherent synchronicity capacity."* 

## 🗂️ TODO – Multi-Token Telegram Sharding
_Trackable tasks for the upcoming "100 worker bots" refactor._

- [ ] Create additional bot accounts `@Chronomancy_0_bot … @Chronomancy_99_bot` and store tokens in the secret `BOT_TOKENS` (comma-separated).
- [ ] Add table `bot_assignment(chat_id INTEGER PRIMARY KEY, token TEXT NOT NULL, code TEXT)`.
- [ ] Implement **router bot** logic in `bot/router.py`:
  - `/start` ➜ choose shard `token_i = BOT_TOKENS[1 + (chat_id mod (N-1))]`.
  - Insert assignment row with a one-time `code`.
  - Reply with deep-link button to `t.me/<worker_username>?start=<code>`.
- [ ] Extend shared FastAPI `/telegram/{token}` endpoint to map incoming updates to the correct `TeleBot` instance.
- [ ] Update scheduler (`send_ping`) to `SELECT token FROM bot_assignment WHERE chat_id=?` and send via that bot.
- [ ] Script to bulk set webhooks for all tokens (see docs/SET_WEBHOOKS.sh).
- [ ] Load-test flood-wait handling: 1,000 chats per bot, 25 msg/s budget.
- [ ] Document migration path for existing users (router sends deep-link ask once).

_When all boxes are checked, we can safely handle ~2,500 msg/s with 100 bots – enough for the first 215 M daily pings._ 

## 📈 Capacity Envelope & Alert Thresholds (2025-06-27)

| User Range | Bottleneck | Current Ceiling | Alert Trigger |
|------------|------------|-----------------|---------------|
| 1–5 k      | none (single DB writer) | ≈8 pings/s, 4 000 RPS API | memory > 85 % |
| 5–20 k     | SQLite write lock | 50 writes/s | DB pool ≥ 80 % |
| 20–80 k    | sync Telegram calls | 30 msgs/s/token | circuit breaker open |
| 80–300 k   | O(n) alarm scan | heap scheduler needed | alarm_scan_lag > 2 s |

Scott Wilber: “Instrument first, optimise second — data-driven scaling avoids premature refactors.”

The **metrics watchdog** in `updatedui/server.py` now sends Telegram alerts (via `send_admin_alert`) when:

• Memory usage crosses 85 %  
• DB pool ≥80 % saturated  
• Telegram circuit breaker enters OPEN state  

Each alert key has a 5-minute cooldown to prevent spam.

--- 