// Minimal service worker. Its only job is to make the app installable on
// Android Chrome — Chrome's install criteria require a registered SW with a
// fetch listener. We don't pre-cache anything: the device sits on a tiny
// LittleFS partition behind HTTP Basic Auth, the asset URLs are content-
// hashed by Vite, and the device sends Cache-Control: no-cache anyway.
//
// Service workers require a secure context (HTTPS or localhost). On bare
// HTTP via a LAN IP the registration will silently fail and the app still
// works as a normal site / iOS home-screen bookmark.

self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', (event) => event.waitUntil(self.clients.claim()));
self.addEventListener('fetch', () => {
  // No-op handler — without respondWith() the browser handles each request
  // normally (auth, redirects, range requests all preserved). The listener
  // must exist for the install prompt; it does not need to do anything.
});
