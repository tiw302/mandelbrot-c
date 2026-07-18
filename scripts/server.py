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
    # attempts to kill the process holding the port to allow instant restarts
    try:
        if sys.platform == "win32":
            # windows path: use netstat to find pid and taskkill to terminate
            cmd = f'netstat -ano | findstr :{port}'
            output = subprocess.check_output(cmd, shell=True).decode()
            for line in output.splitlines():
                if "LISTENING" in line:
                    pid = line.strip().split()[-1]
                    subprocess.call(f'taskkill /F /PID {pid}', shell=True)
        else:
            # unix path: use lsof to find pids and kill syscall to terminate
            cmd = f"lsof -ti:{port}"
            pids = subprocess.check_output(cmd, shell=True).decode().split()
            for pid in pids:
                os.kill(int(pid), signal.SIGKILL)
    except:
        # ignore errors if port is already free or access is denied
        pass

class Handler(http.server.SimpleHTTPRequestHandler):
    # override extensions map to guarantee correct mime types across all platforms
    extensions_map = http.server.SimpleHTTPRequestHandler.extensions_map.copy()
    extensions_map.update({
        '.wasm': 'application/wasm',
        '.js': 'application/javascript',
        '.css': 'text/css',
        '.html': 'text/html',
        '.svg': 'image/svg+xml',
    })

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

    def translate_path(self, path):
        # strip query parameters for routing logic
        clean_path = path.split('?')[0]
        if clean_path == "/" or clean_path == "":
            clean_path = "/index.html"

        if DIRECTORY == "web" or DIRECTORY == ".":
            # custom mapping to allow serving artifacts from the build folder
            if clean_path.startswith("/index.js") or clean_path.startswith("/index.wasm"):
                return os.path.abspath(os.path.join("build_web", clean_path.lstrip("/")))
            elif clean_path.startswith("/assets/"):
                return os.path.abspath(os.path.join(".", clean_path.lstrip("/")))
            elif clean_path == "/index.html" or clean_path == "/coi-serviceworker.js" or clean_path == "/style.css" or clean_path == "/app.js":
                return os.path.abspath(os.path.join("web", clean_path.lstrip("/")))

        return super().translate_path(path)

    def end_headers(self):
        # coop/coep headers are mandatory for sharedarraybuffer (multi-threading)
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        # disable caching to ensure the latest wasm binary is always loaded
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
