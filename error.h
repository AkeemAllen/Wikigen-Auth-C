#ifndef ERROR_H_
#define ERROR_H_

typedef enum { OK = 0, ERROR, PARSE_ERROR, REQUEST_ERROR, DATABASE_ERROR,DB_NOT_FOUND, NOT_FOUND, INVALID_HTTP_METHOD, INVALID_HEADER, INVALID_JSON, INVALID_TOKEN, INVALID_PAYLOAD, INVALID_PARAMETER } Error;

typedef struct ErrorContext {
  Error code;
  char message[256];
} ErrorContext;

#define ERROR_CONTEXT(err, msg) (ErrorContext){.code = (err), .message = (msg)}

#endif 
