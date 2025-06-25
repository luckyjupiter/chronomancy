// Telegram Mini App Initialization
// Handles Telegram WebApp integration and theme management

class TelegramApp {
    constructor() {
        this.webApp = null;
        this.user = null;
        this.theme = null;
        this.isReady = false;
        
        this.init();
    }
    
    init() {
        // Check if running in Telegram
        if (typeof window.Telegram === 'undefined' || !window.Telegram.WebApp) {
            console.warn('Not running in Telegram Mini App environment');
            this.mockTelegramForDevelopment();
            return;
        }
        
        this.webApp = window.Telegram.WebApp;
        
        // Initialize Telegram WebApp
        this.setupTelegramListeners();
        this.updateTheme();
        this.getUserInfo();
        
        // Ready to show
        this.webApp.ready();
        this.webApp.expand();
        
        // Enable haptic feedback for better UX
        this.enableHapticFeedback();
        
        this.isReady = true;
        console.log('Telegram Mini App initialized', {
            version: this.webApp.version,
            platform: this.webApp.platform,
            user: this.user
        });
    }
    
    setupTelegramListeners() {
        // Theme change listener
        this.webApp.onEvent('themeChanged', () => {
            this.updateTheme();
        });
        
        // Viewport change listener
        this.webApp.onEvent('viewportChanged', (params) => {
            console.log('Viewport changed:', params);
            // Could trigger Three.js canvas resize here
            window.dispatchEvent(new Event('resize'));
        });
        
        // Back button handler
        this.webApp.BackButton.onClick(() => {
            // Close any open panels first
            const panels = document.querySelectorAll('.floating-panel.visible');
            if (panels.length > 0) {
                panels.forEach(panel => panel.classList.remove('visible'));
            } else {
                this.webApp.close();
            }
        });
        
        // Main button setup (for future use)
        this.webApp.MainButton.setText('Report Anomaly');
        this.webApp.MainButton.onClick(() => {
            window.showPanel('anomaly-panel');
        });
    }
    
    updateTheme() {
        this.theme = this.webApp.themeParams;
        
        // Apply Telegram theme to CSS variables
        const root = document.documentElement;
        
        if (this.theme.bg_color) {
            root.style.setProperty('--tg-theme-bg-color', this.theme.bg_color);
            
            // Convert hex to RGB for backdrop filters
            const rgb = this.hexToRgb(this.theme.bg_color);
            if (rgb) {
                root.style.setProperty('--tg-theme-bg-color-rgb', `${rgb.r}, ${rgb.g}, ${rgb.b}`);
            }
        }
        
        if (this.theme.text_color) {
            root.style.setProperty('--tg-theme-text-color', this.theme.text_color);
        }
        
        if (this.theme.hint_color) {
            root.style.setProperty('--tg-theme-hint-color', this.theme.hint_color);
        }
        
        if (this.theme.button_color) {
            root.style.setProperty('--tg-theme-button-color', this.theme.button_color);
        }
        
        if (this.theme.button_text_color) {
            root.style.setProperty('--tg-theme-button-text-color', this.theme.button_text_color);
        }
        
        // Update header and background colors
        if (this.webApp.headerColor) {
            this.webApp.setHeaderColor(this.theme.bg_color || '#ffffff');
        }
        
        if (this.webApp.backgroundColor) {
            this.webApp.setBackgroundColor(this.theme.bg_color || '#ffffff');
        }
        
        // Notify Three.js scene about theme change
        window.dispatchEvent(new CustomEvent('themeChanged', { 
            detail: { 
                theme: this.theme,
                isDark: this.webApp.colorScheme === 'dark'
            }
        }));
    }
    
    getUserInfo() {
        const initData = this.webApp.initDataUnsafe;
        
        if (initData && initData.user) {
            this.user = {
                id: initData.user.id,
                firstName: initData.user.first_name,
                lastName: initData.user.last_name,
                username: initData.user.username,
                languageCode: initData.user.language_code,
                isPremium: initData.user.is_premium || false
            };
        }
        
        // Store for API calls
        window.telegramUser = this.user;
    }
    
    enableHapticFeedback() {
        if (!this.webApp.HapticFeedback) return;
        
        // Add haptic feedback to buttons
        document.addEventListener('click', (e) => {
            if (e.target.classList.contains('button')) {
                this.webApp.HapticFeedback.impactOccurred('light');
            }
        });
    }
    
    // Utility functions
    hexToRgb(hex) {
        const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
        return result ? {
            r: parseInt(result[1], 16),
            g: parseInt(result[2], 16),
            b: parseInt(result[3], 16)
        } : null;
    }
    
    // Development mock for testing outside Telegram
    mockTelegramForDevelopment() {
        console.log('Mocking Telegram environment for development');
        
        // Create mock WebApp object
        window.Telegram = {
            WebApp: {
                ready: () => console.log('Mock: ready()'),
                expand: () => console.log('Mock: expand()'),
                close: () => console.log('Mock: close()'),
                version: '6.0',
                platform: 'web',
                colorScheme: 'light',
                themeParams: {
                    bg_color: '#ffffff',
                    text_color: '#000000',
                    hint_color: '#999999',
                    button_color: '#007AFF',
                    button_text_color: '#ffffff'
                },
                initDataUnsafe: {
                    user: {
                        id: 123456789,
                        first_name: 'Test',
                        last_name: 'User',
                        username: 'testuser',
                        language_code: 'en'
                    }
                },
                BackButton: {
                    onClick: (callback) => {
                        document.addEventListener('keydown', (e) => {
                            if (e.key === 'Escape') callback();
                        });
                    }
                },
                MainButton: {
                    setText: (text) => console.log('Mock MainButton text:', text),
                    onClick: (callback) => {
                        // Mock main button behavior
                        window.mockMainButtonCallback = callback;
                    }
                },
                HapticFeedback: {
                    impactOccurred: (type) => console.log('Mock haptic:', type)
                },
                onEvent: (event, callback) => {
                    console.log('Mock event listener:', event);
                    if (event === 'themeChanged') {
                        window.mockThemeCallback = callback;
                    }
                },
                setHeaderColor: (color) => console.log('Mock header color:', color),
                setBackgroundColor: (color) => console.log('Mock bg color:', color)
            }
        };
        
        this.webApp = window.Telegram.WebApp;
        this.updateTheme();
        this.getUserInfo();
        this.isReady = true;
    }
}

// Initialize Telegram app
window.telegramApp = new TelegramApp();

// Global utility functions
window.showMainButton = (text, callback) => {
    if (window.telegramApp.webApp.MainButton) {
        window.telegramApp.webApp.MainButton.setText(text);
        window.telegramApp.webApp.MainButton.show();
        window.telegramApp.webApp.MainButton.onClick(callback);
    }
};

window.hideMainButton = () => {
    if (window.telegramApp.webApp.MainButton) {
        window.telegramApp.webApp.MainButton.hide();
    }
};

window.showBackButton = () => {
    if (window.telegramApp.webApp.BackButton) {
        window.telegramApp.webApp.BackButton.show();
    }
};

window.hideBackButton = () => {
    if (window.telegramApp.webApp.BackButton) {
        window.telegramApp.webApp.BackButton.hide();
    }
};

window.hapticFeedback = (type = 'light') => {
    if (window.telegramApp.webApp.HapticFeedback) {
        window.telegramApp.webApp.HapticFeedback.impactOccurred(type);
    }
}; 