#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LovyanGFX.hpp>
#include <Preferences.h>

#include "DeviceConfig.h"
class Panel:public lgfx::LGFX_Device {
 lgfx::Bus_SPI bus;lgfx::Panel_ILI9341 panel;lgfx::Touch_XPT2046 touch;
 public: Panel(){auto b=bus.config();b.spi_host=SPI2_HOST;b.spi_mode=0;b.freq_write=40000000;b.freq_read=16000000;b.pin_sclk=14;b.pin_mosi=13;b.pin_miso=12;b.pin_dc=2;b.dma_channel=1;bus.config(b);panel.setBus(&bus);
 auto p=panel.config();p.pin_cs=15;p.pin_rst=-1;p.panel_width=240;p.panel_height=320;p.readable=false;p.rgb_order=false;panel.config(p);
 auto t=touch.config();t.spi_host=SPI3_HOST;t.freq=1000000;t.pin_sclk=25;t.pin_mosi=32;t.pin_miso=39;t.pin_cs=33;t.pin_int=36;t.x_min=200;t.x_max=3900;t.y_min=200;t.y_max=3900;t.bus_shared=false;touch.config(t);panel.setTouch(&touch);setPanel(&panel);}
} lcd;
lgfx::LGFX_Sprite canvas(&lcd);
static const auto& chinese=lgfx::fonts::efontCN_12;
Preferences prefs;
SemaphoreHandle_t lock;
JsonDocument docs[5];
volatile int page=0,groupIndex=1,nodeOffset=0;
volatile bool dirty=true,forcePoll=true;
uint32_t lastOk[5]={};
String notice="启动中",pendingAction,pendingGroup,pendingNode;
int confirm=0;
String testNodes[4],testGroup;int testCount=0,testNext=0;
volatile bool testing=false;
float downHistory[48]={},upHistory[48]={};int historyCount=0;
uint32_t toastUntil=0;
uint16_t green=0x4F4F,orange=0xFDC6,border=0x4228;

bool connected=false;
bool connectionView=false,systemDetails=false;
uint16_t bg=0x0841,card=0x318C,accent=0x3DFF,white=0xEF7D,muted=0x9CD3;
const char* tabs[]={"总览","节点","UA3F","流量","系统"};
String bytes(double n){const char* u[]={"B","KB","MB","GB","TB"};int i=0;while(n>=1024&&i<4){n/=1024;i++;}return String(n,i>0?1:0)+u[i];}
String displayText(const String& s){String out;for(unsigned i=0;i<s.length();){uint8_t c=s[i];unsigned n=c<128?1:(c<224?2:(c<240?3:4));if(i+n>s.length())break;bool variation=n==3&&c==0xEF&&(uint8_t)s[i+1]==0xB8&&(uint8_t)s[i+2]>=0x80;if(n<4&&!variation)out+=s.substring(i,i+n);i+=n;}return out;}
void text(String s,int x,int y,uint16_t c=0xEF7D){s=displayText(s);canvas.setTextColor(c);canvas.setClipRect(x,y,max(0,308-x),34);canvas.drawString(s,x,y);canvas.clearClipRect();}
void button(String s,int x,int y,int w=72){canvas.fillRoundRect(x,y,w,25,5,card);text(s,x+6,y+6,accent);}
void row(String label,String value,int y){text(label,18,y,muted);text(value,152,y);}
void metric(String label,String value,int x,int y){canvas.fillRoundRect(x,y,144,55,7,card);text(label,x+10,y+7,muted);canvas.setFont(&lgfx::fonts::Font4);text(value,x+10,y+25,accent);canvas.setFont(&chinese);}
void queueAction(String a,String g="",String n=""){xSemaphoreTake(lock,portMAX_DELAY);if(pendingAction==""){pendingAction=a;pendingGroup=g;pendingNode=n;notice="正在提交操作…";}xSemaphoreGive(lock);dirty=true;}
void startTests(int index=-1){
 xSemaphoreTake(lock,portMAX_DELAY);
 if(!testing){JsonArray groups=docs[1]["groups"].as<JsonArray>();JsonVariant g=groups[groupIndex-1];testGroup=g["name"]|"";testCount=0;testNext=0;
 JsonArray nodes=g["nodes"].as<JsonArray>();for(int i=0;i<(int)nodes.size()&&testCount<4;i++)if(index<0||index==i)testNodes[testCount++]=nodes[i]["name"].as<String>();testing=testCount>0;notice="正在测试节点延迟…";}
 xSemaphoreGive(lock);dirty=true;
}
void fetchTask(void*){
 HTTPClient http;uint32_t poll=0;int prior=-1;
 for(;;){
  DeviceConfig cfg=configSnapshot();
  if(WiFi.status()!=WL_CONNECTED||cfg.routerToken.isEmpty()){vTaskDelay(pdMS_TO_TICKS(200));continue;}
  int pg=page,gi=groupIndex,no=nodeOffset;String action,g,n;
  xSemaphoreTake(lock,portMAX_DELAY);action=pendingAction;g=pendingGroup;n=pendingNode;pendingAction="";
  if(action==""&&testing&&testNext<testCount){action="delay";g=testGroup;n=testNodes[testNext++];notice="测速 "+String(testNext)+"/"+String(testCount);}
  xSemaphoreGive(lock);
  if(action!="" || forcePoll || prior!=pg || millis()-poll>cfg.refreshMs){
   forcePoll=false;poll=millis();prior=pg;
   String endpoint=pg==1?"clash&g="+String(gi)+"&p="+String(no):pg==2?"ua":pg==3?"traffic":"status";
   if(action!="")endpoint="action";
   http.begin("http://"+cfg.routerHost+":"+String(cfg.routerPort)+"/cgi-bin/routerdock?"+endpoint);http.setConnectTimeout(1500);http.setTimeout(6500);http.addHeader("Authorization",String("Bearer ")+cfg.routerToken);
   int code;
   if(action!=""){JsonDocument req;req["action"]=action;req["args"]["group"]=g;req["args"]["node"]=n;String body;serializeJson(req,body);http.addHeader("Content-Type","application/json");code=http.POST(body);}else code=http.GET();
   JsonDocument result;String body=code==200?http.getString():"";auto err=deserializeJson(result,body);http.end();
   xSemaphoreTake(lock,portMAX_DELAY);
   if(code==200&&!err){
    if(action!=""){toastUntil=millis()+5000;notice=result["ok"].as<bool>()?String(result["msg"]|"操作成功"):String(result["error"]|"操作失败");forcePoll=true;
     if(action=="delay"){JsonArray groups=docs[1]["groups"].as<JsonArray>();for(JsonVariant grp:groups){JsonArray nodes=grp["nodes"].as<JsonArray>();for(JsonVariant node:nodes)if(node["name"].as<String>()==n){node["delay"]=result["delay"]|0;node["failed"]=!result["ok"].as<bool>();node["tested"]=true;}}if(testNext>=testCount){testing=false;notice="测速完成 · 右侧可单独重测";}}
    }
    else if(!result["error"].is<const char*>() && (pg!=1 || (gi==groupIndex&&no==nodeOffset))){docs[pg]=result;lastOk[pg]=millis();if(millis()>toastUntil)notice="实时数据 · "+String(cfg.refreshMs/1000)+"秒更新";
      if(pg==0){if(historyCount==48){for(int i=1;i<48;i++){downHistory[i-1]=downHistory[i];upHistory[i-1]=upHistory[i];}historyCount=47;}downHistory[historyCount]=result["live"]["rx_bps"]|0.0;upHistory[historyCount]=result["live"]["tx_bps"]|0.0;historyCount++;}}
    else notice=result["error"].as<String>();
   }else {notice="数据请求失败 HTTP "+String(code);if(action=="delay"&&testNext>=testCount)testing=false;}
   Serial.printf("[api] page=%d HTTP=%d bytes=%u parse=%s heap=%lu\n",pg,code,body.length(),err.c_str(),ESP.getFreeHeap());
   xSemaphoreGive(lock);dirty=true;
  }
  vTaskDelay(pdMS_TO_TICKS(40));
 }
}
void drawSettings(){
 canvas.fillScreen(bg);canvas.setFont(&chinese);
 text("ROUTERDOCK / 设备设置",12,12,accent);
 text("浏览器打开 / Open in browser",12,40,muted);
 text(WiFi.status()==WL_CONNECTED?"http://"+WiFi.localIP().toString():"http://192.168.4.1",12,60,white);
 text("Hotspot: "+configPortalSSID(),12,90,accent);
 text("Login: admin",12,115,muted);
 text("Password: "+configAdminPassword(),12,137,white);
 text("热点与后台使用同一密码",12,161,muted);
 text("点此返回 / Tap to return",12,205,accent);canvas.pushSprite(0,0);
}
void draw(){
 if(settingsScreen){drawSettings();return;}
 xSemaphoreTake(lock,portMAX_DELAY);
 canvas.fillScreen(bg);canvas.setFont(&chinese);
 canvas.fillCircle(12,14,3,WiFi.status()==WL_CONNECTED?green:orange);text("OPENWRT  /  "+String(tabs[page]),22,8,white);text(WiFi.status()==WL_CONNECTED?String(WiFi.RSSI())+"dBm":"WIFI 离线",239,9,muted);
 canvas.drawFastHLine(8,27,304,border);
 JsonVariant d=docs[page].as<JsonVariant>();
 if(d.isNull()&&page!=4){text(configSnapshot().routerToken.isEmpty()?"请进入系统页配置路由器":"等待实时数据…",42,100,accent);}
 else if(page==0){
  // Dense 320x240 dashboard: resources, WAN throughput, trend and connections.
  auto tile=[&](int x,String label,String value,int pct,uint16_t color){
   canvas.fillRoundRect(x,34,96,49,6,card);text(label,x+8,39,muted);canvas.setFont(&lgfx::fonts::Font4);text(value,x+8,53,color);canvas.setFont(&chinese);
   canvas.fillRoundRect(x+8,77,80,3,1,border);if(pct>0)canvas.fillRoundRect(x+8,77,constrain(pct,1,100)*80/100,3,1,color);
  };
  tile(8,"CPU",String(d["cpu"]["pct"]|0)+"%",d["cpu"]["pct"]|0,accent);
  tile(112,"内存",String(d["mem"]["used_pct"]|0)+"%",d["mem"]["used_pct"]|0,green);
  tile(216,"温度",d["cpu"]["temp_c"].isNull()?"--":String(d["cpu"]["temp_c"].as<float>(),0)+" C",(int)(d["cpu"]["temp_c"]|0.0),orange);
  canvas.fillRoundRect(8,89,304,87,6,card);
  text("WAN 下载",18,96,accent);text("WAN 上传",173,96,green);
  canvas.setFont(&lgfx::fonts::Font4);text(bytes(d["live"]["rx_bps"]|0.0),18,110,white);text(bytes(d["live"]["tx_bps"]|0.0),173,110,white);canvas.setFont(&chinese);
  text("/s",133,119,muted);text("/s",284,119,muted);
  double peak=1024;for(int i=0;i<historyCount;i++)peak=max(peak,(double)max(downHistory[i],upHistory[i]));
  canvas.drawFastHLine(18,167,284,border);for(int i=1;i<historyCount;i++){int x1=18+(i-1)*284/47,x2=18+i*284/47;canvas.drawLine(x1,166-downHistory[i-1]/peak*27,x2,166-downHistory[i]/peak*27,accent);canvas.drawLine(x1,166-upHistory[i-1]/peak*27,x2,166-upHistory[i]/peak*27,green);}
  canvas.fillRoundRect(8,182,146,19,4,card);canvas.fillRoundRect(162,182,150,19,4,card);
  text("Clash连接",15,186,muted);text(d["clash"]["available"]==true?String(d["clash"]["connections"]|0):"--",98,186,accent);
  text("已运行 "+String((d["uptime_s"]|0)/3600)+"h "+String(((d["uptime_s"]|0)/60)%60)+"m",171,186,muted);
 }else if(page==1){
  if(d["ok"]!=true){text("Clash API 不可用",88,100,accent);text("请检查路由器采集端配置",62,122,muted);}
  else if(connectionView){text("活跃连接 / 下载量前五 · 点此返回",12,38,accent);int y=62;JsonArray tops=d["top"].as<JsonArray>();for(auto c:tops){text(c["host"]|"--",12,y);text(String(c["src"]|"")+"  "+bytes(c["download"]|0.0),12,y+14,muted);y+=28;}}else{
  auto gs=d["groups"].as<JsonArray>();int count=gs.size();if(groupIndex>count&&count){groupIndex=1;forcePoll=true;}
  auto g=gs[groupIndex-1];String name=g["name"]|"等待策略组";
  text(name,12,35,accent);text("当前: "+String(g["now"]|"--"),12,51);
  button("< 组",8,70,55);button("组 >",67,70,55);button(testing?"测试中":"测速",126,70,62);button("< 页",192,70,55);button("页 >",251,70,61);
  auto ns=g["nodes"].as<JsonArray>();int i=0;for(auto v:ns){
   int y=102+i*23;bool selected=v["name"].as<String>()==String(g["now"]|"");canvas.fillRoundRect(8,y,304,21,4,card);if(selected)canvas.fillRoundRect(8,y+3,3,15,1,accent);
   canvas.setFont(&chinese);canvas.setTextColor(selected?accent:white);canvas.setClipRect(17,y+4,220,14);canvas.drawString(displayText(v["name"].as<String>()),17,y+4);canvas.clearClipRect();
   int delay=v["delay"]|0;String ds=v["failed"]==true?"超时":delay>0?String(delay)+"ms":"--";
   text(ds,245,y+4,v["failed"]==true?0xF986:delay>0&&delay<200?green:orange);i++;
  }
  text("连接 "+String(d["connections"]|0)+"  "+String(nodeOffset+1)+"-"+String(min(nodeOffset+4,g["count"]|0))+"/"+String(g["count"]|0)+"   连接详情 >",12,197,muted);}
 }else if(page==2){
  auto s=d["stats"];metric("保留记录 · 改写",String(s["rewrite_total"]|0),12,38);metric("改写比例",String(s["rewrite_ratio_pct"]|0.0,1)+"%",164,38);
  row("服务状态",d["installed"]!=true?"未安装":d["running"]==true?"运行中":"未运行",103);row("连接 / 放行",String(s["conn_total"]|0)+" / "+String(s["pass_total"]|0),121);
  auto r=d["recent"].as<JsonArray>();if(r.size())text(r[r.size()-1]["dst"].as<String>().substring(0,45),16,144,muted);
  button("重启 UA3F",12,169,110);text("统计为当前保留日志",130,177,muted);
 }else if(page==3){
  metric("下载速率",bytes(d["now"]["rx_bps"]|0.0)+"/s",12,38);metric("上传速率",bytes(d["now"]["tx_bps"]|0.0)+"/s",164,38);
  row("累计下载",bytes(d["total"]["rx_b"]|0.0),102);row("累计上传",bytes(d["total"]["tx_b"]|0.0),120);text("来源: "+String(d["source"]|"--"),16,139,muted);
  auto series=d["series"].as<JsonArray>();double peak=1;for(int i=1;i<(int)series.size();i++){double dt=series[i]["t"].as<double>()-series[i-1]["t"].as<double>();double rate=dt>0?(series[i]["rx_b"].as<double>()-series[i-1]["rx_b"].as<double>())/dt:0;peak=max(peak,rate);}
  int px=14,py=193;for(int i=1;i<(int)series.size();i++){double dt=series[i]["t"].as<double>()-series[i-1]["t"].as<double>();double r=dt>0?(series[i]["rx_b"].as<double>()-series[i-1]["rx_b"].as<double>())/dt:0;int x=14+i*290/max(1,(int)series.size()-1),y=193-max(0,(int)(r/peak*35));canvas.drawLine(px,py,x,y,accent);px=x;py=y;}
 }else{
  if(systemDetails){row("内存 已用/总量",bytes((d["mem"]["used_kb"]|0.0)*1024)+"/"+bytes((d["mem"]["total_kb"]|0.0)*1024),40);row("磁盘占用",String(d["disk"]["used_pct"]|0)+"%",60);row("WAN 链路",String(d["net"]["wan"]["speed_mbps"]|0)+" Mbps",80);int y=105;JsonArray procs=d["proc"]["top"].as<JsonArray>();for(auto v:procs){text(String(v["name"]|"")+"   "+String(v["cpu_pct"]|0.0,1)+"%   "+bytes((v["mem_kb"]|0.0)*1024),16,y);y+=17;}text("点上方标题返回",16,190,muted);}
  else {row("路由器",configSnapshot().routerHost,40);row("设备 IP",WiFi.localIP().toString(),58);row("可用堆内存",bytes(ESP.getFreeHeap()),76);row("主机",d["host"]|"--",94);
  button("重启 Clash",12,121,142);button("更新订阅",166,121,142);button("设备后台 / 配网",12,157,142);button("触摸校准",166,157,142);text("点标题查看内存/磁盘/进程",16,190,muted);}
 }
 if(confirm){canvas.fillRoundRect(12,90,296,100,8,card);text("确认执行此操作？",50,108);button("取消",30,146,110);button("确认",170,146,110);}
 String footer=notice;if(lastOk[page]&&millis()-lastOk[page]>10000)footer="数据已过期 · "+notice;
 if(page!=1)text(footer,12,205,muted);else if(testing)text("正在测试 "+String(testNext)+"/"+String(testCount),188,8,orange);
 canvas.fillRect(0,219,320,21,card);for(int i=0;i<5;i++){if(page==i){canvas.fillRoundRect(i*64+3,220,58,20,4,bg);canvas.drawFastHLine(i*64+20,219,24,accent);}text(tabs[i],i*64+17,225,page==i?accent:muted);}
 canvas.pushSprite(0,0);xSemaphoreGive(lock);
}
void calibrate(){lcd.fillScreen(bg);lcd.setFont(&chinese);lcd.setTextColor(white);lcd.drawString("请依次按住屏幕上的十字",10,100);uint16_t data[8];lcd.calibrateTouch(data,accent,bg,12);prefs.putBytes("touch",data,sizeof(data));lcd.setTouchCalibrate(data);dirty=true;}
void tap(int x,int y){
 if(settingsScreen){settingsScreen=false;dirty=true;return;}
 if(confirm){if(y>=140&&y<182){int a=confirm;confirm=0;if(x>160){if(a==1)queueAction("restart_clash");if(a==2)queueAction("update");if(a==3)queueAction("restart_ua");}dirty=true;}return;}
 if(y>=218){page=min(4,x/64);forcePoll=true;dirty=true;return;}
 if(page==0&&y>=182&&y<202&&x<155){page=1;connectionView=true;forcePoll=true;dirty=true;return;}
 if(page==4&&y<31){systemDetails=!systemDetails;dirty=true;return;}
 if(page==4&&!systemDetails){if(y>=121&&y<148)confirm=x<160?1:2;if(y>=157&&y<185){if(x<160){configStartPortal();dirty=true;}else calibrate();}dirty=true;}
 if(page==2&&y>=169&&y<198){confirm=3;dirty=true;}
 if(page==1){
  if(connectionView){connectionView=false;dirty=true;return;}
  if(y>=195&&y<217){connectionView=true;dirty=true;return;}
  if(y>=70&&y<96&&x>=126&&x<188){startTests();return;}
  xSemaphoreTake(lock,portMAX_DELAY);auto gs=docs[1]["groups"].as<JsonArray>();auto g=gs[groupIndex-1];String gn=g["name"]|"",nn;
  if(y>=70&&y<96){if(x<126){groupIndex+=x<65?-1:1;groupIndex=constrain(groupIndex,1,max(1,(int)gs.size()));nodeOffset=0;}else{nodeOffset+=x<248?-4:4;nodeOffset=constrain(nodeOffset,0,max(0,((g["count"]|1)-1)/4*4));}forcePoll=true;}
  if(y>=102&&y<194&&x<240&&String(g["type"]|"")=="Selector")nn=g["nodes"][(y-102)/23]["name"]|"";
  xSemaphoreGive(lock);if(y>=102&&y<194&&x>=240)startTests((y-102)/23);else if(nn!="")queueAction("switch",gn,nn);dirty=true;
 }
}
void setup(){Serial.begin(115200);Serial.setTimeout(100);pinMode(21,OUTPUT);digitalWrite(21,HIGH);for(int p:{22,16,17}){pinMode(p,OUTPUT);digitalWrite(p,HIGH);}lcd.init();lcd.setRotation(1);prefs.begin("monitor",false);uint16_t cal[8];if(prefs.getBytesLength("touch")==sizeof(cal)){prefs.getBytes("touch",cal,sizeof(cal));lcd.setTouchCalibrate(cal);}
 canvas.setColorDepth(8);if(!canvas.createSprite(320,240)){Serial.println("framebuffer allocation failed");while(true)delay(1000);}lock=xSemaphoreCreateMutex();configBegin();draw();
 xTaskCreatePinnedToCore(fetchTask,"network",10000,nullptr,1,nullptr,0);Serial.println("[boot] terminal ready; serial commands: status, calibrate, page 0..4");}
void loop(){configLoop();static uint32_t heartbeat=0;if(millis()-heartbeat>1000){heartbeat=millis();dirty=true;}static bool down=false;uint16_t x,y;bool pressed=lcd.getTouch(&x,&y);if(pressed&&!down&&x<320&&y<240){Serial.printf("[touch] %u,%u\n",x,y);tap(x,y);}down=pressed;
 bool wifi=WiFi.status()==WL_CONNECTED;if(wifi!=connected){connected=wifi;dirty=true;Serial.printf("[wifi] status=%d ip=%s\n",WiFi.status(),WiFi.localIP().toString().c_str());}
 
 if(Serial.available()){String cmd=Serial.readStringUntil('\n');cmd.trim();if(cmd=="calibrate")calibrate();if(cmd=="portal"){configStartPortal();dirty=true;}if(cmd.startsWith("configure "))configSerialProvision(cmd.substring(10));if(cmd.startsWith("tap ")){int tx,ty;if(sscanf(cmd.c_str(),"tap %d %d",&tx,&ty)==2&&tx>=0&&tx<320&&ty>=0&&ty<240)tap(tx,ty);}if(cmd.startsWith("page ")){page=constrain(cmd.substring(5).toInt(),0,4);forcePoll=true;}if(cmd=="shot"){xSemaphoreTake(lock,portMAX_DELAY);Serial.println("RGBBEGIN");for(int y=0;y<240;y++)for(int x=0;x<320;x++){uint16_t c=canvas.readPixel(x,y);Serial.write(c>>8);Serial.write(c&255);}Serial.println("RGBEND");xSemaphoreGive(lock);}if(cmd=="status")Serial.printf("wifi=%d heap=%lu page=%d\n",WiFi.status(),ESP.getFreeHeap(),page);dirty=true;}
 if(dirty){dirty=false;draw();}delay(12);}
