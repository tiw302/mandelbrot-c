import http.server
import socketserver
import os
import sys
import subprocess
import signal

PORT = 8081

def kill_port_owner(port):
    try:
        cmd = f"lsof -ti:{port}"
        pids = subprocess.check_output(cmd, shell=True).decode().split()
        for pid in pids:
            os.kill(int(pid), signal.SIGKILL)
    except subprocess.CalledProcessError:
        pass

class MyHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        super().end_headers()

def run_server():
    kill_port_owner(PORT)
    socketserver.TCPServer.allow_reuse_address = True
    try:
        with socketserver.TCPServer(("", PORT), MyHTTPRequestHandler) as httpd:
            print(f"Mandelbrot Explorer Server started at http://localhost:{PORT}")
            print("Status: Running")
            print("Action: Press Ctrl+C to terminate the process.")
            httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutdown: Server stopped by user.")
        sys.exit(0)
    except Exception as e:
        print(f"\nError: {e}")
        sys.exit(1)

if __name__ == "__main__":
    run_server()
