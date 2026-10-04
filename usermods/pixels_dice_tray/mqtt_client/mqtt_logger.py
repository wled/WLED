#!/usr/bin/env python
import argparse
import json
import os
from pathlib import Path
import time

# Dependency installed with `pip install paho-mqtt`.
# https://pypi.org/project/paho-mqtt/
import paho.mqtt.client as mqtt

state = {"label": "None"}


# Define MQTT callbacks
def on_connect(client, userdata, connect_flags, reason_code, properties):
    print("Connected with result code " + str(reason_code))
    state["start_time"] = None
    client.subscribe(f"{state['root_topic']}#")


def on_message(client, userdata, msg):
    if msg.topic.endswith("roll_label"):
        state["label"] = msg.payload.decode("ascii")
        print(f"Label set to {state['label']}")
    elif msg.topic.endswith("roll"):
        json_str = msg.payload.decode("ascii")
        msg_data = json.loads(json_str)
        # Convert the relative timestamps reported to the dice to an approximate absolute time.
        # The "last_time" check is to detect if the ESP32 was restarted or the counter rolled over.
        if state["start_time"] is None or msg_data["time"] < state["last_time"]:
            state["start_time"] = time.time() - (msg_data["time"] / 1000.0)
        state["last_time"] = msg_data["time"]
        timestamp = state["start_time"] + (msg_data["time"] / 1000.0)
        state["csv_fd"].write(
            f"{timestamp:.3f}, {msg_data['name']}, {state['label']}, {msg_data['state']}, {msg_data['val']}\n"
        )
        state["csv_fd"].flush()
        if msg_data["state"] == 1:
            print(
                f"{timestamp:.3f}: {msg_data['name']} rolled {msg_data['val']}")


def main():
    parser = argparse.ArgumentParser(
        description="Log die rolls from WLED MQTT events to CSV.")

    # IP address (with a default value)
    parser.add_argument(
        "--host",
        type=str,
        default="127.0.0.1",
        help="Host address of broker (default: 127.0.0.1)",
    )
    parser.add_argument(
        "--port", type=int, default=1883, help="Broker TCP port (default: 1883)"
    )
    parser.add_argument("--user", type=str, help="Optional MQTT username")
    parser.add_argument("--password", type=str, help="Optional MQTT password")
    parser.add_argument(
        "--topic",
        type=str,
        help="Optional MQTT topic to listen to. For example if topic is 'wled/e5a658/dice/', subscript to  to 'wled/e5a658/dice/#'. By default, listen to all topics looking for ones that end in 'roll_label' and 'roll'.",
    )
    parser.add_argument(
        "-o",
        "--output-dir",
        type=Path,
        default=Path(__file__).absolute().parent / "logs",
        help="Directory to log to",
    )
    args = parser.parse_args()

    timestr = time.strftime("%Y-%m-%d")
    os.makedirs(args.output_dir, exist_ok=True)
    state["csv_fd"] = open(args.output_dir / f"roll_log_{timestr}.csv", "a")

    # Create `an MQTT client
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

    # Set MQTT callbacks
    client.on_connect = on_connect
    client.on_message = on_message

    if args.user and args.password:
        client.username_pw_set(args.user, args.password)

    state["root_topic"] = ""

    # Connect to the MQTT broker
    client.connect(args.host, args.port, 60)

    try:
        while client.loop(timeout=1.0) == mqtt.MQTT_ERR_SUCCESS:
            time.sleep(0.1)
    except KeyboardInterrupt:
        exit(0)

    print("Connection Failure")
    exit(1)


if __name__ == "__main__":
    main()
