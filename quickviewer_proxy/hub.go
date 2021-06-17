// Copyright 2013 The Gorilla WebSocket Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package main

import (
	"log"
	"os"
	"time"
)

// Hub maintains the set of active clients and broadcasts messages to the
// clients.
type Hub struct {
	// Registered clients.
	clients map[*Client]bool

	// Inbound messages from the clients.
	broadcast chan []byte

	// Register requests from the clients.
	register chan *Client

	// Unregister requests from clients.
	unregister chan *Client
}

func newHub() *Hub {
	return &Hub{
		broadcast:  make(chan []byte),
		register:   make(chan *Client),
		unregister: make(chan *Client),
		clients:    make(map[*Client]bool),
	}
}

func (h *Hub) run() {
	f, err := os.Create("desktop-clients.log")
	if err != nil {
		panic(err)
	}
	defer f.Close()

	f.WriteString("Starting desktop connection log.\n")
	f.Sync()
	//fmt.Printf("wrote %d bytes\n", n3)

	for {
		select {
		case client := <-h.register:
			log.Printf("Registering desktop client %s with hub.", client.id)
			h.clients[client] = true
			log.Printf("The hub has %d desktop clients connected.", len(h.clients))

			dt := time.Now()
			f.WriteString(dt.Format("01-02-2006 15:04:05") + " > " + client.id + " connected.\n")
			f.Sync()

		case client := <-h.unregister:
			if _, ok := h.clients[client]; ok {
				log.Printf("UNregistering desktop client %s with hub.", client.id)
				delete(h.clients, client)
				close(client.txtmsg)
			}
		case message := <-h.broadcast:
			for client := range h.clients {
				select {
				case client.txtmsg <- message:
				default:
					close(client.txtmsg)
					delete(h.clients, client)
				}
			}
		}
	}
}
