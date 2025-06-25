/**
 * Chronomancy Mini App - Main Application Logic
 * 
 * Enhanced temporal exploration interface implementing Scott Wilber's
 * comprehensive framework for anomaly detection and pattern analysis.
 * Provides full feature parity with the Telegram bot backend.
 */

class ChromancyApp {
    constructor() {
        this.state = {
            user: null,
            settings: null,
            profile: null,
            activePings: [],
            globalSyncTime: null,
            currentPanel: null,
            userReports: [],
            futureMessages: [],
            challenges: [],
            isLoading: false
        };
        
        this.eventSource = null;
        this.updateInterval = null;
        this.scene = null;
        
        // Ensure DOM is ready before initializing
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', () => this.init());
        } else {
            this.init();
        }
    }
    
    init() {
        console.log('Initializing Chronomancy App...');
        
        // Initialize Telegram Web App
        if (window.Telegram?.WebApp) {
            window.Telegram.WebApp.ready();
            window.Telegram.WebApp.expand();
            
            // Get user data from Telegram
            this.state.user = window.Telegram.WebApp.initDataUnsafe?.user || {
                id: 12345,
                first_name: "Demo User"
            };
            console.log('Telegram user:', this.state.user);
        }
        
        // Initialize API
        if (window.chromancyAPI) {
            console.log('API initialized');
        } else {
            console.error('API not available');
        }
        
        // Initialize temporal scene
        try {
            this.scene = new TemporalScene();
            console.log('Temporal scene initialized');
        } catch (error) {
            console.error('Error initializing scene:', error);
        }
        
        // Set up event listeners
        this.setupEventListeners();
        
        // Load initial data
        this.loadUserProfile().then(() => {
            // After profile loaded, auto-sync timezone if missing or outdated
            const localTzOffset = -new Date().getTimezoneOffset() / 60; // JS offset is negative of UTC hours
            if (
                !this.state.profile?.tz_offset ||
                this.state.profile.tz_offset !== localTzOffset
            ) {
                console.log(`Updating timezone offset to ${localTzOffset}`);
                if (window.chromancyAPI) {
                    window.chromancyAPI.updateWindowSettings({ tz_offset: localTzOffset }).catch(
                        (err) => console.error('Failed to update tz_offset', err)
                    );
                }
            }
        });
        this.loadGlobalStats();
        this.startUpdates();
        
        // Initialize quota management
        this.initializeQuotaManagement();
        
        console.log('App initialization complete');
    }
    
    async waitForTelegram() {
        return new Promise((resolve) => {
            const checkReady = () => {
                if (window.telegramApp && window.telegramApp.isReady) {
                    this.state.user = window.telegramUser;
                    resolve();
                } else {
                    setTimeout(checkReady, 100);
                }
            };
            checkReady();
        });
    }
    
    async loadUserData() {
        try {
            // Load user profile
            const profile = await window.chromancyAPI.getUserProfile();
            this.state.profile = profile;
            
            // Load user settings
            const settings = await window.chromancyAPI.getUserSettings();
            this.state.settings = settings;
            
            // Load user reports
            const reports = await window.chromancyAPI.getUserReports();
            this.state.userReports = reports;
            
            // Load future messages
            const futureMessages = await window.chromancyAPI.getFutureMessages();
            this.state.futureMessages = futureMessages;
            
            // Load global sync time
            const sync = await window.chromancyAPI.getGlobalSync();
            this.state.globalSyncTime = new Date(sync.next_sync * 1000);
            
            // Load challenges
            const challenges = await window.chromancyAPI.getChallenges();
            this.state.challenges = challenges;
            
        } catch (error) {
            console.error('Error loading user data:', error);
        }
    }
    
    setupEventListeners() {
        console.log('Setting up event listeners...');
        
        // Navigation buttons
        const navButtons = [
            { id: 'show-profile', panel: 'profile-panel', action: () => this.loadProfileData() },
            { id: 'show-anomaly', panel: 'anomaly-panel' },
            { id: 'show-history', panel: 'history-panel', action: () => this.loadHistory() },
            { id: 'show-activity', panel: 'activity-panel', action: () => this.loadActivityData() },
            { id: 'show-global', panel: 'global-panel', action: () => this.loadGlobalStats() },
            { id: 'show-challenges', panel: 'challenges-panel', action: () => this.loadChallenges() },
            { id: 'show-future', panel: 'future-panel', action: () => this.loadFutureMessages() },
            { id: 'show-settings', panel: 'settings-panel' }
        ];
        
        navButtons.forEach(({ id, panel, action }) => {
            const button = document.getElementById(id);
            if (button) {
                button.addEventListener('click', (e) => {
                    console.log(`Button clicked: ${id}`);
                    this.createRipple(e);
                    this.showPanel(panel);
                    if (action) action();
                });
                console.log(`Event listener added for: ${id}`);
            } else {
                console.warn(`Button not found: ${id}`);
            }
        });
        
        // Close buttons
        document.querySelectorAll('.close-button').forEach(button => {
            button.addEventListener('click', (e) => {
                console.log('Close button clicked');
                const panel = button.closest('.panel');
                if (panel) {
                    this.closePanel(panel.id);
                }
            });
        });
        
        // Quota management buttons
        const quotaPlusBtn = document.getElementById('quota-plus');
        const quotaMinusBtn = document.getElementById('quota-minus');
        
        if (quotaPlusBtn) {
            quotaPlusBtn.addEventListener('click', (e) => {
                console.log('Quota plus clicked');
                this.createRipple(e);
                this.adjustQuota(1);
            });
        }
        
        if (quotaMinusBtn) {
            quotaMinusBtn.addEventListener('click', (e) => {
                console.log('Quota minus clicked');
                this.createRipple(e);
                this.adjustQuota(-1);
            });
        }
        
        // Mute buttons
        document.querySelectorAll('.mute-btn').forEach(button => {
            button.addEventListener('click', (e) => {
                console.log('Mute button clicked:', button.dataset.duration);
                this.createRipple(e);
                this.activateMute(button.dataset.duration);
            });
        });
        
        // Report anomaly button
        const reportBtn = document.getElementById('report-anomaly');
        if (reportBtn) {
            reportBtn.addEventListener('click', (e) => {
                console.log('Report anomaly clicked');
                this.createRipple(e);
                this.reportAnomaly();
            });
        }
        
        console.log('Event listeners setup complete');
    }
    
    startRealtimeUpdates() {
        // Update UI every minute
        this.updateInterval = setInterval(() => {
            this.updateTimeDisplays();
            this.refreshGlobalSync();
        }, 60000);
        
        // Initial time update
        this.updateTimeDisplays();
    }
    
    async refreshGlobalSync() {
        try {
            const sync = await window.chromancyAPI.getGlobalSync();
            this.state.globalSyncTime = new Date(sync.next_sync * 1000);
            this.updateGlobalSyncDisplay();
        } catch (error) {
            console.error('Error refreshing global sync:', error);
        }
    }
    
    updateUI() {
        this.updateUserInfo();
        this.updateSettingsDisplay();
        this.updateProfileDisplay();
        this.updateTimeDisplays();
    }
    
    updateUserInfo() {
        if (this.state.user) {
            const headerElement = document.querySelector('.header p');
            if (headerElement) {
                headerElement.textContent = 
                    `Welcome, ${this.state.user.firstName || 'Temporal Explorer'}`;
            }
        }
    }
    
    updateSettingsDisplay() {
        const windowElement = document.getElementById('personal-window');
        if (windowElement && this.state.settings) {
            const { ping_start, ping_end } = this.state.settings;
            windowElement.textContent = 
                `${ping_start}-${ping_end}`;
        }
    }
    
    updateProfileDisplay() {
        if (this.state.profile) {
            const element = document.getElementById('profile-summary');
            if (element) {
                element.innerHTML = `
                    <div>Future Messages: ${this.state.profile.future_message_count}</div>
                    <div>Anomalies Reported: ${this.state.profile.anomaly_count}</div>
                    <div>Daily Pings: ${this.state.profile.daily_count}</div>
                `;
            }
        }
    }
    
    updateTimeDisplays() {
        this.updateGlobalSyncDisplay();
        this.updateSyncTimer();
        this.updateWindowDisplay();
    }
    
    updateSyncTimer() {
        const now = new Date();
        const nextSync = new Date(now);
        nextSync.setHours(nextSync.getHours() + 1);
        nextSync.setMinutes(0);
        nextSync.setSeconds(0);
        
        const diff = nextSync - now;
        const hours = Math.floor(diff / (1000 * 60 * 60));
        const minutes = Math.floor((diff % (1000 * 60 * 60)) / (1000 * 60));
        const seconds = Math.floor((diff % (1000 * 60)) / 1000);
        
        const syncElement = document.getElementById('global-sync-time');
        if (syncElement) {
            syncElement.textContent = 
                `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
        }
    }
    
    updateWindowDisplay() {
        const now = new Date();
        const hour = now.getHours();
        
        let windowName, windowIcon;
        
        if (hour >= 6 && hour < 12) {
            windowName = "Morning";
            windowIcon = "🌅";
        } else if (hour >= 12 && hour < 18) {
            windowName = "Afternoon";
            windowIcon = "☀️";
        } else if (hour >= 18 && hour < 22) {
            windowName = "Evening";
            windowIcon = "🌆";
        } else {
            windowName = "Night";
            windowIcon = "🌙";
        }
        
        const windowNameEl = document.getElementById('window-name');
        const windowIconEl = document.getElementById('window-icon');
        
        if (windowNameEl) windowNameEl.textContent = windowName;
        if (windowIconEl) windowIconEl.textContent = windowIcon;
    }
    
    getWindowIcon(windowName) {
        const icons = {
            'Dawn Flow': '🌅',
            'Solar Peak': '☀️',
            'Golden Hour': '🌇',
            'Twilight Depth': '🌙',
            'Void Space': '🌌',
            'Continuous Flow': '∞',
            'Evening Flow': '🌅',
            'Default': '🌀'
        };
        return icons[windowName] || '🌀';
    }
    
    createRipple(event) {
        const button = event.currentTarget;
        const ripple = document.createElement('span');
        const rect = button.getBoundingClientRect();
        const size = Math.max(rect.width, rect.height);
        const x = event.clientX - rect.left - size / 2;
        const y = event.clientY - rect.top - size / 2;
        
        ripple.style.cssText = `
            position: absolute;
            width: ${size}px;
            height: ${size}px;
            left: ${x}px;
            top: ${y}px;
            background: rgba(255, 255, 255, 0.3);
            border-radius: 50%;
            pointer-events: none;
            transform: scale(0);
            animation: ripple-animation 0.6s ease-out;
        `;
        
        button.style.position = 'relative';
        button.style.overflow = 'hidden';
        button.appendChild(ripple);
        
        ripple.addEventListener('animationend', () => ripple.remove());
    }
    
    updateGlobalSyncDisplay() {
        const syncElement = document.getElementById('global-sync-time');
        if (syncElement && this.state.globalSyncTime) {
            const now = new Date();
            const timeToSync = this.state.globalSyncTime.getTime() - now.getTime();
            
            if (timeToSync > 0) {
                const hours = Math.floor(timeToSync / (1000 * 60 * 60));
                const minutes = Math.floor((timeToSync % (1000 * 60 * 60)) / (1000 * 60));
                syncElement.textContent = `${hours}h ${minutes}m`;
                
                // Update Three.js visualization
                if (window.temporalScene) {
                    window.temporalScene.updateGlobalSyncVisualization(timeToSync / 1000);
                }
            } else {
                syncElement.textContent = 'Synchronizing...';
            }
        }
    }
    
    // Panel Management
    showPanel(panelId) {
        console.log(`Showing panel: ${panelId}`);
        // Hide all other panels
        document.querySelectorAll('.panel').forEach(panel => {
            panel.classList.remove('active');
        });
        
        // Show the requested panel
        const panel = document.getElementById(panelId);
        if (panel) {
            panel.classList.add('active');
            console.log(`Panel ${panelId} shown`);
        } else {
            console.error(`Panel not found: ${panelId}`);
        }
    }
    
    closePanel(panelId) {
        console.log(`Closing panel: ${panelId}`);
        const panel = document.getElementById(panelId);
        if (panel) {
            panel.classList.remove('active');
        }
    }
    
    // Settings Management
    async saveSettings() {
        try {
            const timePreset = document.getElementById('time-preset').value;
            const timezone = document.getElementById('timezone').value || 'UTC';
            
            let pingStart, pingEnd;
            
            switch (timePreset) {
                case 'day':
                    pingStart = '09:00';
                    pingEnd = '18:00';
                    break;
                case 'evening':
                    pingStart = '18:00';
                    pingEnd = '23:00';
                    break;
                case 'night':
                    pingStart = '23:00';
                    pingEnd = '06:00';
                    break;
                default:
                    pingStart = '09:00';
                    pingEnd = '18:00';
            }
            
            const settings = {
                user_id: this.state.user.id,
                timezone,
                ping_start: pingStart,
                ping_end: pingEnd,
                ping_enabled: true
            };
            
            await window.chromancyAPI.updateUserSettings(settings);
            
            // Update local state
            this.state.settings = settings;
            this.updateSettingsDisplay();
            this.hidePanel('setup-panel');
            
            // Success feedback
            if (window.Telegram?.WebApp?.HapticFeedback) {
                window.Telegram.WebApp.HapticFeedback.impactOccurred('medium');
            }
            
            console.log('Settings saved successfully');
            
        } catch (error) {
            console.error('Error saving settings:', error);
        }
    }
    
    async saveWindowSettings() {
        try {
            const windowStart = document.getElementById('window-start').value;
            const windowEnd = document.getElementById('window-end').value;
            const dailyCount = parseInt(document.getElementById('daily-count').value) || 3;
            const tzOffset = new Date().getTimezoneOffset();
            
            const windowSettings = {
                window_start: windowStart,
                window_end: windowEnd,
                daily_count: dailyCount,
                tz_offset: -tzOffset // Convert to positive offset
            };
            
            await window.chromancyAPI.updateWindowSettings(windowSettings);
            
            // Update local state
            this.state.profile = { ...this.state.profile, ...windowSettings };
            this.updateProfileDisplay();
            this.hidePanel('window-panel');
            
            console.log('Window settings saved successfully');
            
        } catch (error) {
            console.error('Error saving window settings:', error);
        }
    }
    
    // Anomaly Reporting
    async submitAnomaly() {
        const textArea = document.getElementById('anomaly-text');
        const description = textArea.value.trim();
        
        if (!description) {
            textArea.focus();
            return;
        }
        
        try {
            const anomaly = {
                user_id: this.state.user.id,
                description,
                location: null,
                latitude: null,
                longitude: null,
                media_url: null
            };
            
            // Try to get location if available
            if (navigator.geolocation) {
                try {
                    const position = await new Promise((resolve, reject) => {
                        navigator.geolocation.getCurrentPosition(resolve, reject, {
                            timeout: 5000,
                            enableHighAccuracy: false
                        });
                    });
                    
                    anomaly.latitude = position.coords.latitude;
                    anomaly.longitude = position.coords.longitude;
                    anomaly.location = `${position.coords.latitude.toFixed(4)}, ${position.coords.longitude.toFixed(4)}`;
                } catch (geoError) {
                    console.log('Location not available:', geoError);
                }
            }
            
            await window.chromancyAPI.reportAnomaly(anomaly);
            
            // Clear form and update UI
            textArea.value = '';
            this.hidePanel('anomaly-panel');
            
            // Trigger visual effect
            if (window.temporalScene) {
                window.temporalScene.triggerAnomalyEffect();
            }
            
            // Success feedback
            if (window.Telegram?.WebApp?.HapticFeedback) {
                window.Telegram.WebApp.HapticFeedback.impactOccurred('heavy');
            }
            
            // Refresh user data
            await this.loadUserData();
            this.updateUI();
            
            console.log('Anomaly report submitted successfully');
            
        } catch (error) {
            console.error('Error submitting anomaly:', error);
        }
    }
    
    // Data Management
    async loadHistory() {
        const historyContent = document.getElementById('timeline-content');
        
        try {
            historyContent.innerHTML = '<div class="loading">Loading timeline...</div>';
            
            const reports = await window.chromancyAPI.getUserReports();
            
            if (reports.length > 0) {
                const historyHTML = reports.map(report => `
                    <div class="timeline-entry">
                        <div class="timeline-date">
                            ${this.formatDate(new Date(report.created_at))}
                        </div>
                        <div class="timeline-content">
                            ${this.escapeHtml(report.text || 'Anomaly detected')}
                            ${report.ping_type ? `<div style="color: #b8c5d6; font-size: 0.8rem; margin-top: 4px;">Type: ${report.ping_type}</div>` : ''}
                        </div>
                    </div>
                `).join('');
                
                historyContent.innerHTML = historyHTML;
            } else {
                historyContent.innerHTML = 
                    '<div class="loading">No anomalies recorded yet. Start exploring!</div>';
            }
            
        } catch (error) {
            console.error('Error loading history:', error);
            historyContent.innerHTML = 
                '<div class="loading">Error loading timeline. Please try again.</div>';
        }
    }
    
    async loadUserProfile() {
        try {
            const profile = await window.chromancyAPI.getUserProfile();
            this.state.profile = profile;
            this.updateQuotaDisplay();
            this.updateSettingsDisplay();
            console.log('Profile loaded:', profile);
        } catch (error) {
            console.error('Error loading profile:', error);
        }
    }
    
    async loadActivityData() {
        const activityContent = document.getElementById('activity-content');
        
        try {
            activityContent.innerHTML = '<div class="loading">Loading activity data...</div>';
            
            const activity = await window.chromancyAPI.getUserActivity();
            
            activityContent.innerHTML = `
                <div class="user-card">
                    <h3>📊 Overall Statistics</h3>
                    <div class="user-stats">
                        <div class="stat-item">
                            <div class="stat-value">${activity.total_pings}</div>
                            <div class="stat-label">Total Pings</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-value">${activity.anomaly_count}</div>
                            <div class="stat-label">Anomaly Reports</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-value">${(activity.response_rate * 100).toFixed(1)}%</div>
                            <div class="stat-label">Response Rate</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-value">${activity.average_response_time || 'N/A'}</div>
                            <div class="stat-label">Avg Response</div>
                        </div>
                    </div>
                </div>
                
                <div class="user-card">
                    <h3>🎯 Ping Types</h3>
                    <div class="reports-list">
                        ${Object.entries(activity.ping_stats).map(([type, count]) => `
                            <div class="report-item">
                                <div class="report-type">${type}</div>
                                <div class="report-description">${count} occurrences</div>
                            </div>
                        `).join('')}
                    </div>
                </div>
            `;
            
        } catch (error) {
            console.error('Error loading activity:', error);
            activityContent.innerHTML = '<div class="loading">Error loading activity data.</div>';
        }
    }
    
    async loadFutureMessages() {
        const messagesContent = document.getElementById('messages-content');
        
        try {
            messagesContent.innerHTML = '<div class="loading">Loading future messages...</div>';
            
            const messages = await window.chromancyAPI.getFutureMessages();
            
            if (messages.length > 0) {
                const messagesHTML = messages.map(msg => `
                    <div class="report-item">
                        <div class="report-description">${this.escapeHtml(msg.message)}</div>
                        <button onclick="window.chronApp.deleteFutureMessage(${msg.id})" class="button" style="background: rgba(239, 68, 68, 0.2); color: #ef4444; border: 1px solid rgba(239, 68, 68, 0.3); padding: 4px 8px; font-size: 0.8rem;">
                            🗑️ Delete
                        </button>
                    </div>
                `).join('');
                
                messagesContent.innerHTML = `
                    <div class="user-card">
                        <h3>💭 Your Future Messages</h3>
                        <div class="reports-list">${messagesHTML}</div>
                    </div>
                    
                    <div class="user-card">
                        <h3>➕ Add New Message</h3>
                        <div class="form-group">
                            <input type="text" id="new-message-text" placeholder="Enter future message..." maxlength="200" class="form-input">
                            <button onclick="window.chronApp.addFutureMessage()" class="button">Add Message</button>
                        </div>
                    </div>
                `;
            } else {
                messagesContent.innerHTML = `
                    <div class="user-card">
                        <div class="loading">No future messages set.</div>
                    </div>
                    
                    <div class="user-card">
                        <h3>➕ Add New Message</h3>
                        <div class="form-group">
                            <input type="text" id="new-message-text" placeholder="Enter future message..." maxlength="200" class="form-input">
                            <button onclick="window.chronApp.addFutureMessage()" class="button">Add Message</button>
                        </div>
                    </div>
                `;
            }
            
        } catch (error) {
            console.error('Error loading future messages:', error);
            messagesContent.innerHTML = '<div class="loading">Error loading messages.</div>';
        }
    }
    
    async addFutureMessage() {
        const input = document.getElementById('new-message-text');
        const text = input.value.trim();
        
        if (!text) {
            input.focus();
            return;
        }
        
        try {
            await window.chromancyAPI.addFutureMessage(text);
            input.value = '';
            await this.loadFutureMessages(); // Refresh the list
            
        } catch (error) {
            console.error('Error adding future message:', error);
        }
    }
    
    async deleteFutureMessage(messageId) {
        try {
            await window.chromancyAPI.deleteFutureMessage(messageId);
            await this.loadFutureMessages(); // Refresh the list
            
        } catch (error) {
            console.error('Error deleting future message:', error);
        }
    }
    
    async loadChallenges() {
        const challengesContent = document.getElementById('challenges-content');
        
        try {
            challengesContent.innerHTML = '<div class="loading">Loading challenges...</div>';
            
            const challenges = await window.chromancyAPI.getChallenges();
            
            challengesContent.innerHTML = `
                <div class="user-card">
                    <button onclick="window.chronApp.getRandomChallenge()" class="button">
                        🎯 Get Random Challenge
                    </button>
                    <div id="current-challenge" style="margin-top: 16px;"></div>
                </div>
                
                <div class="user-card">
                    <h3>🎪 Available Challenges</h3>
                    <div class="reports-list">
                        ${challenges.map(challenge => `
                            <div class="report-item">
                                <div class="report-description">${this.escapeHtml(challenge.text)}</div>
                            </div>
                        `).join('')}
                    </div>
                </div>
            `;
            
        } catch (error) {
            console.error('Error loading challenges:', error);
            challengesContent.innerHTML = '<div class="loading">Error loading challenges.</div>';
        }
    }
    
    async getRandomChallenge() {
        try {
            const challenge = await window.chromancyAPI.getRandomChallenge();
            const currentChallenge = document.getElementById('current-challenge');
            
            if (currentChallenge) {
                currentChallenge.innerHTML = `
                    <div style="background: rgba(255, 215, 0, 0.1); border: 1px solid rgba(255, 215, 0, 0.3); border-radius: 8px; padding: 16px; margin-top: 16px;">
                        <div style="color: #ffd700; font-weight: 600; margin-bottom: 8px;">🌟 Your Challenge:</div>
                        <div style="color: #ffffff; line-height: 1.4;">${this.escapeHtml(challenge.text)}</div>
                    </div>
                `;
            }
            
        } catch (error) {
            console.error('Error getting random challenge:', error);
        }
    }
    
    async loadGlobalStats() {
        const statsContent = document.getElementById('stats-content');
        
        try {
            statsContent.innerHTML = '<div class="loading">Loading global statistics...</div>';
            
            const stats = await window.chromancyAPI.getGlobalStats();
            
            statsContent.innerHTML = `
                <div class="user-card">
                    <h3>🌐 Network Overview</h3>
                    <div class="user-stats">
                        <div class="stat-item">
                            <div class="stat-value">${stats.active_users}</div>
                            <div class="stat-label">Active Users</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-value">${stats.total_pings.toLocaleString()}</div>
                            <div class="stat-label">Total Pings</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-value">${stats.total_anomalies.toLocaleString()}</div>
                            <div class="stat-label">Total Anomalies</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-value">${(stats.response_rate * 100).toFixed(1)}%</div>
                            <div class="stat-label">Response Rate</div>
                        </div>
                    </div>
                </div>
                
                <div class="user-card">
                    <h3>📈 Recent Activity</h3>
                    <div class="form-group">
                        <label class="form-label">Recent Pings</label>
                        <div class="status-value">${stats.recent_pings} in last 24h</div>
                    </div>
                    <div class="form-group">
                        <label class="form-label">Next Global Sync</label>
                        <div class="status-value">${stats.next_sync_hours}h remaining</div>
                    </div>
                    <div class="form-group">
                        <label class="form-label">Network Status</label>
                        <div class="status-value" style="color: #4ade80;">Synchronized</div>
                    </div>
                </div>
            `;
            
        } catch (error) {
            console.error('Error loading global stats:', error);
            statsContent.innerHTML = '<div class="loading">Error loading statistics.</div>';
        }
    }
    
    async exportUserData() {
        try {
            const blob = await window.chromancyAPI.exportUserData();
            
            // Create download link
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `chronomancy-data-${new Date().toISOString().split('T')[0]}.csv`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            window.URL.revokeObjectURL(url);
            
            console.log('Data export completed');
            
        } catch (error) {
            console.error('Error exporting data:', error);
        }
    }
    
    // Utility functions
    formatDate(date) {
        return new Intl.DateTimeFormat('en-US', {
            month: 'short',
            day: 'numeric',
            hour: 'numeric',
            minute: '2-digit'
        }).format(date);
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    // Cleanup
    destroy() {
        if (this.eventSource) {
            this.eventSource.close();
        }
        
        if (this.updateInterval) {
            clearInterval(this.updateInterval);
        }
    }
    
    // Quota Management System
    initializeQuotaManagement() {
        const plusBtn = document.getElementById('quota-plus');
        const minusBtn = document.getElementById('quota-minus');
        
        if (plusBtn) {
            plusBtn.addEventListener('click', (e) => {
                this.createRipple(e, plusBtn);
                this.adjustQuota(1);
            });
        }
        
        if (minusBtn) {
            minusBtn.addEventListener('click', (e) => {
                this.createRipple(e, minusBtn);
                this.adjustQuota(-1);
            });
        }
    }
    
    async adjustQuota(delta) {
        console.log(`Adjusting quota by: ${delta}`);
        const currentQuota = this.state.profile?.daily_count || 3;
        const newQuota = Math.max(0, Math.min(10, currentQuota + delta)); // 0-10 range
        
        console.log(`Quota change: ${currentQuota} -> ${newQuota}`);
        
        // Update local state
        if (this.state.profile) {
            this.state.profile.daily_count = newQuota;
        }
        
        // Update UI immediately
        this.updateQuotaDisplay();
        
        // Save to backend
        this.saveQuotaSettings(newQuota);
    }
    
    async saveQuotaSettings(dailyCount) {
        try {
            console.log(`Saving quota settings: ${dailyCount}`);
            if (window.chromancyAPI) {
                await window.chromancyAPI.updateWindowSettings({
                    daily_count: dailyCount
                });
                console.log('Quota settings saved successfully');
            } else {
                console.error('API not available for saving quota');
            }
        } catch (error) {
            console.error('Error saving quota settings:', error);
        }
    }
    
    updateQuotaDisplay() {
        const quotaCount = document.getElementById('quota-count');
        const quotaPreview = document.getElementById('quota-preview');
        
        if (quotaCount) {
            quotaCount.textContent = this.state.profile?.daily_count || 3;
        }
        
        if (quotaPreview) {
            // Calculate next ping time (simplified preview)
            const nextPingTime = this.calculateNextPingTime();
            quotaPreview.textContent = nextPingTime;
        }
    }
    
    calculateNextPingTime() {
        const now = new Date();
        const dailyCount = this.state.profile?.daily_count || 3;
        
        if (dailyCount === 0) {
            return "No pings scheduled";
        }
        
        // Simplified calculation - assume pings spread throughout window
        const windowStart = this.state.profile?.window_start || "09:00";
        const windowEnd = this.state.profile?.window_end || "21:00";
        
        const [startHour, startMin] = windowStart.split(':').map(Number);
        const [endHour, endMin] = windowEnd.split(':').map(Number);
        
        const startTime = new Date();
        startTime.setHours(startHour, startMin, 0, 0);
        
        const endTime = new Date();
        endTime.setHours(endHour, endMin, 0, 0);
        
        if (endTime <= startTime) {
            endTime.setDate(endTime.getDate() + 1);
        }
        
        // If we're before the window, show time until first ping
        if (now < startTime) {
            const diff = startTime - now;
            const hours = Math.floor(diff / (1000 * 60 * 60));
            const minutes = Math.floor((diff % (1000 * 60 * 60)) / (1000 * 60));
            return `Next ping in ${hours}h ${minutes}m`;
        }
        
        // If we're in the window, calculate next ping
        const windowDuration = endTime - startTime;
        const pingInterval = windowDuration / dailyCount;
        const nextPingTime = new Date(startTime.getTime() + pingInterval);
        
        if (nextPingTime > now) {
            const diff = nextPingTime - now;
            const hours = Math.floor(diff / (1000 * 60 * 60));
            const minutes = Math.floor((diff % (1000 * 60 * 60)) / (1000 * 60));
            return `Next ping in ${hours}h ${minutes}m`;
        }
        
        return "Next ping tomorrow";
    }
    
    // Mute Control System
    initializeMuteControls() {
        const muteButtons = document.querySelectorAll('.mute-btn');
        const unmuteBtn = document.getElementById('unmute-btn');
        
        muteButtons.forEach(btn => {
            if (btn.id !== 'unmute-btn') {
                btn.addEventListener('click', (e) => {
                    this.createRipple(e, btn);
                    const duration = btn.dataset.duration;
                    this.activateMute(duration);
                });
            }
        });
        
        if (unmuteBtn) {
            unmuteBtn.addEventListener('click', (e) => {
                this.createRipple(e, unmuteBtn);
                this.deactivateMute();
            });
        }
    }
    
    activateMute(duration) {
        const now = new Date();
        let muteUntil;
        let durationHours;
        
        switch (duration) {
            case '1':
                durationHours = 1;
                muteUntil = new Date(now.getTime() + 1 * 60 * 60 * 1000);
                break;
            case '3':
                durationHours = 3;
                muteUntil = new Date(now.getTime() + 3 * 60 * 60 * 1000);
                break;
            case 'until-9am':
                muteUntil = new Date();
                muteUntil.setHours(9, 0, 0, 0);
                if (muteUntil <= now) {
                    muteUntil.setDate(muteUntil.getDate() + 1);
                }
                durationHours = Math.ceil((muteUntil - now) / (1000 * 60 * 60));
                break;
            default:
                return;
        }
        
        // Store mute status locally and sync with backend
        localStorage.setItem('chronomancy_mute_until', muteUntil.toISOString());
        
        // Sync with backend
        this.syncMuteWithBackend(durationHours);
        
        this.updateMuteDisplay();
        
        // Haptic feedback
        if (window.Telegram?.WebApp?.HapticFeedback) {
            window.Telegram.WebApp.HapticFeedback.impactOccurred('medium');
        }
    }
    
    async syncMuteWithBackend(durationHours) {
        try {
            await window.chromancyAPI.muteUser(durationHours);
        } catch (error) {
            console.error('Error syncing mute with backend:', error);
        }
    }
    
    async deactivateMute() {
        localStorage.removeItem('chronomancy_mute_until');
        
        // Sync with backend
        try {
            await window.chromancyAPI.unmuteUser();
        } catch (error) {
            console.error('Error syncing unmute with backend:', error);
        }
        
        this.updateMuteDisplay();
        
        // Haptic feedback
        if (window.Telegram?.WebApp?.HapticFeedback) {
            window.Telegram.WebApp.HapticFeedback.impactOccurred('light');
        }
    }
    
    async checkMuteStatus() {
        // Check local storage first for responsiveness
        const localMuteUntil = localStorage.getItem('chronomancy_mute_until');
        
        if (localMuteUntil) {
            const muteDate = new Date(localMuteUntil);
            const now = new Date();
            
            if (muteDate > now) {
                this.updateMuteDisplay();
                return;
            } else {
                // Local mute expired, clean up
                localStorage.removeItem('chronomancy_mute_until');
            }
        }
        
        // Check backend status
        try {
            const muteStatus = await window.chromancyAPI.getMuteStatus();
            
            if (muteStatus.is_muted) {
                // Sync local storage with backend
                localStorage.setItem('chronomancy_mute_until', muteStatus.muted_until);
                this.updateMuteDisplay();
            } else {
                this.updateMuteDisplay();
            }
        } catch (error) {
            console.error('Error checking mute status:', error);
            this.updateMuteDisplay();
        }
    }
    
    updateMuteDisplay() {
        const muteStatus = document.getElementById('mute-status');
        const unmuteBtn = document.getElementById('unmute-btn');
        const muteButtons = document.querySelectorAll('.mute-btn:not(#unmute-btn)');
        
        const muteUntil = localStorage.getItem('chronomancy_mute_until');
        
        if (muteUntil) {
            const muteDate = new Date(muteUntil);
            const now = new Date();
            
            if (muteDate > now) {
                const diff = muteDate - now;
                const hours = Math.floor(diff / (1000 * 60 * 60));
                const minutes = Math.floor((diff % (1000 * 60 * 60)) / (1000 * 60));
                
                if (muteStatus) {
                    muteStatus.textContent = `Muted for ${hours}h ${minutes}m`;
                }
                
                if (unmuteBtn) {
                    unmuteBtn.style.display = 'block';
                }
                
                muteButtons.forEach(btn => {
                    btn.style.opacity = '0.5';
                    btn.style.pointerEvents = 'none';
                });
                
                return;
            }
        }
        
        // Not muted
        if (muteStatus) {
            muteStatus.textContent = '';
        }
        
        if (unmuteBtn) {
            unmuteBtn.style.display = 'none';
        }
        
        muteButtons.forEach(btn => {
            btn.style.opacity = '1';
            btn.style.pointerEvents = 'auto';
        });
    }

    // Profile Panel Data Loading
    async loadProfileData() {
        const profileContent = document.getElementById('profile-content');
        
        try {
            const profile = await window.chromancyAPI.getUserProfile();
            
            profileContent.innerHTML = `
                <div class="user-card">
                    <h3>🌀 Temporal Profile</h3>
                    <div class="user-stats">
                        <div class="stat-item">
                            <div class="stat-value">${profile.future_message_count}</div>
                            <div class="stat-label">Future Messages</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-value">${profile.anomaly_count}</div>
                            <div class="stat-label">Anomalies Reported</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-value">${profile.daily_count}</div>
                            <div class="stat-label">Daily Pings</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-value">${profile.ping_count || 0}</div>
                            <div class="stat-label">Total Pings</div>
                        </div>
                    </div>
                </div>
                
                <div class="user-card">
                    <h3>⚙️ Current Settings</h3>
                    <div class="form-group">
                        <label class="form-label">Temporal Window</label>
                        <div class="status-value">${profile.window_start || '09:00'} - ${profile.window_end || '21:00'}</div>
                    </div>
                    <div class="form-group">
                        <label class="form-label">Daily Count</label>
                        <div class="status-value">${profile.daily_count || 3} pings</div>
                    </div>
                    <div class="form-group">
                        <label class="form-label">Timezone</label>
                        <div class="status-value">${profile.timezone_offset || 'UTC'}</div>
                    </div>
                </div>
                
                <button class="button" onclick="window.chronApp.exportUserData()">
                    📊 Export My Data
                </button>
            `;
            
        } catch (error) {
            console.error('Error loading profile:', error);
            profileContent.innerHTML = '<div class="loading">Failed to load profile data</div>';
        }
    }

    // Anomaly Reporting
    async reportAnomaly() {
        console.log('Reporting anomaly...');
        
        const description = document.getElementById('anomaly-description')?.value;
        const type = document.getElementById('anomaly-type')?.value;
        
        if (!description || !type) {
            this.showNotification('Please fill in all fields');
            return;
        }
        
        try {
            if (window.chromancyAPI) {
                await window.chromancyAPI.reportAnomaly({
                    description: description,
                    type: type,
                    timestamp: new Date().toISOString()
                });
                
                this.showNotification('Anomaly reported successfully! ✨');
                
                // Clear form
                document.getElementById('anomaly-description').value = '';
                document.getElementById('anomaly-type').value = 'synchronistic';
                
                // Close panel
                this.closePanel('anomaly-panel');
                
                // Trigger visual effect
                if (this.scene) {
                    this.scene.triggerAnomalyEffect();
                }
            } else {
                console.error('API not available for reporting anomaly');
                this.showNotification('Error: API not available');
            }
        } catch (error) {
            console.error('Error reporting anomaly:', error);
            this.showNotification('Error reporting anomaly');
        }
    }

    // Notification System
    showNotification(message) {
        console.log('Showing notification:', message);
        
        // Create notification element if it doesn't exist
        let notification = document.getElementById('app-notification');
        if (!notification) {
            notification = document.createElement('div');
            notification.id = 'app-notification';
            notification.className = 'notification';
            document.body.appendChild(notification);
        }
        
        notification.textContent = message;
        notification.classList.add('show');
        
        // Hide after 3 seconds
        setTimeout(() => {
            notification.classList.remove('show');
        }, 3000);
    }
}

// Initialize the app when page loads
console.log('Initializing Chronomancy App...');
window.chronApp = new ChromancyApp();

// Global functions for backward compatibility
window.showPanel = (panelId) => {
    if (window.chronApp) {
        window.chronApp.showPanel(panelId);
    }
};

window.closePanel = (panelId) => {
    if (window.chronApp) {
        window.chronApp.closePanel(panelId);
    }
};

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (window.chronApp?.scene) {
        window.chronApp.scene.cleanup();
    }
}); 