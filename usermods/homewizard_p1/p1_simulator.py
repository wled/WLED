"""Fake HomeWizard P1 local API v1 for testing the WLED usermod.

Serves /api/v1/data and cycles active_power_w every 10 seconds:
  injection (-800 W) -> neutral (10 W) -> offtake (+1200 W)
"""
import json
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

CYCLE = [(-800.0, "injection"), (10.0, "neutral"), (1200.0, "offtake")]
PERIOD_S = 10


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path != "/api/v1/data":
            self.send_response(404)
            self.end_headers()
            return
        phase_idx = int(time.time() // PERIOD_S) % len(CYCLE)
        power, label = CYCLE[phase_idx]
        body = json.dumps({
            "wifi_ssid": "simulated",
            "wifi_strength": 100,
            "smr_version": 50,
            "meter_model": "Simulator",
            "active_power_w": power,
            "active_power_l1_w": power,
            "total_power_import_kwh": 1234.5,
            "total_power_export_kwh": 678.9,
        }).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)
        print(f"{time.strftime('%H:%M:%S')} served {power:+.0f} W ({label}) to {self.client_address[0]}", flush=True)

    def log_message(self, *args):
        pass  # quiet default access log; we print our own line above


if __name__ == "__main__":
    ThreadingHTTPServer(("0.0.0.0", 8123), Handler).serve_forever()
