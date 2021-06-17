// Copyright 2013 The Gorilla WebSocket Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package main

import (
	"log"
	"net/http"
	"strings"
	"time"

	"github.com/gorilla/websocket"
)

const (
	// Time allowed to write a message to the peer.
	writeWait = 10 * time.Second

	// Time allowed to read the next pong message from the peer.
	pongWait = 60 * time.Second

	// Send pings to peer with this period. Must be less than pongWait.
	pingPeriod = (pongWait * 9) / 10

	// Maximum message size allowed from peer.
	maxMessageSize = 128000
)

var (
	newline = []byte{'\n'}
	space   = []byte{' '}
	zero    = []byte{0, 0, 0, 0}
)

var client_num = 0

var upgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
}

// Client is a middleman between the websocket connection and the hub.
type Client struct {
	hub *Hub

	// The websocket connection.
	conn *websocket.Conn

	// Who's our partner?
	partner *Client

	// Buffered channel of outbound messages.
	txtmsg chan []byte

	binmsg chan []byte

	registered bool
	id         string
	is_desktop bool
}

// readPump pumps messages from the websocket connection to the hub.
//
// The application runs readPump in a per-connection goroutine. The application
// ensures that there is at most one reader on a connection by executing all
// reads from this goroutine.
/*
func (c *Client) readPump() {
	defer func() {
		c.hub.unregister <- c
		c.conn.Close()
	}()
	c.conn.SetReadLimit(maxMessageSize)
	c.conn.SetReadDeadline(time.Now().Add(pongWait))
	c.conn.SetPongHandler(func(string) error { c.conn.SetReadDeadline(time.Now().Add(pongWait)); return nil })
	for {
		_, message, err := c.conn.ReadMessage()

		log.Printf("message: %s", message)

		if string(message) == "1111HELO" {
			log.Printf("message: %s", "Got packet Start")
		}

		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseAbnormalClosure) {
				log.Printf("error: %v", err)
			}
			break
		}

		message = bytes.TrimSpace(bytes.Replace(message, newline, space, -1))
		c.hub.broadcast <- message
	}
}
*/

// writePump pumps messages from the hub to the websocket connection.
//
// A goroutine running writePump is started for each connection. The
// application ensures that there is at most one writer to a connection by
// executing all writes from this goroutine.
func (c *Client) writePump() {
	ticker := time.NewTicker(pingPeriod)
	defer func() {
		ticker.Stop()
		c.conn.Close()
	}()
	for {
		select {
		case message, ok := <-c.txtmsg:
			c.conn.SetWriteDeadline(time.Now().Add(writeWait))
			if !ok {
				// The hub closed the channel.
				c.conn.WriteMessage(websocket.CloseMessage, []byte{})
				return
			}

			w, err := c.conn.NextWriter(websocket.TextMessage)
			if err != nil {
				return
			}
			w.Write(message)

			// Add queued chat messages to the current websocket message.
			n := len(c.txtmsg)
			for i := 0; i < n; i++ {
				w.Write(newline)
				w.Write(<-c.txtmsg)
			}

			if err := w.Close(); err != nil {
				return
			}

		case binary, ok := <-c.binmsg:
			c.conn.SetWriteDeadline(time.Now().Add(writeWait))
			if !ok {
				// The hub closed the channel.
				c.conn.WriteMessage(websocket.CloseMessage, []byte{})
				return
			}

			w, err := c.conn.NextWriter(websocket.BinaryMessage)
			if err != nil {
				return
			}
			w.Write(binary)

			// Add queued chat messages to the current websocket message.
			n := len(c.binmsg)
			for i := 0; i < n; i++ {
				w.Write(<-c.binmsg)
			}

			if err := w.Close(); err != nil {
				return
			}

		case <-ticker.C:
			c.conn.SetWriteDeadline(time.Now().Add(writeWait))
			if err := c.conn.WriteMessage(websocket.PingMessage, nil); err != nil {
				return
			}
		}
	}
}

// readPump pumps messages from the websocket connection to the hub.
//
// The application runs readPump in a per-connection goroutine. The application
// ensures that there is at most one reader on a connection by executing all
// reads from this goroutine.
func (c *Client) readPump() {
	defer func() {
		log.Print("Closing readPump() client conn.")

		// If the desktop disconnects, but the web client is active, we need to kill the web client as well.
		//if c.is_desktop {
		if c.partner != nil {
			c.partner.conn.Close()
		}
		//}

		c.hub.unregister <- c // unregister client if it exists in the hub
		c.conn.Close()
	}()
	c.conn.SetReadLimit(maxMessageSize) // will cause server to kill connection if a full-screen refresh breaches size.
	c.conn.SetReadDeadline(time.Now().Add(pongWait))
	c.conn.SetPongHandler(func(string) error { c.conn.SetReadDeadline(time.Now().Add(pongWait)); return nil })

	for { // loop continuously for the attched 'client'
		_, message, err := c.conn.ReadMessage()

		if !c.registered { // is this client already in an established and configured?

			if len(message) < 8 { // check for valid message
				log.Printf("Invalid message recieved: %s ", string(message))
				break
			}

			var str_message = string(message) // convert to string
			log.Printf("Got message from client or desktop: %s", str_message)

			if strings.HasPrefix(str_message, "1111REGO") {
				log.Print("Attempting desktop registration request.")

				var client_info = strings.Split(str_message, "|")

				// If the starting packet doesn't contain 4 x |'s
				if len(client_info) != 4 {
					log.Print("Invalid desktop client info recieved")
					break
				}

				c.id = client_info[1]
				c.is_desktop = true

				c.hub.register <- c

				client_version := client_info[2]
				log.Printf("Desktop of version %s connected", client_version)

				if client_version != "2" {
					c.txtmsg <- []byte("TXTMSG:Please update the QuickView app from the website.")
				} else {
					// Craft response with size header
					//var rego_response [4]byte {0,0,0,0}

					response_header := []byte{'R', 'E', 'G', 'O', byte(len(client_info[1])), 0, 0, 0} // send response back with size.

					// Note: No '1111' packet start header for stuff that is sent to the desktop client
					//c.binmsg <- append([]byte("REGO---" + len(client_info[1])), []byte(client_info[1])...) // send back client id
					c.binmsg <- append(response_header, []byte(client_info[1])...) // send response back with size and ID as they sent to us

					c.registered = true

					//c.txtmsg <- []byte("TXTMSG:Hello How are you???")
				}

			} // desktop host registration request.

			if strings.HasPrefix(str_message, "1111CONN") {
				log.Print("Attempting client access request.")

				var client_info = strings.Split(str_message, "|")

				// If the starting packet doesn't contain 4 x |'s
				if len(client_info) != 4 {
					log.Print("Invalid web client info recieved")
					break
				}

				// Look for a partner desktop client
				//	log.Printf("There are %d desktops connected to server", len(c.hub.clients))

				var match = false
				for client := range c.hub.clients {
					log.Printf("Checking to see if a desktop of ID %s exists.", client_info[1])

					if client.id == client_info[1] { // Link the clients

						// is this desktop already connected to somebody else?
						if client.partner != nil {
							//client.partner.conn.Close()
							break // don't break what's already happening.
						} else {

							c.partner = client
							client.partner = c

							log.Printf("Creating connection passthru for client id: %s", client.id)
							match = true

							// Send message to desktop host to kick off nonce
							//c.partner.binmsg <- append([]byte("1111REGO"), []byte(client_info[1])...) // send back client id
						}

					}
				}

				time.Sleep(1 * time.Second) // wait 1 second

				if !match {
					response_header := []byte{'1', '1', '1', '1', 'C', 'O', 'N', 'N', 0, 0, 0, 0} // send response back with 0 size

					//c.binmsg <- append([]byte("1111CONN----"),  []byte("FAILTRAIN")...) // acknowledge connection to web client
					c.binmsg <- append(response_header, []byte("FAILTRAIN")...)
					break
				} // exit if we couldn't find a match

				// c.id = client_info[1] // web client's don't have an ID
				// c.is_desktop = false already calse
				// c.hub.register <- c // no need to register with hub

				log.Print("Web client connected.")

				c.registered = true // we're sorted

				response_header := []byte{'1', '1', '1', '1', 'C', 'O', 'N', 'N', byte(len(client_info[1])), 0, 0, 0} // send response back with web client uuid
				c.binmsg <- append(response_header, []byte(client_info[1])...)                                        // acknowledge connection to web client

			} // desktop host registration request.

		} else { // We are registered
			if c.partner != nil {
				c.partner.binmsg <- message // send message across the bridge
			}
		}

		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseAbnormalClosure) {
				log.Printf("error: %v", err)
			}
			break
		}

		//	message = bytes.TrimSpace(bytes.Replace(message, newline, space, -1))
		//	c.hub.broadcast <- message
	}

	log.Print(("readPump() goroutine ended  ")) // good to see if we're cleaning up properly
}

// serveWs handles websocket requests from the peer.
func serveWs(hub *Hub, w http.ResponseWriter, r *http.Request) {
	upgrader.CheckOrigin = func(r *http.Request) bool { return true } // remove this in production
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Println(err)
		return
	}
	log.Println(("Incoming host has been served."))

	client := &Client{hub: hub, conn: conn, txtmsg: make(chan []byte, 512), binmsg: make(chan []byte, 512)}
	//client.hub.register <- client // Don't do this just yet

	go client.readPump()  // Listen for incoming
	go client.writePump() // Prepared to output

	// Allow collection of memory referenced by the caller by doing all work in
	// new goroutines.
	//go client.readPump()
}
