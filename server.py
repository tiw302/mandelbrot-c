import http.server
import socketserver
import os
import sys
import subprocess
import signal
import argparse

# default config
PORT = 8081
DIRECTORY = "web" # default directory to serve

def kill_port_owner(port):
    try:
        # linux/macos only
        cmd = f"lsof -ti:{port}"
        pids = subprocess.check_output(cmd, shell=True).decode().split()
        for pid in pids:
            os.kill(int(pid), signal.SIGKILL)
    except:
        pass

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=".", **kwargs)

    def translate_path(self, path):
        # Map paths to simulate the GitHub Pages 'deploy' folder layout
        if path == "/" or path == "":
            path = "/index.html"
            
        if path.startswith("/index.js") or path.startswith("/index.wasm"):
            return os.path.abspath(os.path.join("build-web", path.lstrip("/")))
        elif path.startswith("/assets/"):
            return os.path.abspath(os.path.join(".", path.lstrip("/")))
        elif path == "/index.html" or path == "/coi-serviceworker.js":
            return os.path.abspath(os.path.join("web", path.lstrip("/")))
            
        return super().translate_path(path)

    def end_headers(self):
        # required for sharedarraybuffer/pthreads
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        # prevent caching during development
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        super().end_headers()

def run_server():
    global DIRECTORY
    parser = argparse.ArgumentParser(description="mandelbrot development server")
    parser.add_argument("--dir", default="web", help="directory to serve (default: web)")
    parser.add_argument("--port", type=int, default=8081, help="port to use (default: 8081)")
    args = parser.parse_args()

    DIRECTORY = args.dir
    port = args.port

    kill_port_owner(port)
    socketserver.TCPServer.allow_reuse_address = True
    
    try:
        with socketserver.TCPServer(("", port), Handler) as httpd:
            print(f"server: serving '{DIRECTORY}' at http://localhost:{port}")
            print("status: ready (press ctrl+c to stop)")
            httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nserver: stopped by user.")
        sys.exit(0)
    except Exception as e:
        print(f"\nerror: {e}")
        sys.exit(1)

if __name__ == "__main__":
    run_server()
