#!/usr/bin/env python3
"""
ESP32 CNC Predictive Maintenance Dashboard Backend
Polls ESP32 telemetry endpoint and provides real-time data via WebSocket and REST API
"""

import os
import time
from collections import deque
from datetime import datetime
from threading import Thread, Lock

import requests
from flask import Flask, render_template, jsonify
from flask_cors import CORS
from flask_socketio import SocketIO, emit

# =====================================================
# CONFIGURATION
# =====================================================

ESP32_IP = os.getenv("ESP32_IP", "192.168.0.128")
ESP32_URL = f"http://{ESP32_IP}/telemetry"

POLL_INTERVAL = 1.0
MAX_HISTORY = 300

# =====================================================
# FLASK APP
# =====================================================

app = Flask(__name__)
app.config['SECRET_KEY'] = 'esp32-dashboard-secret'

CORS(app)

socketio = SocketIO(
    app,
    cors_allowed_origins="*"
)

# =====================================================
# DATA STORAGE
# =====================================================

data_lock = Lock()

data_history = {
    'timestamps': deque(maxlen=MAX_HISTORY),
    'temperature_c': deque(maxlen=MAX_HISTORY),
    'pressure_pa': deque(maxlen=MAX_HISTORY),
    'analog_pressure_bar': deque(maxlen=MAX_HISTORY),
    'distance_cm': deque(maxlen=MAX_HISTORY),
    'water_detected': deque(maxlen=MAX_HISTORY),
    'accel_x': deque(maxlen=MAX_HISTORY),
    'accel_y': deque(maxlen=MAX_HISTORY),
    'accel_z': deque(maxlen=MAX_HISTORY),
}

latest_data = {}

# =====================================================
# ESP32 POLLING THREAD
# =====================================================

def fetch_esp32_data():
    global latest_data

    while True:
        try:
            response = requests.get(
                ESP32_URL,
                timeout=5
            )

            response.raise_for_status()

            data = response.json()

            timestamp = datetime.now().isoformat()

            with data_lock:

                data_history['timestamps'].append(timestamp)
                data_history['temperature_c'].append(
                    data.get('temperature_c', 0)
                )

                data_history['pressure_pa'].append(
                    data.get('pressure_pa', 0)
                )

                data_history['analog_pressure_bar'].append(
                    data.get('analog_pressure_bar', 0)
                )

                data_history['distance_cm'].append(
                    data.get('distance_cm', 0)
                )

                data_history['water_detected'].append(
                    data.get('water_detected', False)
                )

                data_history['accel_x'].append(
                    data.get('accel_x', 0)
                )

                data_history['accel_y'].append(
                    data.get('accel_y', 0)
                )

                data_history['accel_z'].append(
                    data.get('accel_z', 0)
                )

                latest_data = {
                    'timestamp': timestamp,
                    **data
                }

            # Flask-SocketIO 5.x+
            socketio.emit(
                'data_update',
                latest_data
            )

        except requests.exceptions.RequestException as e:

            print(
                f"[ERROR] Failed to fetch ESP32 data: {e}"
            )

            socketio.emit(
                'connection_error',
                {
                    'error': str(e)
                }
            )

        time.sleep(POLL_INTERVAL)

# =====================================================
# REST API
# =====================================================

@app.route('/api/data', methods=['GET'])
def get_data():

    with data_lock:
        return jsonify(latest_data)

@app.route('/api/history', methods=['GET'])
def get_history():

    with data_lock:

        return jsonify({
            'timestamps': list(data_history['timestamps']),
            'temperature_c': list(data_history['temperature_c']),
            'pressure_pa': list(data_history['pressure_pa']),
            'analog_pressure_bar': list(data_history['analog_pressure_bar']),
            'distance_cm': list(data_history['distance_cm']),
            'water_detected': list(data_history['water_detected']),
            'accel_x': list(data_history['accel_x']),
            'accel_y': list(data_history['accel_y']),
            'accel_z': list(data_history['accel_z']),
        })

# =====================================================
# FRONTEND
# =====================================================

@app.route('/')
def index():
    return render_template('dashboard.html')

# =====================================================
# WEBSOCKET EVENTS
# =====================================================

@socketio.on('connect')
def handle_connect():

    print("[INFO] Client connected")

    with data_lock:
        emit(
            'initial_data',
            latest_data
        )

@socketio.on('disconnect')
def handle_disconnect():

    print("[INFO] Client disconnected")

# =====================================================
# MAIN
# =====================================================

if __name__ == '__main__':

    poll_thread = Thread(
        target=fetch_esp32_data,
        daemon=True
    )

    poll_thread.start()

    print(
        f"[INFO] Starting dashboard, polling ESP32 at {ESP32_URL}"
    )

    print(
        "[INFO] Open http://localhost:5000 in your browser"
    )

    socketio.run(
        app,
        host='0.0.0.0',
        port=5000,
        debug=False
    )
