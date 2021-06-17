// Copyright 2013 The Gorilla WebSocket Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
package main

import (
	"flag"
	"log"
	"net/http"
	"os"
	"strings"
)

var addr = flag.String("addr", ":8080", "standard socket service address")

var addr2 = flag.String("addr2", ":8443", "ssl socket service address")

func webClientServer() {
	log.Print("Started WebClient Server")
	err := http.ListenAndServe(*addr, nil)
	if err != nil {
		log.Fatal("ListenAndServe: ", err)
	}
}

func main() {
	log.Print("Started.")

	hn, err := os.Hostname()
	if err != nil {
		panic(err)
	}

	flag.Parse()
	hub := newHub()
	go hub.run() // Run the hub in it's own thread

	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) { // remote host handler
		serveWs(hub, w, r)
	})

	go webClientServer()

	if !strings.Contains(hn, "DESKTOP") {
		// This is blocking so the golang main won't exit
		log.Print("Started Secure Desktop Server")
		err := http.ListenAndServeTLS(*addr2, "cert.pem", "privkey.pem", nil)
		if err != nil {
			log.Fatal("ListenAndServeTLS: ", err)
		}
	} else {
		select {} // loop
	}

}
