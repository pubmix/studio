#pragma once
#include <stdint.h>
namespace stemclient {
enum class Action { Library, Search, Create, Status, Cancel, Send };
struct Item { char id[25], kind[8], title[71], state[24]; };
struct Result {
  Action action = Action::Library;
  bool ok = false, more = false;
  char error[120] = {}, id[25] = {}, title[71] = {}, state[24] = {}, message[101] = {};
  char delivery[16] = {}, deliveryMessage[101] = {}, files[4][48] = {};
  int percent = -1, sourceCount = 0, count = 0, fileCount = 0;
  bool fallback = false;
  uint32_t bytes = 0, wavBytes = 0, wireBytes = 0;
  Item items[6] = {};
};
void begin();
bool configured();
bool connected();
bool busy();
// Trusted-LAN provisioning, no secret returned to callers. Must not run during requests.
bool configure(const char* origin, const char* key);
bool request(Action action, const char* value = "", const char* kind = "", const char* mode = "four", bool compressed = false, int offset = 0);
bool take(Result& result);
}
