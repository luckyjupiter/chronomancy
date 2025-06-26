// Refined Ping Console - Information Design Focused
class PingConsoleApp {
  constructor() {
    this.settings = {
      startTime: "08:00",
      endTime: "20:00",
      pingsPerDay: 3,
      muted: false,
    }
    this.wallet = {
      address: null,
      balance: 150,
      hasFauceted: false,
    }
    this.chainWalk = {
      currentPrice: 1.2847,
      priceHistory: [],
      selectedDirection: null,
      betAmount: 10,
      bets: [],
    }
    this.init()
  }

  init() {
    this.updateClock()
    this.loadSettings()
    this.loadWalletData()
    this.updateCalculatedValues()
    this.initChainWalk()

    // Update clock every second
    setInterval(() => this.updateClock(), 1000)

    // Update price every 5 seconds (demo)
    setInterval(() => this.updatePrice(), 5000)

    // Update calculated values when inputs change
    document.getElementById("start-time").addEventListener("change", () => this.updateCalculatedValues())
    document.getElementById("end-time").addEventListener("change", () => this.updateCalculatedValues())

    // Close modals when clicking outside
    document.addEventListener("click", (e) => {
      if (e.target.classList.contains("modal")) {
        this.hideAllModals()
      }
    })

    // Keyboard navigation
    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape") {
        this.hideAllModals()
      }
    })
  }

  initChainWalk() {
    // Generate initial price history
    this.chainWalk.priceHistory = this.generatePriceHistory(24)
    this.updatePriceChart()
    this.updateChainWalkDisplay()
  }

  generatePriceHistory(points) {
    const history = []
    let price = 1.25

    for (let i = 0; i < points; i++) {
      // Random walk with slight upward bias
      const change = (Math.random() - 0.48) * 0.02
      price = Math.max(0.5, Math.min(2.0, price + change))
      history.push(price)
    }

    return history
  }

  updatePrice() {
    // Simulate price movement
    const change = (Math.random() - 0.5) * 0.01
    this.chainWalk.currentPrice = Math.max(0.5, Math.min(2.0, this.chainWalk.currentPrice + change))

    // Add to history
    this.chainWalk.priceHistory.push(this.chainWalk.currentPrice)
    if (this.chainWalk.priceHistory.length > 24) {
      this.chainWalk.priceHistory.shift()
    }

    this.updatePriceChart()
    this.updateChainWalkDisplay()
  }

  updatePriceChart() {
    const history = this.chainWalk.priceHistory
    if (history.length < 2) return

    const minPrice = Math.min(...history)
    const maxPrice = Math.max(...history)
    const priceRange = maxPrice - minPrice || 0.01

    // Generate SVG points
    const points = history
      .map((price, index) => {
        const x = (index / (history.length - 1)) * 380 + 10
        const y = 110 - ((price - minPrice) / priceRange) * 100
        return `${x},${y}`
      })
      .join(" ")

    // Update the price line
    const priceLine = document.getElementById("price-line")
    if (priceLine) {
      priceLine.setAttribute("points", points)
    }

    // Update current point
    const currentPoint = document.getElementById("current-point")
    if (currentPoint && history.length > 0) {
      const lastY = 110 - ((history[history.length - 1] - minPrice) / priceRange) * 100
      currentPoint.setAttribute("cy", lastY)
    }
  }

  updateChainWalkDisplay() {
    const priceEl = document.getElementById("current-price")
    const changeEl = document.getElementById("price-change")
    const balanceEl = document.getElementById("available-balance")

    if (priceEl) {
      priceEl.textContent = this.chainWalk.currentPrice.toFixed(4)
    }

    if (changeEl && this.chainWalk.priceHistory.length >= 2) {
      const history = this.chainWalk.priceHistory
      const change = history[history.length - 1] - history[history.length - 2]
      changeEl.textContent = (change >= 0 ? "+" : "") + change.toFixed(4)
      changeEl.className = change >= 0 ? "price-change" : "price-change negative"
    }

    if (balanceEl) {
      balanceEl.textContent = this.wallet.balance
    }

    this.updateBetSummary()
  }

  selectDirection(direction) {
    this.chainWalk.selectedDirection = direction

    // Update button states
    document.querySelectorAll(".direction-btn").forEach((btn) => {
      btn.classList.remove("selected")
    })

    const selectedBtn = document.getElementById(`bet-${direction}`)
    if (selectedBtn) {
      selectedBtn.classList.add("selected")
    }

    this.updateBetSummary()
    this.updatePlaceBetButton()
  }

  setStake(amount) {
    const stakeInput = document.getElementById("bet-amount")
    if (stakeInput) {
      stakeInput.value = Math.min(amount, this.wallet.balance)
      this.chainWalk.betAmount = Number.parseInt(stakeInput.value)
      this.updateBetSummary()
    }
  }

  updateBetSummary() {
    const directionEl = document.getElementById("selected-direction")
    const returnEl = document.getElementById("potential-return")
    const stakeInput = document.getElementById("bet-amount")

    if (stakeInput) {
      this.chainWalk.betAmount = Number.parseInt(stakeInput.value) || 0
    }

    if (directionEl) {
      if (this.chainWalk.selectedDirection) {
        directionEl.textContent = this.chainWalk.selectedDirection === "up" ? "↑ Up" : "↓ Down"
        directionEl.className = `summary-value ${this.chainWalk.selectedDirection}`
      } else {
        directionEl.textContent = "Select direction"
        directionEl.className = "summary-value"
      }
    }

    if (returnEl) {
      if (this.chainWalk.selectedDirection && this.chainWalk.betAmount > 0) {
        // Simple 1.8x multiplier for demo
        const potentialReturn = Math.floor(this.chainWalk.betAmount * 1.8)
        returnEl.textContent = `${potentialReturn} CHR`
      } else {
        returnEl.textContent = "—"
      }
    }
  }

  updatePlaceBetButton() {
    const placeBetBtn = document.getElementById("place-bet-btn")
    if (placeBetBtn) {
      const canBet =
        this.chainWalk.selectedDirection &&
        this.chainWalk.betAmount > 0 &&
        this.chainWalk.betAmount <= this.wallet.balance

      placeBetBtn.disabled = !canBet
      placeBetBtn.textContent = canBet
        ? "Place Bet"
        : !this.chainWalk.selectedDirection
          ? "Select Direction"
          : this.chainWalk.betAmount > this.wallet.balance
            ? "Insufficient Balance"
            : "Enter Stake Amount"
    }
  }

  async placeBet() {
    if (!this.chainWalk.selectedDirection || this.chainWalk.betAmount <= 0) {
      this.showToast("Please select direction and enter stake amount")
      return
    }

    if (this.chainWalk.betAmount > this.wallet.balance) {
      this.showToast("Insufficient balance")
      return
    }

    try {
      // Deduct stake from balance immediately
      this.wallet.balance -= this.chainWalk.betAmount
      this.updateWalletDisplay()
      this.updateChainWalkDisplay()

      // Create bet entry
      const bet = {
        id: Date.now(),
        time: new Date().toLocaleTimeString("en-US", { hour12: false, hour: "2-digit", minute: "2-digit" }),
        direction: this.chainWalk.selectedDirection,
        stake: this.chainWalk.betAmount,
        entryPrice: this.chainWalk.currentPrice,
        status: "pending",
      }

      this.chainWalk.bets.unshift(bet)
      this.updateBetHistory()

      // API call would go here
      const userId = this.getTelegramUserId()
      await this.apiCall("/api/bet", {
        method: "POST",
        body: {
          user_id: userId,
          direction: this.chainWalk.selectedDirection,
          stake: this.chainWalk.betAmount,
        },
      })

      this.showToast(`Bet placed: ${this.chainWalk.betAmount} CHR on ${this.chainWalk.selectedDirection}`)

      // Reset form
      this.chainWalk.selectedDirection = null
      this.chainWalk.betAmount = 10
      document.querySelectorAll(".direction-btn").forEach((btn) => btn.classList.remove("selected"))
      document.getElementById("bet-amount").value = 10
      this.updateBetSummary()
      this.updatePlaceBetButton()

      // Simulate bet resolution after 30 seconds (for demo)
      setTimeout(() => this.resolveBet(bet.id), 30000)
    } catch (error) {
      console.error("Failed to place bet:", error)
      // Refund on error
      this.wallet.balance += this.chainWalk.betAmount
      this.updateWalletDisplay()
      this.showToast("Failed to place bet")
    }
  }

  resolveBet(betId) {
    const bet = this.chainWalk.bets.find((b) => b.id === betId)
    if (!bet || bet.status !== "pending") return

    // Simple resolution logic for demo
    const priceChange = this.chainWalk.currentPrice - bet.entryPrice
    const won = (bet.direction === "up" && priceChange > 0) || (bet.direction === "down" && priceChange < 0)

    bet.status = won ? "win" : "loss"

    if (won) {
      const payout = Math.floor(bet.stake * 1.8)
      bet.payout = payout
      this.wallet.balance += payout
      this.updateWalletDisplay()
      this.showToast(`Bet won! +${payout} CHR`)
    } else {
      bet.payout = 0
      this.showToast(`Bet lost: -${bet.stake} CHR`)
    }

    this.updateBetHistory()
  }

  updateBetHistory() {
    const betList = document.getElementById("bet-list")
    if (!betList) return

    const headerHTML = `
      <div class="bet-list-header">
        <span class="col-time">Time</span>
        <span class="col-direction">Direction</span>
        <span class="col-stake">Stake</span>
        <span class="col-result">Result</span>
      </div>
    `

    const betsHTML = this.chainWalk.bets
      .slice(0, 10)
      .map((bet) => {
        let resultText = "Pending"
        let resultClass = "pending"

        if (bet.status === "win") {
          resultText = `+${bet.payout} CHR`
          resultClass = "win"
        } else if (bet.status === "loss") {
          resultText = `-${bet.stake} CHR`
          resultClass = "loss"
        }

        return `
        <div class="bet-entry ${bet.status}">
          <span class="bet-time">${bet.time}</span>
          <span class="bet-direction ${bet.direction}">
            ${bet.direction === "up" ? "↑ Up" : "↓ Down"}
          </span>
          <span class="bet-stake">${bet.stake} CHR</span>
          <span class="bet-result ${resultClass}">${resultText}</span>
        </div>
      `
      })
      .join("")

    betList.innerHTML = headerHTML + (betsHTML || '<div class="empty-state">No bets yet</div>')
  }

  updateClock() {
    const now = new Date()
    const timeString = now.toLocaleTimeString("en-US", {
      hour12: false,
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit",
    })
    const shortTime = now.toLocaleTimeString("en-US", {
      hour12: false,
      hour: "2-digit",
      minute: "2-digit",
    })

    const mainClock = document.getElementById("main-clock")
    const trayClock = document.getElementById("tray-time")

    if (mainClock) {
      mainClock.textContent = timeString
      mainClock.setAttribute("datetime", now.toISOString())
    }
    if (trayClock) trayClock.textContent = shortTime
  }

  updateCalculatedValues() {
    // Update window duration
    const startTime = document.getElementById("start-time").value
    const endTime = document.getElementById("end-time").value
    const duration = this.calculateDuration(startTime, endTime)
    document.getElementById("window-duration").textContent = `${duration} hours active`

    // Update frequency interval
    const pingsPerDay = Number.parseInt(document.getElementById("ping-count").textContent)
    const interval = Math.round((duration / pingsPerDay) * 10) / 10
    document.getElementById("frequency-interval").textContent = `Every ${interval} hours`
  }

  calculateDuration(start, end) {
    const [startHour, startMin] = start.split(":").map(Number)
    const [endHour, endMin] = end.split(":").map(Number)

    let duration = endHour * 60 + endMin - (startHour * 60 + startMin)
    if (duration < 0) duration += 24 * 60 // Handle overnight windows

    return Math.round((duration / 60) * 10) / 10
  }

  loadSettings() {
    // Fetch persisted settings from backend then populate inputs
    (async () => {
      try {
        const userId = this.getTelegramUserId()
        const resp = await this.apiCall(`/api/user/${userId}/window`)

        if (resp.ok) {
          const data = await resp.json()
          // Overwrite defaults only if values present
          this.settings.startTime = data.window_start || this.settings.startTime
          this.settings.endTime = data.window_end || this.settings.endTime
          this.settings.pingsPerDay = data.daily_count || this.settings.pingsPerDay

          // A mute is active if muted_until is in the future
          if (data.muted_until) {
            const remaining = new Date(data.muted_until) - new Date()
            this.settings.muted = remaining > 0
          }
        }
      } catch (err) {
        console.warn("Could not load settings from API, falling back to defaults", err)
      }

      // Update form controls regardless
      const startTimeEl = document.getElementById("start-time")
      const endTimeEl = document.getElementById("end-time")
      const pingCountEl = document.getElementById("ping-count")
      const muteToggleEl = document.getElementById("mute-toggle")

      if (startTimeEl) startTimeEl.value = this.settings.startTime
      if (endTimeEl) endTimeEl.value = this.settings.endTime
      if (pingCountEl) pingCountEl.textContent = this.settings.pingsPerDay
      if (muteToggleEl) muteToggleEl.checked = this.settings.muted

      // Recompute calculated durations/intervals after loading
      this.updateCalculatedValues()
    })()
  }

  async loadWalletData() {
    try {
      const userId = this.getTelegramUserId()

      // Load wallet info
      const walletResponse = await this.apiCall(`/api/user/${userId}/wallet`)
      if (walletResponse.ok) {
        const walletData = await walletResponse.json()
        this.wallet = { ...this.wallet, ...walletData }
        this.updateWalletDisplay()
      }

      // Load transactions
      this.loadTransactions()

      // Load leaderboard
      this.loadLeaderboard()
    } catch (error) {
      console.error("Failed to load wallet data:", error)
      this.updateStatus("Failed to load wallet", "error")
    }
  }

  updateWalletDisplay() {
    const balanceEl = document.getElementById("chr-balance")
    const addressEl = document.getElementById("wallet-address")
    const faucetBtn = document.getElementById("faucet-btn")

    if (balanceEl) balanceEl.textContent = this.wallet.balance || 0
    if (addressEl) addressEl.textContent = this.wallet.address || "CHR1234567890abcdef"

    if (faucetBtn && this.wallet.hasFauceted) {
      faucetBtn.textContent = "Faucet Used"
      faucetBtn.disabled = true
    }
  }

  async loadTransactions() {
    // Placeholder for transaction loading
    const transactionList = document.getElementById("transaction-list")
    if (transactionList) {
      // This would load from /api/user/{id}/transactions
      transactionList.innerHTML = `
        <div class="list-header">
          <span>Recent Transfers</span>
        </div>
        <div class="empty-state">
          No transactions yet
        </div>
      `
    }
  }

  async loadLeaderboard() {
    try {
      const response = await this.apiCall("/api/leaderboard")
      if (response.ok) {
        const leaderboard = await response.json()
        this.updateLeaderboardDisplay(leaderboard)
      }
    } catch (error) {
      console.error("Failed to load leaderboard:", error)
    }
  }

  updateLeaderboardDisplay(leaderboard) {
    const leaderboardList = document.getElementById("leaderboard-list")
    if (leaderboardList && leaderboard.length > 0) {
      const listHTML = `
        <div class="list-header">
          <span>CHR Rich List</span>
        </div>
        ${leaderboard
          .slice(0, 10)
          .map(
            (entry, index) => `
          <div class="leaderboard-entry">
            <span class="rank">${index + 1}.</span>
            <span class="nickname">${entry.nickname || "Anonymous"}</span>
            <span class="balance">${entry.balance} CHR</span>
          </div>
        `,
          )
          .join("")}
      `
      leaderboardList.innerHTML = listHTML
    }
  }

  getTelegramUserId() {
    if (window.Telegram && window.Telegram.WebApp) {
      return window.Telegram.WebApp.initDataUnsafe?.user?.id
    }
    return localStorage.getItem("demo_user_id") || "demo_user"
  }

  async apiCall(endpoint, options = {}) {
    const defaultHeaders = { "Content-Type": "application/json" }
    const opts = {
      credentials: "include",
      headers: { ...defaultHeaders, ...(options.headers || {}) },
      ...options,
    }

    // Automatically JSON stringify body objects
    if (opts.body && typeof opts.body === "object" && !(opts.body instanceof FormData)) {
      opts.body = JSON.stringify(opts.body)
    }

    const url = `${endpoint}` // same origin relative path
    return fetch(url, opts)
  }

  adjustPings(delta) {
    const pingCountEl = document.getElementById("ping-count")
    let current = Number.parseInt(pingCountEl.textContent)
    current = Math.max(1, Math.min(10, current + delta))
    pingCountEl.textContent = current
    this.updateCalculatedValues()
  }

  async saveSettings() {
    try {
      const startTime = document.getElementById("start-time").value
      const endTime = document.getElementById("end-time").value
      const pingsPerDay = Number.parseInt(document.getElementById("ping-count").textContent)
      const muted = document.getElementById("mute-toggle").checked

      this.settings = { startTime, endTime, pingsPerDay, muted }

      const userId = this.getTelegramUserId()
      const resp = await this.apiCall(`/api/user/${userId}/window`, {
        method: "POST",
        body: {
          window_start: startTime,
          window_end: endTime,
          daily_count: pingsPerDay,
          muted_until: muted ? new Date(Date.now() + 24 * 60 * 60 * 1000).toISOString() : null,
        },
      })

      if (resp.ok) {
              this.updateStatus("Temporal scanning activated", "success")
      this.showToast("Scanning activated! You'll receive anomaly pings within your window.")

        // Send confirmation ping via bot
        await this.apiCall(`/api/user/${userId}/test-ping`, { method: "POST" })
      } else {
        this.updateStatus("Save failed", "error")
        this.showToast("Failed to save configuration")
      }
    } catch (error) {
      console.error("Failed to save settings:", error)
      this.updateStatus("Save failed", "error")
      this.showToast("Failed to save configuration")
    }
  }

  async requestFaucet() {
    try {
      const userId = this.getTelegramUserId()
      const response = await this.apiCall(`/api/user/${userId}/faucet`, { method: "POST" })

      if (response.ok) {
        this.wallet.balance += 100
        this.wallet.hasFauceted = true
        this.updateWalletDisplay()
        this.updateChainWalkDisplay()
        this.showToast("Received 100 CHR from faucet!")
      }
    } catch (error) {
      console.error("Faucet request failed:", error)
      this.showToast("Faucet request failed")
    }
  }

  showTransferDialog() {
    this.showWindow("transfer-dialog")
  }

  async executeTransfer() {
    try {
      const toUserId = document.getElementById("transfer-to").value
      const amount = Number.parseInt(document.getElementById("transfer-amount").value)

      if (!toUserId || !amount || amount <= 0) {
        this.showToast("Please enter valid recipient and amount")
        return
      }

      if (amount > this.wallet.balance) {
        this.showToast("Insufficient balance")
        return
      }

      const userId = this.getTelegramUserId()
      const response = await this.apiCall("/api/transfer", {
        method: "POST",
        body: {
          from_id: userId,
          to_id: toUserId,
          amount: amount,
        },
      })

      if (response.ok) {
        this.wallet.balance -= amount
        this.updateWalletDisplay()
        this.updateChainWalkDisplay()
        this.hideWindow("transfer-dialog")
        this.showToast(`Transferred ${amount} CHR successfully`)
        this.loadTransactions() // Refresh transaction list
      }
    } catch (error) {
      console.error("Transfer failed:", error)
      this.showToast("Transfer failed")
    }
  }

  showWindow(windowId) {
    this.hideAllModals()

    // Update taskbar
    this.updateTaskbar(windowId)

    const window = document.getElementById(windowId)
    if (window) {
      window.classList.remove("hidden")
      // Focus first interactive element
      const firstInput = window.querySelector("button, input, select, textarea")
      if (firstInput) firstInput.focus()
    }
  }

  hideWindow(windowId) {
    const window = document.getElementById(windowId)
    if (window) {
      window.classList.add("hidden")
    }
    this.updateTaskbar()
  }

  hideAllModals() {
    document.querySelectorAll(".modal").forEach((modal) => {
      modal.classList.add("hidden")
    })
    document.querySelectorAll(".window").forEach((window) => {
      window.classList.add("hidden")
    })
    // Show ping console by default
    document.getElementById("ping-console").classList.remove("hidden")
  }

  updateTaskbar(activeWindow = "ping-console") {
    document.querySelectorAll(".taskbar-btn").forEach((btn) => {
      btn.classList.remove("active")
    })

    // Add active class to current window button
    const activeBtn = document.querySelector(`[onclick="showWindow('${activeWindow}')"]`)
    if (activeBtn) {
      activeBtn.classList.add("active")
    }
  }

  toggleStartMenu() {
    const menu = document.getElementById("start-menu")
    menu.classList.toggle("hidden")
  }

  showWalletTab(tabName) {
    // Hide all tabs
    document.querySelectorAll("#wallet .tab-content").forEach((tab) => {
      tab.classList.add("hidden")
    })
    document.querySelectorAll("#wallet .tab-btn").forEach((btn) => {
      btn.classList.remove("active")
    })

    // Show target tab
    document.getElementById(`${tabName}-tab`).classList.remove("hidden")
    event.target.classList.add("active")
  }

  openTelegram() {
    const telegramUrl = "https://t.me/lucidsurrealism"
    if (window.Telegram && window.Telegram.WebApp) {
      window.Telegram.WebApp.openTelegramLink(telegramUrl)
    } else {
      window.open(telegramUrl, "_blank")
    }
    this.hideWindow("field-report")
  }

  runDiagnostics() {
    this.showToast("Diagnostics placeholder – all systems nominal")
  }

  updateStatus(message, type = "info") {
    const statusEl = document.getElementById("status-text")
    if (statusEl) {
      statusEl.textContent = message
      statusEl.className = `status-${type}`
    }
  }

  showToast(message) {
    const container = document.getElementById("toast-container")
    const toast = document.createElement("div")
    toast.className = "toast"
    toast.textContent = message
    toast.setAttribute("role", "alert")

    container.appendChild(toast)

    setTimeout(() => {
      toast.remove()
    }, 3000)
  }
}

// Global functions for HTML onclick handlers
let app

window.adjustPings = (delta) => app.adjustPings(delta)
window.saveSettings = () => app.saveSettings()
window.showWindow = (windowId) => app.showWindow(windowId)
window.hideWindow = (windowId) => app.hideWindow(windowId)
window.toggleStartMenu = () => app.toggleStartMenu()
window.showWalletTab = (tabName) => app.showWalletTab(tabName)
window.requestFaucet = () => app.requestFaucet()
window.showTransferDialog = () => app.showTransferDialog()
window.executeTransfer = () => app.executeTransfer()
window.selectDirection = (direction) => app.selectDirection(direction)
window.setStake = (amount) => app.setStake(amount)
window.placeBet = () => app.placeBet()
window.openTelegram = () => app.openTelegram()
window.runDiagnostics = () => app.runDiagnostics()

// Initialize app when DOM is loaded
document.addEventListener("DOMContentLoaded", () => {
  app = new PingConsoleApp()
})
