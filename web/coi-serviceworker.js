/*! coi-serviceworker v0.1.7 - github.com/gzuidhof/coi-serviceworker */

/* service worker implementation for cross-origin isolation.
 * required to enable sharedarraybuffer and high-resolution timers.
 * injects mandatory coop and coep headers into all same-origin responses. */

if (typeof window === 'undefined') {
    self.addEventListener("install", () => self.skipWaiting());
    self.addEventListener("activate", (event) => event.waitUntil(self.clients.claim()));
    
    self.addEventListener("fetch", (event) => {
        // bypass logic for non-origin or cached requests
        if (event.request.cache === "only-if-cached" && event.request.mode !== "same-origin") {
            return;
        }
        
        event.respondWith(
            fetch(event.request).then((response) => {
                if (response.status === 0) return response;
                
                // clone and append mandatory security headers
                const newHeaders = new Headers(response.headers);
                newHeaders.set("Cross-Origin-Embedder-Policy", "require-corp");
                newHeaders.set("Cross-Origin-Opener-Policy", "same-origin");
                
                return new Response(response.body, {
                    status: response.status,
                    statusText: response.statusText,
                    headers: newHeaders,
                });
            }).catch(e => {
                console.error("[coi] fetch error:", e);
            })
        );
    });
} else {
    // client-side registration logic
    const load = () => {
        if ("serviceWorker" in navigator) {
            navigator.serviceWorker.register(window.location.pathname + "coi-serviceworker.js").then((registration) => {
                registration.addEventListener("updatefound", () => {
                    // force reload on update to ensure isolation headers are applied
                    window.location.reload();
                });
                if (registration.active && !navigator.serviceWorker.controller) {
                    // force reload if worker is active but not yet controlling the page
                    window.location.reload();
                }
            });
        }
    };
    load();
}
