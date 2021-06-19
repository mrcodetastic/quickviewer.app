package main

import (
	"fmt"
	"log"
	"net/http"
)

func main() {
	http.HandleFunc("/", serveFile)
	http.ListenAndServe(":80", nil)
}

func serveFile(w http.ResponseWriter, r *http.Request) {
	log.Println(r.URL)

	if r.Method != "GET" {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	http.ServeFile(w, r, r.URL.Path[1:])
}

func HelloServer(w http.ResponseWriter, r *http.Request) {
	fmt.Fprintf(w, "Hello, %s!", r.URL.Path[1:]) // strip the '/'
}
