#pragma once
#include <string>
#include <unordered_map>
#include <sstream>


/* STRUCTURA UNUI REQUEST HTTP (Raw Bytes)
   ---------------------------------------
   [Request Line]
   GET /video/avatar.mp4?token=123 HTTP/1.1\r\n

   [Headers]
   Host: cdn.node1.net\r\n
   User-Agent: Mozilla/5.0...\r\n
   Range: bytes=0-1024\r\n
   Connection: keep-alive\r\n
   \r\n
   [Body - optional, ex: la POST/PUT]
   ...binary_data_or_text...
*/


struct HttpRequest {

};
enum class StatusCode {
    OK = 200,
    PartialContent = 206,
    Found = 302,
    BadRequest = 400,
    NotFound = 404,
    InternalError = 500
};

struct HttpResponse {


private:

};