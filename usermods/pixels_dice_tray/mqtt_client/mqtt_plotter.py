import argparse
from http import server
import os
from pathlib import Path
import socketserver

import pandas as pd
import plotly.express as px

# python -m http.server 8000 --directory /tmp/


def main():
    parser = argparse.ArgumentParser(
        description="Generate an html plot of rolls captured by mqtt_logger.py")
    parser.add_argument("input_file", type=Path, help="Log file to plot")
    parser.add_argument(
        "-s",
        "--start-server",
        action="store_true",
        help="After generating the plot, run a webserver pointing to it",
    )
    parser.add_argument(
        "-o",
        "--output-dir",
        type=Path,
        default=Path(__file__).absolute().parent / "logs",
        help="Directory to log to",
    )
    args = parser.parse_args()

    df = pd.read_csv(
        args.input_file, names=["timestamp", "die", "label", "state", "roll"]
    )

    df_filt = df[df["state"] == 1]

    time = (df_filt["timestamp"] - df_filt["timestamp"].min()) / 60 / 60

    fig = px.bar(
        df_filt,
        x=time,
        y="roll",
        color="label",
        labels={
            "x": "Game Time (min)",
        },
        title=f"Roll Report: {args.input_file.name}",
    )

    output_path = args.output_dir / (args.input_file.stem + ".html")

    fig.write_html(output_path)
    if args.start_server:
        PORT = 8000
        os.chdir(args.output_dir)
        try:
            with socketserver.TCPServer(
                ("", PORT), server.SimpleHTTPRequestHandler
            ) as httpd:
                print(
                    f"Serving HTTP on http://0.0.0.0:{PORT}/{output_path.name}")
                httpd.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
