#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;

#include <curl/curl.h>
#define AUTH_HEADER_LENGTH 256
#define MAX_RESP_LEN (1 << 16)

#include "mjson/mjson.h"

CURL *curl;
char *auth_header;

char *response_buf;

typedef struct {
  char *content;
  char *role;
  u32 content_len;
} Message;

#define CHAT_APPEND(item)                                                      \
  do {                                                                         \
    if (chat_len >= chat_cap) {                                                \
      chat_cap = chat_cap > 0 ? chat_cap * 2 : 8;                              \
      chat_history = realloc(chat_history, chat_cap * sizeof(Message));        \
    }                                                                          \
    chat_history[chat_len++] = item;                                           \
  } while (0)

Message *chat_history = NULL;
u32 chat_len = 0;
u32 chat_cap = 0;

const char SOUL[] =
    "!SYSTEM-SOUL!\n"
    "you are xi, a tiny chat companion living in the CLI\n"
    "you keep your responses as brief & honest as possible\n"
    "you dont do flattery, you dont wish the user a good day, you "
    "are a tool\n"
    "if you were a food, you would be a flavorless protein shake\n"
    "you behave normally, not comical\n";
const Message soul = {
    .content = (char *)SOUL,
    .role = "system",
    .content_len = sizeof(SOUL) - 1, // exclude null at the end
};

i32 prompt() {
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

  u32 in_json_len = 1024;
  char *in_json = malloc(in_json_len);
  assert(in_json != NULL);
  char *in_json_cursor = in_json;

  in_json_cursor +=
      snprintf(in_json_cursor, in_json_len,
               "{\"model\": \"moonshotai/kimi-k2.5\" \"reasoning\": "
               "{\"enabled\": false}, \"messages\" : [");

  // render messages to json
  for (u32 i = 0; i < chat_len; i++) {
    // check if our cursor in the string has ran out buffer size
    if (strlen(chat_history[i].content) >=
        (in_json_len - (u32)(in_json_cursor - in_json))) {
      // alloc larger buffer
      in_json_len *= 2;
      in_json_len += chat_history[i].content_len + 2;
      in_json = realloc(in_json, in_json_len);
    }

    in_json_cursor +=
        sprintf(in_json_cursor, "{\"role\": \%s\", \"content\": \"%.*s\"},",
                chat_history[i].role, chat_history[i].content_len,
                chat_history[i].content);
  }
  // we just hope in_json_cursor has that extra space , not smart but ya know,
  // should be fine
  in_json_cursor += sprintf(in_json_cursor, "]}");

  // realloc to trim in_json_buf
  in_json_len = (in_json_cursor - in_json) + 1;
  in_json = realloc(in_json, in_json_len + 1);
  in_json[in_json_len] = '\0';

  // post json
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
  const i32 resp_len =
      mjson_get_string(buf, (i32)strlen(buf), "$.choices[0].message.content",
                       response_buf, (i32)MAX_RESP_LEN);
  if (resp_len <= 0) {
    fprintf(stderr, "failed to parse message from response: %s\n", buf);
    free(in_json);
    free(buf);
    return -1;
  }

  response_buf[resp_len] = '\0'; // zero end
  printf("\n%.*s\n", resp_len, response_buf);
  // append response to history
  Message resp = {.content = malloc(resp_len + 1),
                  .content_len = resp_len + 1, // not sure if +1 is right here
                  .role = "assistant"};
  strncpy(resp.content, response_buf, resp_len + 1);
  CHAT_APPEND(resp);

  free(in_json);
  free(buf);
  return 0;
}

// main loop
i32 main() {

  response_buf = malloc(MAX_RESP_LEN);
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

  // init chat_history
  chat_history = malloc(2 * sizeof(Message));
  chat_cap = 2;
  chat_len = 0;

  CHAT_APPEND(soul);

  // start loop
  char input[1024];
  while (1) {
    printf("> ");
    fflush(stdout);

    if (!fgets(input, sizeof(input), stdin)) {
      break; // eof or err
    }
    input[strcspn(input, "\n")] = '\0';
    Message user_msg = {.content = malloc(strlen(input) + 1),
                        .role = "user",
                        .content_len = strlen(input)};
    user_msg.content[user_msg.content_len] = '\0';
    memcpy(user_msg.content, input, user_msg.content_len + 1);

    CHAT_APPEND(user_msg);
    if (prompt() != 0) {
      break; // err
    }
  }

  curl_easy_cleanup(curl);
  curl_global_cleanup();
  // cleanup
  free(auth_header);
  free(response_buf);

  // TODO: free chat_history
  return 0;
}
