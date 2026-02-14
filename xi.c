#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MSG_LENGTH 99999

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;

const char *soul =
    "!SYSTEM-SOUL!\n"
    "you are xi, a tiny chat companion living in the CLI\n"
    "you keep your responses as brief & honest as possible\n"
    "you dont do flattery, you dont wish the user a good day, you are a tool\n"
    "if you were a food, you would be a flavorless protein shake\n"
    "you behave normally, not comical\n";
#include <curl/curl.h>
#define AUTH_HEADER_LENGTH 256

#include "mjson/mjson.h"

CURL *curl;
char *auth_header;

char *response_buf;

i32 prompt(const char *msg) {
  CURLcode res;
  struct curl_slist *headers = NULL;

  char *buf;
  u32 len;
  FILE *mem = open_memstream(&buf, &len);

  // setup headers
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, auth_header);

  // setup curl opts
  curl_easy_setopt(curl, CURLOPT_URL,
                   "https://openrouter.ai/api/v1/chat/completions");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, mem);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  u32 msglen = strnlen(msg, MAX_MSG_LENGTH);
  char *in_json = malloc(strlen(soul) + msglen + 600); // +600 for json overhead
  snprintf(in_json, MAX_MSG_LENGTH + 600,
           "{\"model\": \"moonshotai/kimi-k2.5\",\"messages\": [ "
           "{\"role\": \"system\", \"content\": \"%s\"},"
           "{\"role\": \"user\", \"content\": \"%s\"}], \"reasoning\": "
           "{\"enabled\": false}}",
           soul, msg);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, in_json);
  res = curl_easy_perform(curl);
  // cleanup curl
  curl_slist_free_all(headers);
  // flush buffer memstream
  fflush(mem);
  fclose(mem);
  // handle http resp
  if (res != CURLE_OK) {
    fprintf(stderr, "failed to curl openrouter: %s\n", curl_easy_strerror(res));
    free(in_json);
    free(buf);
    return -1;
  }
  // parse resp
  i32 n =
      mjson_get_string(buf, (int)strlen(buf), "$.choices[0].message.content",
                       response_buf, (int)MAX_MSG_LENGTH);
  if (n <= 0) {
    fprintf(stderr, "failed to parse message from response: %s\n", buf);
    free(in_json);
    free(buf);
    return -1;
  }

  printf("> %s\n", response_buf);
  free(in_json);
  free(buf);
  return 0;
}

i32 main(i32 argc, char **argv) {
  if (argc != 2) {
    return 1;
  }

  response_buf = malloc(MAX_MSG_LENGTH);
  auth_header = malloc(AUTH_HEADER_LENGTH); // random length but im guessing
                                            // openrouter lengths are capped
  // get openrouter token && setup auth header
  const char *auth_token = getenv("OPENROUTER");
  if (auth_token == NULL) {
    // handle err
    fprintf(stderr, "OPENROUTER token not set!\n");
    free(auth_header);
    free(response_buf);
    return 1;
  }
  snprintf(auth_header, AUTH_HEADER_LENGTH, "Authorization: Bearer %s",
           auth_token);

  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl = curl_easy_init();
  // todo
  i32 res = prompt(argv[1]);

  curl_easy_cleanup(curl);
  curl_global_cleanup();
  // cleanup
  free(auth_header);
  free(response_buf);
  return 0;
}
