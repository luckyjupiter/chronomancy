"use client"

import { useState, useEffect } from "react"
import { Button } from "@/components/ui/button"
import { Card } from "@/components/ui/card"
import { Textarea } from "@/components/ui/textarea"
import { Clock, Play, Square, Settings, Menu, Minus, Plus } from "lucide-react"

interface ChromomancySession {
  active: boolean
  window_start: string
  window_end: string
  anomaly_count: number
  use_24_hour: boolean
}

export default function ChromomancyApp() {
  const [session, setSession] = useState<ChromomancySession>({
    active: false,
    window_start: "09:00",
    window_end: "17:00",
    anomaly_count: 3,
    use_24_hour: true,
  })
  const [mode, setMode] = useState<"setup" | "anomaly">("setup")
  const [observation, setObservation] = useState("")
  const [showStartMenu, setShowStartMenu] = useState(false)
  const [currentTime, setCurrentTime] = useState(new Date())

  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000)

    // Check URL params
    const urlParams = new URLSearchParams(window.location.search)
    if (urlParams.get("mode") === "anomaly") {
      setMode("anomaly")
    }

    return () => clearInterval(timer)
  }, [])

  const toggleChronomancy = () => {
    setSession((prev) => ({ ...prev, active: !prev.active }))
  }

  const submitObservation = () => {
    setObservation("")
    setMode("setup")
  }

  const formatTime = (time: Date) => {
    return session.use_24_hour
      ? time.toLocaleTimeString("en-US", { hour12: false })
      : time.toLocaleTimeString("en-US", { hour12: true })
  }

  const adjustAnomalyCount = (delta: number) => {
    setSession((prev) => ({
      ...prev,
      anomaly_count: Math.max(1, Math.min(10, prev.anomaly_count + delta)),
    }))
  }

  if (mode === "anomaly") {
    return (
      <div className="min-h-screen bg-gray-300 p-2 font-sans">
        {/* Windows 98 Title Bar */}
        <div className="bg-gradient-to-r from-blue-800 to-blue-600 text-white px-2 py-1 text-sm flex justify-between items-center mb-1">
          <span>⏰ Chronomancy - Temporal Anomaly Detected</span>
          <div className="flex gap-1">
            <div className="w-4 h-4 bg-gray-400 border border-gray-600"></div>
            <div className="w-4 h-4 bg-gray-400 border border-gray-600"></div>
            <div className="w-4 h-4 bg-red-500 border border-red-700"></div>
          </div>
        </div>

        {/* Main Window */}
        <div className="border-2 border-gray-800 bg-gray-200 p-4 h-[calc(100vh-3rem)]">
          {/* HUGE Anomaly Alert */}
          <div className="text-center mb-8">
            <div className="text-8xl mb-4 animate-pulse">⚡</div>
            <h1 className="text-4xl font-bold text-red-600 mb-2">TEMPORAL ANOMALY</h1>
            <p className="text-xl text-gray-700">Something unusual is happening around you right now</p>
          </div>

          {/* Simple Observation Box */}
          <Card className="bg-white border-2 border-gray-400 p-6 mb-6">
            <label className="block text-lg font-bold mb-3 text-gray-800">What do you observe?</label>
            <Textarea
              value={observation}
              onChange={(e) => setObservation(e.target.value)}
              placeholder="Describe anything unusual you notice right now - sounds, sights, feelings, coincidences..."
              className="text-lg min-h-[120px] border-2 border-gray-400"
              autoFocus
            />
          </Card>

          {/* HUGE Action Buttons */}
          <div className="space-y-4">
            <Button
              onClick={submitObservation}
              disabled={!observation.trim()}
              className="w-full h-16 text-2xl font-bold bg-green-600 hover:bg-green-700 text-white border-2 border-green-800"
            >
              📝 Record Observation
            </Button>

            <Button
              onClick={() => setMode("setup")}
              className="w-full h-12 text-lg bg-gray-400 hover:bg-gray-500 text-gray-800 border-2 border-gray-600"
            >
              Nothing unusual - Back to Setup
            </Button>
          </div>
        </div>
      </div>
    )
  }

  return (
    <div className="min-h-screen bg-gray-300 p-2 font-sans relative">
      {/* Windows 98 Title Bar */}
      <div className="bg-gradient-to-r from-blue-800 to-blue-600 text-white px-2 py-1 text-sm flex justify-between items-center mb-1">
        <span>⏰ Chronomancy - Temporal Field Scanner</span>
        <div className="flex gap-1">
          <div className="w-4 h-4 bg-gray-400 border border-gray-600"></div>
          <div className="w-4 h-4 bg-gray-400 border border-gray-600"></div>
          <div className="w-4 h-4 bg-red-500 border border-red-700"></div>
        </div>
      </div>

      {/* Main Window */}
      <div className="border-2 border-gray-800 bg-gray-200 p-6 h-[calc(100vh-3rem)]">
        {/* Current Time Display */}
        <div className="text-center mb-8">
          <div className="text-6xl font-mono font-bold text-gray-800 mb-2">{formatTime(currentTime)}</div>
          <p className="text-lg text-gray-600">Set your temporal scanning window</p>
        </div>

        {/* HUGE Time Window Selector */}
        <Card className="bg-white border-2 border-gray-400 p-8 mb-6">
          <div className="text-center mb-6">
            <h2 className="text-2xl font-bold text-gray-800 mb-2">Scanning Window</h2>
            <p className="text-gray-600">When should we scan for temporal anomalies?</p>
          </div>

          <div className="flex items-center justify-center gap-8 mb-6">
            <div className="text-center">
              <label className="block text-sm font-bold text-gray-700 mb-2">FROM</label>
              <input
                type="time"
                value={session.window_start}
                onChange={(e) => setSession((prev) => ({ ...prev, window_start: e.target.value }))}
                className="text-3xl font-mono p-4 border-2 border-gray-400 rounded text-center w-32"
              />
            </div>

            <div className="text-4xl text-gray-400 mt-8">→</div>

            <div className="text-center">
              <label className="block text-sm font-bold text-gray-700 mb-2">TO</label>
              <input
                type="time"
                value={session.window_end}
                onChange={(e) => setSession((prev) => ({ ...prev, window_end: e.target.value }))}
                className="text-3xl font-mono p-4 border-2 border-gray-400 rounded text-center w-32"
              />
            </div>
          </div>

          {/* Anomaly Count Control */}
          <div className="flex items-center justify-center gap-6 mb-4">
            <Button
              onClick={() => adjustAnomalyCount(-1)}
              disabled={session.anomaly_count === 1}
              className="w-12 h-12 bg-gray-100 border-2 border-gray-400 text-gray-800 hover:bg-gray-200"
            >
              <Minus className="w-5 h-5" />
            </Button>

            <div className="text-center">
              <div className="text-3xl font-bold text-gray-800">{session.anomaly_count}</div>
              <div className="text-sm text-gray-600">anomalies/window</div>
            </div>

            <Button
              onClick={() => adjustAnomalyCount(1)}
              disabled={session.anomaly_count === 10}
              className="w-12 h-12 bg-gray-100 border-2 border-gray-400 text-gray-800 hover:bg-gray-200"
            >
              <Plus className="w-5 h-5" />
            </Button>
          </div>

          {/* Time Format Toggle */}
          <div className="text-center">
            <Button
              onClick={() => setSession((prev) => ({ ...prev, use_24_hour: !prev.use_24_hour }))}
              className="bg-gray-100 border border-gray-400 text-gray-700 hover:bg-gray-200 text-sm"
            >
              {session.use_24_hour ? "24-hour" : "12-hour"} format
            </Button>
          </div>
        </Card>

        {/* Status Display */}
        <Card
          className={`border-2 p-4 mb-8 ${
            session.active ? "bg-green-100 border-green-400" : "bg-gray-100 border-gray-400"
          }`}
        >
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-3">
              <div
                className={`w-4 h-4 rounded-full ${session.active ? "bg-green-500 animate-pulse" : "bg-gray-400"}`}
              ></div>
              <span className="text-lg font-bold">
                {session.active
                  ? `Scanning active: ${session.window_start} - ${session.window_end}`
                  : "Temporal scanner inactive"}
              </span>
            </div>
            {session.active && <span className="text-sm text-gray-600">{session.anomaly_count} anomalies/day</span>}
          </div>
        </Card>

        {/* GIANT Start/Stop Button */}
        <Button
          onClick={toggleChronomancy}
          className={`w-full h-20 text-3xl font-bold border-4 ${
            session.active
              ? "bg-red-500 hover:bg-red-600 text-white border-red-700"
              : "bg-blue-500 hover:bg-blue-600 text-white border-blue-700"
          }`}
        >
          {session.active ? (
            <div className="flex items-center gap-4">
              <Square className="w-8 h-8" />
              Stop Chronomancy
            </div>
          ) : (
            <div className="flex items-center gap-4">
              <Play className="w-8 h-8" />
              Start Chronomancy
            </div>
          )}
        </Button>

        {/* Report Field Anomaly Button */}
        <Button
          onClick={() => setMode("anomaly")}
          className="w-full mt-4 h-12 text-lg bg-purple-500 hover:bg-purple-600 text-white border-2 border-purple-700"
        >
          ⚡ Report Field Anomaly
        </Button>
      </div>

      {/* Windows 98 Start Menu Button */}
      <Button
        onClick={() => setShowStartMenu(!showStartMenu)}
        className="fixed bottom-4 left-4 bg-gray-400 hover:bg-gray-500 border-2 border-gray-600 px-4 py-2"
      >
        <Menu className="w-4 h-4 mr-2" />
        Start
      </Button>

      {/* Hidden Start Menu */}
      {showStartMenu && (
        <Card className="fixed bottom-16 left-4 bg-gray-200 border-2 border-gray-800 p-2 w-48">
          <div className="space-y-1">
            <Button className="w-full justify-start bg-transparent hover:bg-blue-500 hover:text-white text-left p-2">
              <Settings className="w-4 h-4 mr-2" />
              Advanced Settings
            </Button>
            <Button className="w-full justify-start bg-transparent hover:bg-blue-500 hover:text-white text-left p-2">
              <Clock className="w-4 h-4 mr-2" />
              Temporal Field Scanner
            </Button>
            <Button className="w-full justify-start bg-transparent hover:bg-blue-500 hover:text-white text-left p-2">
              🧠 PSI Research Lab
            </Button>
            <Button className="w-full justify-start bg-transparent hover:bg-blue-500 hover:text-white text-left p-2">
              📊 Community Data
            </Button>
            <hr className="border-gray-400" />
            <Button className="w-full justify-start bg-transparent hover:bg-blue-500 hover:text-white text-left p-2">
              ❓ About Chronomancy
            </Button>
          </div>
        </Card>
      )}

      {/* Click outside to close start menu */}
      {showStartMenu && <div className="fixed inset-0 z-[-1]" onClick={() => setShowStartMenu(false)} />}
    </div>
  )
}
