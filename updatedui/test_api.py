import requests
import time

print("Testing Chronomancy API...")

# Wait for server to start
time.sleep(3)

try:
    # Test health endpoint
    health_resp = requests.get('http://localhost:5001/health')
    print(f"Health endpoint: {health_resp.status_code}")
    
    # Test status endpoint
    status_resp = requests.get('http://localhost:5001/api/user/12345/status')
    print(f"Status endpoint: {status_resp.status_code}")
    
    # Test timer endpoint
    timer_resp = requests.post('http://localhost:5001/api/user/12345/timer', json={
        'window_start': '09:00',
        'window_end': '21:00', 
        'daily_count': 3,
        'tz_offset': 0
    })
    print(f"Timer endpoint: {timer_resp.status_code}")
    
    if timer_resp.status_code == 200:
        print("✅ All API endpoints working!")
    else:
        print(f"❌ Timer API failed: {timer_resp.text}")

except Exception as e:
    print(f"❌ Error testing API: {e}") 