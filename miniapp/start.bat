@echo off
REM Chronomancy Mini App Startup Script for Windows
REM Starts the Flask API server and sets up Cloudflare tunnel

echo 🌀 Starting Chronomancy Mini App...

REM Change to miniapp directory
cd /d "%~dp0"

REM Check if virtual environment exists, create if not
if not exist "venv" (
    echo Creating Python virtual environment...
    python -m venv venv
)

REM Activate virtual environment
call venv\Scripts\activate.bat

REM Install dependencies
echo Installing dependencies...
pip install -r requirements.txt

REM Check if database exists
if not exist "..\bot\chronomancy.db" (
    echo ⚠️  Warning: Bot database not found at ..\bot\chronomancy.db
    echo Make sure the Telegram bot has been run at least once to create the database.
)

REM Set environment variables
set FLASK_ENV=production
set PORT=5000

REM Start FastAPI server
echo Starting FastAPI server with uvicorn...
start "Chronomancy API" cmd /k "uvicorn server:app --host 0.0.0.0 --port 5000 --reload"

REM Wait a moment for FastAPI to start
timeout /t 3 /nobreak >nul

echo ✅ Chronomancy Mini App is running with FastAPI!
echo Server running locally on: http://localhost:5000
echo FastAPI docs available at: http://localhost:5000/docs

echo.
echo Press any key to stop the Mini App...
pause >nul

REM Cleanup (kill background processes)
taskkill /f /im python.exe >nul 2>&1
taskkill /f /im cloudflared.exe >nul 2>&1

echo ✅ Chronomancy Mini App stopped.
pause

echo Starting Chronomancy Services...

echo.
echo Starting Python Server...
start "Chronomancy Server" cmd /k "python server.py"

echo.
echo Starting Cloudflare Tunnel...
start "Chronomancy Tunnel" cmd /k "cloudflared tunnel --token eyJhIjoiMjUyY2VhYjUxYzhlZmE2YWVlNThjODFiMDk1NmM1ZWIiLCJ0IjoiYmQ0MDNhMTYtZGIwYS00MDkxLWJhYjMtNzFmOTk1ZDc3NDJhIiwicyI6Ik1UQTJORGt3TTJNdFpUSTJaaTAwWkRZeExXRXpObVl0TmpNek9EazBOamN6TlRJMiJ9"

echo.
echo ✅ Chronomancy services started!
echo 🌐 Local: http://localhost:5000
echo 🌍 Public: https://chronomancy.app
echo.
pause 