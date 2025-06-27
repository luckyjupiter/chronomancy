/**
 * Chronomancy Temporal Scanner - JavaScript Interface
 * 
 * Implements Scott Wilber's canonical minimal activation energy framework
 * for temporal exploration and anomaly detection.
 */

// Global state
let currentPings = 3;
let userId = null;
let isTimerActive = false;

// Initialize application
document.addEventListener('DOMContentLoaded', function() {
    initializeTelegramWebApp();
    loadUserStatus();
    handleUrlParams();
});

/**
 * Initialize Telegram Web App integration
 */
function initializeTelegramWebApp() {
    if (window.Telegram && window.Telegram.WebApp) {
        window.Telegram.WebApp.ready();
        window.Telegram.WebApp.expand();
        
        // Set theme colors
        window.Telegram.WebApp.setHeaderColor('#1a1a2e');
        window.Telegram.WebApp.setBackgroundColor('#1a1a2e');
        
        // Get user ID from Telegram
        if (window.Telegram.WebApp.initDataUnsafe.user) {
            userId = window.Telegram.WebApp.initDataUnsafe.user.id;
            console.log('Telegram user ID:', userId);
            localStorage.setItem('chronomancy_user_id', userId);
        } else {
            // Fallback for testing - use stored ID or create new one
            userId = localStorage.getItem('chronomancy_user_id') || Math.floor(Math.random() * 1000000) + 100000;
            localStorage.setItem('chronomancy_user_id', userId);
            console.log('Using fallback user ID:', userId);
        }
    } else {
        // Fallback for testing outside Telegram - use stored ID or create new one
        userId = localStorage.getItem('chronomancy_user_id') || Math.floor(Math.random() * 1000000) + 100000;
        localStorage.setItem('chronomancy_user_id', userId);
        console.log('Running outside Telegram, using test user ID:', userId);
    }
    
    console.log('Final user ID for all API calls:', userId);
}

/**
 * Load current user status and update UI
 */
async function loadUserStatus() {
    try {
        // Using the updated UI server endpoint
        const response = await fetch(`/api/user/${userId}/status`);
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }
        
        const status = await response.json();
        console.log('User status:', status);
        
        // Update UI elements
        updateStatusDisplay(status.timer_active);
        
        if (status.window_start) {
            document.getElementById('start-time').value = status.window_start;
        }
        if (status.window_end) {
            document.getElementById('end-time').value = status.window_end;
        }
        if (status.daily_count > 0) {
            currentPings = status.daily_count;
            document.getElementById('ping-count').textContent = currentPings;
        }
        
        isTimerActive = status.timer_active;
        
        // Show Lifetime Pass badge
        if (status.is_backer) {
            document.getElementById('premium-badge').style.display = 'inline-block';
        } else {
            document.getElementById('premium-badge').style.display = 'none';
        }
        
    } catch (error) {
        console.error('Error loading user status:', error);
        // Set default state
        updateStatusDisplay(false);
    }
}

/**
 * Update status display based on timer state
 */
function updateStatusDisplay(active) {
    const statusCard = document.getElementById('status-card');
    const statusText = document.getElementById('status-text');
    const activateBtn = document.getElementById('activate-btn');
    
    if (active) {
        statusCard.className = 'status-card status-active pulse';
        statusText.textContent = 'Scanner active - monitoring temporal field';
        activateBtn.textContent = '✓ Scanner Active';
        activateBtn.style.background = 'linear-gradient(135deg, #0f4c75, #1e6091)';
    } else {
        statusCard.className = 'status-card status-inactive';
        statusText.textContent = 'Configure your scanning window to begin';
        activateBtn.textContent = '🚀 Start Temporal Scanning';
        activateBtn.style.background = 'linear-gradient(135deg, #e94560, #c73650)';
    }
    
    isTimerActive = active;
}

/**
 * Adjust ping frequency
 */
function adjustPings(delta) {
    currentPings = Math.max(1, Math.min(10, currentPings + delta));
    document.getElementById('ping-count').textContent = currentPings;
    
    // Add visual feedback
    const display = document.getElementById('ping-count');
    display.style.transform = 'scale(1.2)';
    setTimeout(() => {
        display.style.transform = 'scale(1)';
    }, 150);
}

/**
 * Activate or update timer settings
 */
async function activateTimer() {
    const startTime = document.getElementById('start-time').value;
    const endTime = document.getElementById('end-time').value;

    if (!startTime || !endTime) {
        showAlert('Please set both start and end times');
        return;
    }

    if (startTime >= endTime) {
        showAlert('End time must be after start time');
        return;
    }

    try {
        // Show loading state
        const btn = document.getElementById('activate-btn');
        const originalText = btn.textContent;
        btn.textContent = '⏳ Configuring...';
        btn.disabled = true;

        // Calculate timezone offset
        const timezoneOffset = -(new Date().getTimezoneOffset() / 60);

        const timerData = {
            window_start: startTime,
            window_end: endTime,
            daily_count: currentPings,
            tz_offset: timezoneOffset
        };
        
        console.log('=== TIMER ACTIVATION DEBUG ===');
        console.log('User ID:', userId);
        console.log('Timer data:', timerData);
        console.log('API URL:', `/api/user/${userId}/timer`);
        console.log('Full URL:', `${window.location.origin}/api/user/${userId}/timer`);

        const response = await fetch(`/api/user/${userId}/timer`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(timerData)
        });

        console.log('Response status:', response.status);
        console.log('Response ok:', response.ok);
        console.log('Response headers:', [...response.headers.entries()]);

        if (response.ok) {
            const responseData = await response.json();
            console.log('Success response:', responseData);
            updateStatusDisplay(true);

            // Standard success note
            let alertMsg = '🎯 Temporal scanner activated! Pings will arrive via Telegram even if this page is closed.';

            // If the backend could NOT DM the user, prompt them to open the bot and press START
            if (responseData.confirmation_sent === false) {
                alertMsg += '\n\n⚠️  One more step: open @chronomancy_bot in Telegram and press START so the bot can message you.';
            }

            showAlert(alertMsg);
            
            // Add success animation
            const statusCard = document.getElementById('status-card');
            statusCard.style.transform = 'scale(1.05)';
            setTimeout(() => {
                statusCard.style.transform = 'scale(1)';
            }, 300);
            
        } else {
            let errorText;
            try {
                errorText = await response.text();
            } catch (e) {
                errorText = 'Failed to read error response';
            }
            console.error('=== ERROR RESPONSE ===');
            console.error('Status:', response.status);
            console.error('Status Text:', response.statusText);
            console.error('Response Body:', errorText);
            console.error('=====================');
            throw new Error(`HTTP ${response.status}: ${errorText}`);
        }
        
    } catch (error) {
        console.error('Error activating timer:', error);
        showAlert(`❌ Error activating scanner: ${error.message}. Please try again.`);
        
    } finally {
        // Restore button state
        const btn = document.getElementById('activate-btn');
        btn.disabled = false;
        if (!isTimerActive) {
            btn.textContent = '🚀 Start Temporal Scanning';
        }
    }
}

/**
 * Mute timer for specified duration
 */
async function muteTimer() {
    try {
        const response = await fetch(`/api/user/${userId}/mute`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ hours: 24 })
        });

        if (response.ok) {
            updateStatusDisplay(false);
            showAlert('🔇 Scanner muted for 24 hours');
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
        
    } catch (error) {
        console.error('Error muting timer:', error);
        showAlert('❌ Error muting scanner. Please try again.');
    }
}

/**
 * Load and display scanning challenge
 */
async function loadChallenge() {
    try {
        const response = await fetch('/api/challenge');
        if (response.ok) {
            const data = await response.json();
            document.getElementById('challenge-text').textContent = data.challenge;
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
    } catch (error) {
        console.error('Error loading challenge:', error);
        document.getElementById('challenge-text').textContent = 
            'Scan your surroundings for anything unusual, unexpected, or meaningful...';
    }
}

/**
 * Submit anomaly observation
 */
async function submitAnomaly() {
    const description = document.getElementById('anomaly-description').value.trim();
    
    if (!description) {
        showAlert('Please describe what you observed');
        return;
    }

    try {
        // Show loading state
        const btn = event.target;
        const originalText = btn.textContent;
        btn.textContent = '📡 Recording...';
        btn.disabled = true;

        const response = await fetch(`/api/user/${userId}/anomaly`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                description: description,
                timestamp: new Date().toISOString()
            })
        });

        if (response.ok) {
            showAlert('✨ Observation recorded! Thank you for scanning the temporal field.');
            
            // Clear the form and return to timer screen
            document.getElementById('anomaly-description').value = '';
            
            // Add success animation before transitioning
            const section = document.querySelector('#anomaly-screen .section');
            section.style.background = 'linear-gradient(135deg, #0f4c75, rgba(15, 76, 117, 0.3))';
            
            setTimeout(() => {
                showTimerScreen();
            }, 1000);
            
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
        
    } catch (error) {
        console.error('Error submitting anomaly:', error);
        showAlert('❌ Error recording observation. Please try again.');
        
    } finally {
        // Restore button state
        const btn = event.target;
        btn.textContent = '📝 Record Observation';
        btn.disabled = false;
    }
}

/**
 * Skip anomaly reporting
 */
function skipAnomaly() {
    showAlert('👁️ No anomalies detected this time. Stay vigilant, Chronomancer.');
    showTimerScreen();
}

/**
 * Show timer configuration screen
 */
function showTimerScreen() {
    document.getElementById('timer-screen').classList.remove('hidden');
    document.getElementById('anomaly-screen').classList.add('hidden');
    
    // Refresh status
    loadUserStatus();
}

/**
 * Show anomaly reporting screen
 */
function showAnomalyScreen() {
    document.getElementById('timer-screen').classList.add('hidden');
    document.getElementById('anomaly-screen').classList.remove('hidden');
    
    // Load challenge and focus textarea
    loadChallenge();
    setTimeout(() => {
        document.getElementById('anomaly-description').focus();
    }, 300);
}

/**
 * Handle URL parameters for screen routing
 */
function handleUrlParams() {
    const urlParams = new URLSearchParams(window.location.search);
    if (urlParams.get('mode') === 'anomaly') {
        showAnomalyScreen();
    }
}

/**
 * Show alert message (uses Telegram WebApp if available)
 */
function showAlert(message) {
    if (window.Telegram && window.Telegram.WebApp && window.Telegram.WebApp.showAlert) {
        window.Telegram.WebApp.showAlert(message);
    } else {
        alert(message);
    }
}

/**
 * Utility: Add visual feedback to buttons
 */
document.addEventListener('click', function(e) {
    if (e.target.classList.contains('btn') || e.target.classList.contains('frequency-btn')) {
        e.target.style.transform = 'scale(0.95)';
        setTimeout(() => {
            e.target.style.transform = '';
        }, 100);
    }
});

/**
 * Add keyboard shortcuts for power users
 */
document.addEventListener('keydown', function(e) {
    // Escape key returns to timer screen
    if (e.key === 'Escape') {
        showTimerScreen();
    }
    
    // Ctrl/Cmd + Enter submits anomaly from textarea
    if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
        if (!document.getElementById('anomaly-screen').classList.contains('hidden')) {
            submitAnomaly();
        }
    }
});

// Export functions for global access
window.adjustPings = adjustPings;
window.activateTimer = activateTimer;
window.muteTimer = muteTimer;
window.submitAnomaly = submitAnomaly;
window.skipAnomaly = skipAnomaly;
window.showTimerScreen = showTimerScreen;
window.showAnomalyScreen = showAnomalyScreen; 