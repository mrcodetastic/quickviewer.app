# QuickViewer
**QuickViewer - Remote desktop management from a web browser, based on Qt5 for the desktop host.**

**This project contains three components:**
1. QuickView - Qt client (Windows, Linux) project;
2. quickviewer_proxy - The intermediary proxy server writtent in Go
3. public-html - The files that need to be served for the web client portal website.

***Client access portal
![Client Screenshot](screenshot-client.jpg)

*** Host desktop Qt Application
![Host Screenshot](screenshot-host.jpg)


**Simple interaction work scheme:**

```
Web Client <-> HTTP Server (for web UI content)
Web Client <-> WebSocket Server <-> Go Proxy <-> QuickViewer host desktop Qt application
```
