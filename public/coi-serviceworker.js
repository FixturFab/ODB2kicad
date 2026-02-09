/*! coi-serviceworker v0.1.7 - Guido Zuidhof and contributors, licensed under MIT */
/*
 * This service worker enables SharedArrayBuffer on pages served without
 * Cross-Origin-Opener-Policy and Cross-Origin-Embedder-Policy headers.
 *
 * It does this by intercepting document requests and adding the required headers.
 */
let coepCredentialless = false;
if (typeof window === 'undefined') {
    self.addEventListener("install", () => self.skipWaiting());
    self.addEventListener("activate", (e) => e.waitUntil(self.clients.claim()));

    self.addEventListener("message", (ev) => {
        if (!ev.data) {
            return;
        } else if (ev.data.type === "deregister") {
            self.registration
                .unregister()
                .then(() => {
                    return self.clients.matchAll();
                })
                .then((clients) => {
                    clients.forEach((client) => client.navigate(client.url));
                });
        } else if (ev.data.type === "coepCredentialless") {
            coepCredentialless = ev.data.value;
        }
    });

    self.addEventListener("fetch", function (event) {
        const r = event.request;
        if (r.cache === "only-if-cached" && r.mode !== "same-origin") {
            return;
        }

        const request =
            coepCredentialless && r.mode === "no-cors"
                ? new Request(r, {
                      credentials: "omit",
                  })
                : r;
        event.respondWith(
            fetch(request)
                .then((response) => {
                    if (response.status === 0) {
                        return response;
                    }

                    const newHeaders = new Headers(response.headers);
                    newHeaders.set(
                        "Cross-Origin-Embedder-Policy",
                        coepCredentialless ? "credentialless" : "require-corp"
                    );
                    newHeaders.set("Cross-Origin-Opener-Policy", "same-origin");

                    return new Response(response.body, {
                        status: response.status,
                        statusText: response.statusText,
                        headers: newHeaders,
                    });
                })
                .catch((e) => console.error(e))
        );
    });
} else {
    (() => {
        const reloadedBySelf = window.sessionStorage.getItem("coiReloadedBySelf");
        window.sessionStorage.removeItem("coiReloadedBySelf");
        const coepDegrading = (reloadedBySelf === "coepdegrade");

        // Check if already has SharedArrayBuffer
        if (window.crossOriginIsolated !== false) {
            return;
        }

        // Check if service workers are supported
        if (!window.isSecureContext) {
            console.log("COOP/COEP Service Worker: Page is not in a secure context.");
            return;
        }

        if (!navigator.serviceWorker) {
            console.log("COOP/COEP Service Worker: Service workers are not supported.");
            return;
        }

        // Register the service worker
        navigator.serviceWorker
            .register(window.document.currentScript.src)
            .then(
                (registration) => {
                    if (registration.active && !navigator.serviceWorker.controller) {
                        window.sessionStorage.setItem("coiReloadedBySelf", coepDegrading ? "coepdegrade" : "");
                        window.location.reload();
                    } else if (navigator.serviceWorker.controller) {
                        if (coepDegrading) {
                            navigator.serviceWorker.controller.postMessage({
                                type: "coepCredentialless",
                                value: true,
                            });
                        }
                    }
                },
                (err) => {
                    console.error("COOP/COEP Service Worker: Registration failed:", err);
                }
            );
    })();
}
