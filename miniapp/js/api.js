/**
 * Chronomancy API Client
 * 
 * Enhanced interface following Scott Wilber's temporal exploration framework
 * for comprehensive anomaly detection and pattern analysis capabilities.
 */

class ChromomancyAPI {
    constructor(baseUrl = '') {
        this.baseUrl = baseUrl;
        this.userId = null;
    }

    setUserId(userId) {
        this.userId = userId;
    }

    async request(endpoint, options = {}) {
        const url = `${this.baseUrl}${endpoint}`;
        const response = await fetch(url, {
            headers: {
                'Content-Type': 'application/json',
                ...options.headers
            },
            ...options
        });

        if (!response.ok) {
            throw new Error(`API Error: ${response.status} ${response.statusText}`);
        }

        return response.json();
    }

    // User Settings
    async getUserSettings(userId = this.userId) {
        return this.request(`/api/user/${userId}/settings`);
    }

    async updateUserSettings(settings, userId = this.userId) {
        return this.request(`/api/user/${userId}/settings`, {
            method: 'POST',
            body: JSON.stringify(settings)
        });
    }

    // User Profile & Statistics
    async getUserProfile(userId = this.userId) {
        return this.request(`/api/user/${userId}/profile`);
    }

    async getUserReports(userId = this.userId, limit = 10) {
        return this.request(`/api/user/${userId}/reports?limit=${limit}`);
    }

    async getUserActivity(userId = this.userId) {
        return this.request(`/api/user/${userId}/activity`);
    }

    async exportUserData(userId = this.userId) {
        const response = await fetch(`${this.baseUrl}/api/user/${userId}/export`);
        if (!response.ok) {
            throw new Error(`Export Error: ${response.status} ${response.statusText}`);
        }
        return response.blob();
    }

    // Future Messages Management
    async getFutureMessages(userId = this.userId) {
        return this.request(`/api/user/${userId}/future-messages`);
    }

    async addFutureMessage(text, userId = this.userId) {
        return this.request(`/api/user/${userId}/future-messages`, {
            method: 'POST',
            body: JSON.stringify({ text })
        });
    }

    async deleteFutureMessage(messageId, userId = this.userId) {
        return this.request(`/api/user/${userId}/future-messages/${messageId}`, {
            method: 'DELETE'
        });
    }

    // Alarm Window Configuration
    async getWindowSettings(userId = this.userId) {
        return this.request(`/api/user/${userId}/window`);
    }

    async updateWindowSettings(settings, userId = this.userId) {
        return this.request(`/api/user/${userId}/window`, {
            method: 'POST',
            body: JSON.stringify(settings)
        });
    }

    // Global Network Statistics
    async getGlobalStats() {
        return this.request('/api/global/stats');
    }

    // Global Synchronization
    async getGlobalSync() {
        return this.request('/api/global-sync');
    }

    // Challenges & Prompts
    async getChallenges() {
        return this.request('/api/challenges');
    }

    async getRandomChallenge() {
        return this.request('/api/challenge');
    }

    // Anomaly Reporting
    async reportAnomaly(anomaly, userId = this.userId) {
        return this.request(`/api/user/${userId}/anomaly`, {
            method: 'POST',
            body: JSON.stringify(anomaly)
        });
    }

    async getUserAnomalies(userId = this.userId, limit = 50) {
        return this.request(`/api/user/${userId}/anomalies?limit=${limit}`);
    }

    // App Statistics
    async getAppStats() {
        return this.request('/api/stats');
    }

    // === Temporal RNG / Entropy ===
    /**
     * Fetch PCQNG entropy bytes from the backend.
     * @param {number} count - Number of bytes to request (max 512).
     * @returns {Promise<Uint8Array>} Raw entropy bytes.
     * Scott Wilber justification: consumed directly without additional
     * whitening so that bias-analysis tooling can inspect raw stream.
     */
    async getEntropy(count = 32) {
        const response = await fetch(`${this.baseUrl}/api/entropy?count=${count}`);
        if (!response.ok) {
            throw new Error(`Entropy Error: ${response.status} ${response.statusText}`);
        }
        const buffer = await response.arrayBuffer();
        return new Uint8Array(buffer);
    }

    // Mute/Unmute Controls
    async muteUser(duration_hours) {
        const response = await fetch(`${this.baseUrl}/api/user/${this.userId}/mute`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ duration_hours })
        });
        return this.handleResponse(response);
    }

    async unmuteUser() {
        const response = await fetch(`${this.baseUrl}/api/user/${this.userId}/unmute`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' }
        });
        return this.handleResponse(response);
    }

    async getMuteStatus() {
        const response = await fetch(`${this.baseUrl}/api/user/${this.userId}/mute-status`);
        return this.handleResponse(response);
    }
}

// Global API instance
window.chromancyAPI = new ChromomancyAPI(); 