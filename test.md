curl -X POST http://127.0.0.1:8080/Upload -d "hello=world"


this is to upload a file to the server
curl -v \
  -X POST "http://127.0.0.1:8080/Upload" \
  -H 'Content-Disposition: form-data; name="file"; filename="abc.txt"' \
  -H 'Content-Type: text/plain' \
  --data-binary 'abc'



to upload a file with name abc.txt and content abc
curl -X DELETE http://127.0.0.1:8080/Upload/abc.bin
this is to delete the file abc.bin from the server



http://localhost:8080/archive
to show archive files


