# ESP32 CNC Predictive Maintenance Dashboard

A real-time telemetry dashboard for your ESP32 sensor system with live graphs and WebSocket updates.

## Quick Start

### 1. Install dependencies

```bash
pip install -r requirements.txt
```

### 2. Set ESP32 IP (if not 192.168.1.29)

```bash
export ESP32_IP=192.168.1.XX
```

### 3. Run the dashboard

```bash
python esp32_dashboard.py
```

### 4. Open in browser

Navigate to: **http://localhost:5000**

## Features

✅ **Real-time graphs** for all 8 sensor parameters
✅ **WebSocket streaming** - no manual refresh needed
✅ **Live metric cards** with current values
✅ **5-minute history** - scrolling window of data
✅ **Status indicator** - connection monitoring
✅ **Responsive design** - works on tablets/phones too

## Sensor Data Plotted

- **Temperature** (°C)
- **Pressure** (Pa)
- **Gauge Pressure** (bar)
- **Distance** (cm)
- **Water Detection** (binary)
- **Accelerometer X/Y/Z** (mg)
- **Authentication Status**

## Architecture

```
ESP32 (192.168.1.29)
    ↓ (HTTP /telemetry endpoint)
Flask Backend (localhost:5000)
    ↓ (WebSocket + REST)
Browser Dashboard
```

The backend polls your ESP32 every 1 second and broadcasts updates to all connected clients via WebSocket.

## Customization

**Change polling interval:** Edit `POLL_INTERVAL` in `esp32_dashboard.py` (in seconds)

**Change history window:** Edit `MAX_HISTORY` in `esp32_dashboard.py` (number of data points)

**Change ESP32 URL:** Update `ESP32_IP` environment variable or hardcode in the script

## Troubleshooting

**"Connection refused" at startup?**
- Make sure ESP32 is powered on and connected to WiFi
- Verify ESP32 IP with `ESP32_IP` variable
- Check HTTP endpoint: `curl http://192.168.1.29/telemetry`

**Graphs not updating?**
- Check browser console (F12) for WebSocket errors
- Ensure firewall allows localhost:5000
- Try refreshing the page

**Port 5000 already in use?**
- Edit the port in the last line of `esp32_dashboard.py`

## Development

For debugging, run with verbose logging:

```bash
python esp32_dashboard.py 2>&1 | tee dashboard.log
```

Monitor WebSocket traffic in browser DevTools (Network tab → WS filter)
