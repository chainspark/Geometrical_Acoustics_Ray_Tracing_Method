#!/usr/bin/env python3
"""Serve the web simulator locally: python scripts/serve.py [port]

Plain static server with the correct MIME types for .wasm / .js modules and caching
disabled, so a rebuilt gart.wasm is picked up on reload.
"""
import functools
import http.server
import sys
from pathlib import Path

WEB_ROOT = Path(__file__).resolve().parent.parent / "web"


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        ".wasm": "application/wasm",
        ".js": "text/javascript",
        ".mjs": "text/javascript",
        ".css": "text/css",
        ".json": "application/json",
    }

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def main() -> None:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    if not (WEB_ROOT / "wasm" / "gart.wasm").exists():
        sys.exit("web/wasm/gart.wasm not found - run scripts/build-wasm.sh first")
    handler = functools.partial(Handler, directory=str(WEB_ROOT))
    with http.server.ThreadingHTTPServer(("127.0.0.1", port), handler) as httpd:
        print(f"Serving {WEB_ROOT} at http://localhost:{port}/  (Ctrl+C to stop)")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
