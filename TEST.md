printf "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080

printf "GET / HTTP/1.1\r\nHost localhost\r\n\r\n" | nc localhost 8080

// =================================== new test =======================================

curl -i --path-as-is http://127.0.0.1:8080/../test

http://127.0.0.1:8080/cgi-bin/
because autoindex is off

expecting 403 forbidden