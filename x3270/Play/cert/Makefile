all: server.crt

server.crt: server.csr extensions server.key
	openssl x509 -req -days 365 -in server.csr -signkey server.key -out server.crt -extfile extensions

server.csr: openssl.cnf server.key
	openssl req -config openssl.cnf -new -key server.key -out server.csr -extensions v3_req

server.key:
	openssl genrsa -des3 -out server.key 1024

dump: server.csr server.crt
	openssl req -in server.csr -text
	openssl x509 -in server.crt -text

server: server.crt server.key
	openssl s_server -cert server.crt -key server.key

