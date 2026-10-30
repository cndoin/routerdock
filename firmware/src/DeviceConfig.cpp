#include "DeviceConfig.h"
#include "PortalPage.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_system.h>

namespace {
Preferences storage;
WebServer server(80);
DNSServer dns;
SemaphoreHandle_t cfgLock;
DeviceConfig current;
String adminPassword, csrf, hostname, apSSID;
bool apActive=false, mdnsStarted=false;
uint32_t apStarted=0, restartAt=0, retryAt=0, bootAt=0, scanAt=0;
bool scanRequested=false, scanReady=false;
String scanResult="[]";
String randomHex(int bytes){String s;for(int i=0;i<bytes;i++){char b[3];snprintf(b,sizeof b,"%02x",(unsigned)(esp_random()&255));s+=b;}return s;}
void headers(){server.sendHeader("Cache-Control","no-store");server.sendHeader("X-Content-Type-Options","nosniff");server.sendHeader("X-Frame-Options","DENY");server.sendHeader("Content-Security-Policy","default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; frame-ancestors 'none'; connect-src 'self'");}
void sendJSON(int status,JsonDocument& doc){String s;serializeJson(doc,s);headers();server.send(status,"application/json",s);}
void error(int status,const char* reason){JsonDocument j;j["ok"]=false;j["error"]=reason;sendJSON(status,j);}
bool auth(bool mutate=false){
 if(!server.authenticate("admin",adminPassword.c_str())){headers();server.requestAuthentication(BASIC_AUTH,"RouterDock");return false;}
 if(mutate&&server.header("X-CSRF-Token")!=csrf){error(403,"Invalid session token. Reload this page.");return false;}return true;
}
bool validHost(const String& host){if(host.isEmpty()||host.length()>253||host.startsWith(".")||host.endsWith("."))return false;for(char c:host)if(!isalnum((unsigned char)c)&&c!='.'&&c!='-')return false;return true;}
bool validText(const String& value){for(char c:value)if((unsigned char)c<32||c==127)return false;return true;}
// Validate the full candidate before writing any NVS keys.
bool update(JsonDocument& j,String& reason){
 DeviceConfig next=configSnapshot();
 if(!j["ssid"].is<const char*>()||!j["routerHost"].is<const char*>()||!j["routerPort"].is<int>()||!j["refreshMs"].is<int>()||!j["brightness"].is<int>()){reason="Missing or invalid settings fields.";return false;}
 String ssid=j["ssid"].as<String>();String password=j["wifiPassword"]|"";String token=j["routerToken"]|"";String admin=j["adminPassword"]|"";
 if(ssid.isEmpty()||ssid.length()>32||!validText(ssid)){reason="Wi-Fi name must contain 1–32 bytes.";return false;}
 bool open=j["openNetwork"]==true;
 if(open)password="";
 else if(password.isEmpty()&&ssid==next.ssid)password=next.wifiPassword;
 else if(password.isEmpty()){reason="Enter the new Wi-Fi password or select Open network.";return false;}
 if(!open&&(password.length()<8||password.length()>63||!validText(password))){reason="Wi-Fi password must contain 8–63 characters.";return false;}
 String host=j["routerHost"].as<String>();int port=j["routerPort"].as<int>();int interval=j["refreshMs"].as<int>();int brightness=j["brightness"].as<int>();
 if(!validHost(host)||port<1||port>65535){reason="Enter a valid router IPv4/hostname and port.";return false;}
 if(interval!=2000&&interval!=3000&&interval!=5000&&interval!=10000){reason="Invalid refresh interval.";return false;}
 if(brightness<40||brightness>255){reason="Brightness must be 40–255.";return false;}
 if(token.isEmpty())token=next.routerToken;
 if(!token.isEmpty()&&(token.length()<16||token.length()>128||!validText(token))){reason="Router token must contain 16–128 characters.";return false;}
 if(!admin.isEmpty()&&(admin.length()<12||admin.length()>63||!validText(admin))){reason="Admin password must contain 12–63 characters.";return false;}
 next.ssid=ssid;next.wifiPassword=password;next.routerHost=host;next.routerPort=port;next.routerToken=token;next.refreshMs=interval;next.brightness=brightness;
 // One versioned JSON record, so interrupted updates cannot mix old/new keys.
 JsonDocument record;record["ssid"]=ssid;record["wifiPassword"]=password;record["routerHost"]=host;record["routerPort"]=port;record["routerToken"]=token;record["refreshMs"]=interval;record["brightness"]=brightness;record["adminPassword"]=admin.isEmpty()?adminPassword:admin;
 String raw;serializeJson(record,raw);if(storage.putString("record",raw)!=raw.length()){reason="Could not save configuration.";return false;}
 xSemaphoreTake(cfgLock,portMAX_DELAY);current=next;xSemaphoreGive(cfgLock);
 // Password changes take effect after the scheduled reboot.
 return true;
}
void routes(){
 const char* names[]={"X-CSRF-Token"};server.collectHeaders(names,1);
 server.on("/",HTTP_GET,[]{if(!auth())return;headers();server.send_P(200,"text/html; charset=utf-8",PORTAL_HTML);});
 server.on("/api/status",HTTP_GET,[]{if(!auth())return;DeviceConfig c=configSnapshot();JsonDocument j;j["name"]="RouterDock";j["version"]="0.1.0";j["hostname"]=hostname;j["connected"]=WiFi.status()==WL_CONNECTED;j["ip"]=WiFi.localIP().toString();j["rssi"]=WiFi.RSSI();j["heap"]=ESP.getFreeHeap();j["uptime"]=millis()/1000;j["portal"]=apActive;j["ssid"]=c.ssid;j["routerHost"]=c.routerHost;j["routerPort"]=c.routerPort;j["tokenConfigured"]=!c.routerToken.isEmpty();j["passwordConfigured"]=!c.wifiPassword.isEmpty();j["refreshMs"]=c.refreshMs;j["brightness"]=c.brightness;j["csrf"]=csrf;sendJSON(200,j);});
 server.on("/api/scan",HTTP_POST,[]{if(!auth(true))return;if(!scanRequested){scanRequested=true;scanReady=false;WiFi.scanDelete();WiFi.scanNetworks(true,false);scanAt=millis();}JsonDocument j;j["ok"]=true;sendJSON(202,j);});
 server.on("/api/scan",HTTP_GET,[]{if(!auth())return;headers();server.send(200,"application/json",String("{\"running\":")+(scanRequested?"true":"false")+",\"ready\":"+(scanReady?"true":"false")+",\"networks\":"+scanResult+"}");});
 server.on("/api/config",HTTP_POST,[]{if(!auth(true))return;String body=server.arg("plain");if(body.length()>2048){error(413,"Settings too large.");return;}JsonDocument j;if(deserializeJson(j,body)||!j.is<JsonObject>()){error(400,"Invalid JSON.");return;}String why;if(!update(j,why)){error(400,why.c_str());return;}JsonDocument out;out["ok"]=true;out["message"]="Saved. Restarting in one second.";sendJSON(200,out);restartAt=millis()+1200;});
 server.on("/api/reboot",HTTP_POST,[]{if(!auth(true))return;JsonDocument j;j["ok"]=true;sendJSON(200,j);restartAt=millis()+1200;});
 server.onNotFound([]{headers();server.send(404,"text/plain","Not found");});server.begin();
}
}
bool settingsScreen=false;
DeviceConfig configSnapshot(){xSemaphoreTake(cfgLock,portMAX_DELAY);DeviceConfig c=current;xSemaphoreGive(cfgLock);return c;}
String configHostname(){return hostname;}
String configPortalSSID(){return apSSID;}
String configAdminPassword(){return adminPassword;}
bool configPortalActive(){return apActive;}
String configRouterURL(){DeviceConfig c=configSnapshot();return "http://"+c.routerHost+":"+String(c.routerPort)+"/cgi-bin/routerdock";}
void configStartPortal(){if(!apActive){WiFi.mode(WIFI_AP_STA);WiFi.softAP(apSSID.c_str(),adminPassword.c_str(),1,false,2);dns.start(53,"*",WiFi.softAPIP());apActive=true;apStarted=millis();}settingsScreen=true;}
void configBegin(){
 cfgLock=xSemaphoreCreateMutex();storage.begin("routerdock",false);JsonDocument j;deserializeJson(j,storage.getString("record","{}"));
 current.ssid=j["ssid"]|"";current.wifiPassword=j["wifiPassword"]|"";current.routerHost=j["routerHost"]|"192.168.1.1";current.routerPort=j["routerPort"]|80;current.routerToken=j["routerToken"]|"";current.refreshMs=j["refreshMs"]|2000;current.brightness=j["brightness"]|220;
 adminPassword=j["adminPassword"]|"";if(adminPassword.length()<12){adminPassword=storage.getString("admin","");if(adminPassword.length()<12){adminPassword=randomHex(8);storage.putString("admin",adminPassword);}}
 csrf=randomHex(16);char suffix[9];snprintf(suffix,sizeof suffix,"%06x",(unsigned)(ESP.getEfuseMac()&0xFFFFFF));hostname=String("routerdock-")+suffix;apSSID=String("RouterDock-")+suffix;
 WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setHostname(hostname.c_str());WiFi.setSleep(false);WiFi.setAutoReconnect(true);pinMode(0,INPUT_PULLUP);analogWrite(21,current.brightness);
 bootAt=millis();if(!current.ssid.isEmpty()){if(current.wifiPassword.isEmpty())WiFi.begin(current.ssid.c_str());else WiFi.begin(current.ssid.c_str(),current.wifiPassword.c_str());}else configStartPortal();routes();
}
bool configSerialProvision(const String& raw){JsonDocument j;String why;if(deserializeJson(j,raw)||!update(j,why)){Serial.println("[config] rejected (invalid settings)");return false;}Serial.println("[config] saved; restarting");restartAt=millis()+1200;return true;}
void configLoop(){
 server.handleClient();if(apActive)dns.processNextRequest();
 if(restartAt&&(int32_t)(millis()-restartAt)>=0)ESP.restart();
 if(scanRequested){int n=WiFi.scanComplete();if(n>=0){JsonDocument j;JsonArray a=j.to<JsonArray>();for(int i=0;i<n&&a.size()<24;i++){String name=WiFi.SSID(i);if(name.isEmpty())continue;bool duplicate=false;for(JsonVariant v:a)if(v["ssid"].as<String>()==name)duplicate=true;if(duplicate)continue;JsonObject o=a.add<JsonObject>();o["ssid"]=name;o["rssi"]=WiFi.RSSI(i);o["open"]=WiFi.encryptionType(i)==WIFI_AUTH_OPEN;o["channel"]=WiFi.channel(i);}scanResult="";serializeJson(j,scanResult);WiFi.scanDelete();scanRequested=false;scanReady=true;}else if(n==WIFI_SCAN_FAILED||millis()-scanAt>20000){scanRequested=false;scanReady=true;scanResult="[]";}}
 bool online=WiFi.status()==WL_CONNECTED;
 if(online&&!mdnsStarted)mdnsStarted=MDNS.begin(hostname.c_str());
 if(!online&&millis()-bootAt>45000&&!apActive)configStartPortal();
 if(!online&&!current.ssid.isEmpty()&&millis()-retryAt>20000){retryAt=millis();WiFi.reconnect();}
 static uint32_t pressedAt=0;static bool held=false;
 if(digitalRead(0)==LOW){if(!pressedAt)pressedAt=millis();if(!held&&millis()-pressedAt>4000){held=true;configStartPortal();}}else{pressedAt=0;held=false;}
 if(apActive&&online&&millis()-apStarted>600000){dns.stop();WiFi.softAPdisconnect(true);WiFi.mode(WIFI_STA);apActive=false;settingsScreen=false;}
}
