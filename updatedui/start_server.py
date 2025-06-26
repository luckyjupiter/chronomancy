#!/usr/bin/env python3
"""
Simple server startup script for the Chronomancy Updated UI
"""

import uvicorn
from server import app

if __name__ == "__main__":
    print("🏵️ Starting Chronomancy Updated UI Server...")
    print("📍 Serving at http://localhost:5000")
    
    try:
        uvicorn.run(
            app, 
            host="0.0.0.0", 
            port=5000, 
            log_level="info",
            reload=False
        )
    except Exception as e:
        print(f"❌ Error starting server: {e}")
        input("Press Enter to exit...") 