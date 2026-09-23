#include "stem_client.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <string.h>

namespace stemclient {
namespace {
struct Request { Action action; char value[65], kind[8], mode[12]; bool compressed; int offset; char requestId[33]; };
QueueHandle_t commands=nullptr, replies=nullptr;
SemaphoreHandle_t lock=nullptr;
char origin[128]={}, key[129]={};
bool pending=false, lastOk=false;
uint32_t lastCheck=0;
template<size_t N> void copy(char (&out)[N], const char* in) { snprintf(out,N,"%s",in ? in : ""); }
String quotedQuery(const char* value) {
  String out;
  for(const unsigned char* p=(const unsigned char*)value;*p;++p){
    if(isalnum(*p)||*p=='-'||*p=='_')out+=char(*p);
    else{char hex[4];snprintf(hex,sizeof(hex),"%%%02X",*p);out+=hex;}
  }
  return out;
}
void worker(void*) {
  Request command;
  for(;;){
    if(xQueueReceive(commands,&command,portMAX_DELAY)!=pdTRUE)continue;
    Result result;result.action=command.action;
    char address[128],secret[129];
    xSemaphoreTake(lock,portMAX_DELAY);copy(address,origin);copy(secret,key);xSemaphoreGive(lock);
    if(WiFi.status()!=WL_CONNECTED){copy(result.error,"Connect Wi-Fi first (MENU > WIFI UPLOAD).");}
    else if(!address[0]||!secret[0]){copy(result.error,"Pair the companion from the Wi-Fi upload page first.");}
    else {
      String route="/v1/panel/",body="{}";bool post=false;
      if(command.action==Action::Library)route+="library?offset="+String(command.offset);
      if(command.action==Action::Search)route+="search?q="+quotedQuery(command.value);
      if(command.action==Action::Create){
        route+="jobs";post=true;
        StaticJsonDocument<384> payload;
        payload[strcmp(command.kind,"video")==0?"video_id":"source_id"]=command.value;
        payload["mode"]=command.mode;payload["rights_confirmed"]=true;payload["request_id"]=command.requestId;
        body="";serializeJson(payload,body);
      }
      if(command.action==Action::Status||command.action==Action::Cancel||command.action==Action::Send){
        route+="jobs/";route+=command.value;
        if(command.action==Action::Cancel){route+="/cancel";post=true;}
        if(command.action==Action::Send){route+="/send?transfer=";route+=command.compressed?"deflate-blocks-v1":"wav";post=true;}
      }
      WiFiClient client;HTTPClient http;
      http.setConnectTimeout(3000);http.setTimeout(5000);http.useHTTP10(true);
      if(!http.begin(client,String(address)+route))copy(result.error,"Invalid companion address.");
      else {
        http.addHeader("Authorization",String("Bearer ")+secret);
        http.addHeader("Content-Type","application/json");
        const int code=post?http.POST(body):http.GET();
        const int length=http.getSize();
        if(code<=0)copy(result.error,"Companion unreachable. Keep the computer awake and on this Wi-Fi.");
        else if(code==401)copy(result.error,"Pairing expired. Re-pair from the Wi-Fi upload page.");
        else if(length<0||length>8192)copy(result.error,"Companion response exceeded the device limit.");
        else {
          // Content-Length is bounded before buffering; never follow redirects with a bearer key.
          String text=http.getString();DynamicJsonDocument json(12288);
          if(text.length()!=size_t(length)||deserializeJson(json,text))copy(result.error,"Incomplete companion response. Refresh to recover.");
          else if(code<200||code>=300){copy(result.error,json["detail"].is<const char*>()?json["detail"].as<const char*>():"Companion rejected the request.");}
          else {
            result.ok=true;result.more=json["more"]|false;
            for(JsonObject item:json["items"].as<JsonArray>()){
              if(result.count>=6)break;auto& target=result.items[result.count++];
              copy(target.id,item["id"]);copy(target.kind,item["kind"]);copy(target.title,item["title"]);copy(target.state,item["state"]);
            }
            copy(result.id,json["id"]);copy(result.title,json["title"]);copy(result.state,json["state"]);copy(result.message,json["message"]);
            copy(result.delivery,json["delivery_state"]);copy(result.deliveryMessage,json["delivery_message"]);
            result.percent=json["percent"].is<int>()?json["percent"].as<int>():-1;
            result.sourceCount=json["source_count"]|0;result.fallback=json["fallback"]|false;result.bytes=json["bytes"]|0u;
            result.wavBytes=json["wav_bytes"]|0u;result.wireBytes=json["wire_bytes"]|0u;
            for(JsonVariant file:json["files"].as<JsonArray>()){if(result.fileCount>=4)break;copy(result.files[result.fileCount++],file.as<const char*>());}
          }
        }
        http.end();
      }
    }
    memset(secret,0,sizeof(secret));
    xQueueOverwrite(replies,&result);
    xSemaphoreTake(lock,portMAX_DELAY);pending=false;lastOk=result.ok;lastCheck=millis();xSemaphoreGive(lock);
  }
}
}
void begin(){
  lock=xSemaphoreCreateMutex();commands=xQueueCreate(1,sizeof(Request));replies=xQueueCreate(1,sizeof(Result));
  if(!lock||!commands||!replies)return;
  Preferences prefs;if(prefs.begin("studio-stems",true)){copy(origin,prefs.getString("origin","").c_str());copy(key,prefs.getString("key","").c_str());prefs.end();}
  if(xTaskCreatePinnedToCore(worker,"stem-http",8192,nullptr,1,nullptr,0)!=pdPASS){commands=nullptr;}
}
bool connected(){if(!lock)return false;xSemaphoreTake(lock,portMAX_DELAY);bool ok=lastOk&&millis()-lastCheck<30000;xSemaphoreGive(lock);return ok;}
bool configured(){if(!lock)return false;xSemaphoreTake(lock,portMAX_DELAY);bool yes=origin[0]&&key[0];xSemaphoreGive(lock);return yes;}
bool busy(){if(!lock||!commands)return true;xSemaphoreTake(lock,portMAX_DELAY);bool value=pending;xSemaphoreGive(lock);return value;}
bool configure(const char* address,const char* secret){
  if(!lock||!commands||!address||!secret)return false;
  // Explicit trusted LAN HTTP only. No redirect, credentials, paths, query or fragment.
  size_t n=strlen(address),k=strlen(secret);
  if(n<8||n>=sizeof(origin)||strncmp(address,"http://",7)||k<32||k>=sizeof(key))return false;
  for(size_t i=7;i<n;++i)if(!(isalnum(address[i])||address[i]=='.'||address[i]=='-'||address[i]==':'))return false;
  for(size_t i=0;i<k;++i)if(!(isalnum(secret[i])||secret[i]=='-'||secret[i]=='_'))return false;
  if(!strncmp(address+7,"127.",4)||!strcmp(address+7,"localhost"))return false;
  xSemaphoreTake(lock,portMAX_DELAY);
  if(pending){xSemaphoreGive(lock);return false;}
  Preferences prefs;bool ok=prefs.begin("studio-stems",false);
  if(ok){ok=prefs.putString("origin",address)>0&&prefs.putString("key",secret)>0;prefs.end();}
  if(ok){copy(origin,address);copy(key,secret);}
  xSemaphoreGive(lock);return ok;
}
bool request(Action action,const char* value,const char* kind,const char* mode,bool compressed,int offset){
  if(!lock||!commands)return false;
  Request command{};command.action=action;copy(command.value,value);copy(command.kind,kind);copy(command.mode,mode);command.compressed=compressed;command.offset=offset;
  snprintf(command.requestId,sizeof(command.requestId),"%08lx%08lx%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random());
  xSemaphoreTake(lock,portMAX_DELAY);
  if(!pending)xQueueReset(replies);
  bool ok=!pending&&xQueueSend(commands,&command,0)==pdTRUE;if(ok)pending=true;
  xSemaphoreGive(lock);return ok;
}
bool take(Result& result){return replies&&xQueueReceive(replies,&result,0)==pdTRUE;}
}
