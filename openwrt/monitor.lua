#!/usr/bin/lua
-- RouterDock authenticated device API. Secrets stay on the router.
local json=require('luci.jsonc')
local sys=require('luci.sys')
local collectors=dofile('/usr/share/routerdock/collectors.lua')
local function read(p) local f=io.open(p); if not f then return '' end local s=f:read('*a');f:close();return s end
local function q(s) return "'"..tostring(s):gsub("'", "'\\''").."'" end
local function run(s) local p=io.popen(s..' 2>/dev/null');local v=p:read('*a');p:close();return v end
local function parse(s) return json.parse(s) or {} end
local function emit(o,status) if status then io.write('Status: '..status..'\r\n') end io.write('Content-Type: application/json\r\nCache-Control: no-store\r\n\r\n'..json.stringify(o)) end
local token=read('/etc/routerdock/token'):gsub('%s+$','')
if token=='' or os.getenv('HTTP_AUTHORIZATION')~='Bearer '..token then emit({ok=false,error='unauthorized'},'401 Unauthorized');return end
local endpoint=(os.getenv('QUERY_STRING') or ''):match('^([a-z]+)') or 'status'
local function script(name) return parse(run('sh /usr/share/routerdock/'..name)) end
local cfg=read('/etc/openclash/config.yaml')
local port=cfg:match('external%-controller:%s*[^\n]-:(%d+)') or collectors.uci('openclash.config.cn_port')
if not port:match('^%d+$') then port='9090' end
local controller=collectors.uci('routerdock.main.controller_url')
if controller=='' then controller='http://127.0.0.1:'..port end
controller=controller:gsub('/+$','')
local secret=collectors.uci('routerdock.main.controller_secret')
if secret=='' then secret=cfg:match('\nsecret:%s*([^\n]+)') or collectors.uci('openclash.config.dashboard_password') end
secret=secret:gsub('^["\']',''):gsub('["\']$','')
local function enc(s) return (s:gsub('([^%w%-_%.~])',function(c)return string.format('%%%02X',c:byte())end)) end
local function api(path,method,data)
 local cmd='curl -s --max-time 5 -w '..q('\n%{http_code}')..' -H '..q('Authorization: Bearer '..secret)..' -H '..q('Content-Type: application/json')..' -X '..q(method or 'GET')
 if data then cmd=cmd..' --data '..q(json.stringify(data)) end
 local out=run(cmd..' '..q(controller..path))
 local body,code=out:match('^(.*)\n(%d+)$'); return parse(body or ''),tonumber(code) or 0
end
local function savejson(path,obj)
 local tmp=path..'.'..require('nixio').getpid();local f=io.open(tmp,'w');if f then f:write(json.stringify(obj));f:close();os.rename(tmp,path) end
end
local function main()
 if endpoint=='status' then
  local o=collectors.status()
  local c,code=api('/connections');o.clash={available=code==200,connections=code==200 and #(c.connections or {}) or nil}
  local w=o.net and o.net.wan or {};local now=tonumber(read('/proc/uptime'):match('^[%d%.]+')) or 0
  local old=parse(read('/tmp/routerdock-wan.json'));local dt=now-(old.t or now)
  o.live={source='wan',rx_bps=dt>0 and old.rx and w.rx_b and w.rx_b>=old.rx and (w.rx_b-old.rx)/dt or 0,tx_bps=dt>0 and old.tx and w.tx_b and w.tx_b>=old.tx and (w.tx_b-old.tx)/dt or 0}
  savejson('/tmp/routerdock-wan.json',{t=now,rx=w.rx_b,tx=w.tx_b});return o
 elseif endpoint=='ua' then
  local o=collectors.ua(); o.stats_scope='retained_log_lines';return o
 elseif endpoint=='traffic' then
  local c,code=api('/connections');local source='clash_api'
  local wan=collectors.net('wan').device
  local function counter(which) if not wan or not wan:match('^[%w%._%-]+$') then return nil end return tonumber(read('/sys/class/net/'..wan..'/statistics/'..which..'_bytes')) end
  local rx,tx=tonumber(c.downloadTotal),tonumber(c.uploadTotal)
  if code~=200 or not rx or not tx then source='netdev';rx=counter('rx');tx=counter('tx') end
  if not rx or not tx then return {error='traffic unavailable'} end
  local old=parse(read('/tmp/routerdock-series.json'));local series=old.series or {};local now=os.time();local last=series[#series]
  if old.source~=source or (last and (last.rx_b>rx or last.tx_b>tx or last.t>now)) then series={};last=nil end
  local dt=last and now-last.t or 0
  local rates={rx_bps=dt>0 and math.max(0,(rx-last.rx_b)/dt) or 0,tx_bps=dt>0 and math.max(0,(tx-last.tx_b)/dt) or 0}
  if not last or dt>0 then series[#series+1]={t=now,rx_b=rx,tx_b=tx} end
  while #series>90 do table.remove(series,1) end
  local tmp='/tmp/routerdock-series.'..require('nixio').getpid();local f=io.open(tmp,'w')
  if f then f:write(json.stringify({source=source,series=series}));f:close();os.rename(tmp,'/tmp/routerdock-series.json') end
  return {v=4,source=source,now=rates,total={rx_b=rx,tx_b=tx},series=series,wan={rx_b=counter('rx'),tx_b=counter('tx')}}
 elseif endpoint=='clash' then
  local p,code=api('/proxies');local c=api('/connections');local v=api('/version');local groups={}
  for name,g in pairs(p.proxies or {}) do if g.all then
   local tests=parse(read('/tmp/routerdock-delays.json'));local nodes={};for _,n in ipairs(g.all) do local a=(p.proxies or {})[n] or {}; local h=a.history or {}; local t=tests[n];if t and os.time()-(t.ts or 0)>300 then t=nil end;nodes[#nodes+1]={name=n,delay=t and t.delay or (h[#h] or {}).delay or 0,tested=t~=nil,failed=t and t.failed or false} end
   groups[#groups+1]={name=name,now=g.now or '',type=g.type,nodes=nodes}
  end end
  table.sort(groups,function(a,b)return a.name<b.name end)
  local gi=tonumber((os.getenv('QUERY_STRING') or ''):match('g=(%d+)')) or 1
  local offset=tonumber((os.getenv('QUERY_STRING') or ''):match('p=(%d+)')) or 0
  for i,g in ipairs(groups) do g.count=#g.nodes;if i~=gi then g.nodes={} else local page={};for n=offset+1,math.min(offset+4,#g.nodes) do page[#page+1]=g.nodes[n] end;g.nodes=page end end
  local top={};for _,a in ipairs(c.connections or {}) do local m=a.metadata or {};top[#top+1]={host=m.host~='' and m.host or m.destinationIP,src=m.sourceIP,upload=a.upload,download=a.download} end
  table.sort(top,function(a,b)return (a.download or 0)>(b.download or 0)end)
  while #top>5 do table.remove(top) end
  return {v=4,ok=code==200,version=v.version,groups=groups,connections=#(c.connections or {}),download=c.downloadTotal,upload=c.uploadTotal,top=top}
 elseif endpoint=='action' then
  if os.getenv('REQUEST_METHOD')~='POST' then return {ok=false,error='POST required'} end
  local len=tonumber(os.getenv('CONTENT_LENGTH')) or 0
  if len<2 or len>2048 then return {ok=false,error='invalid body size'} end
  local b=parse(io.read(len)); local a=b.action;local args=b.args or {}
  if a=='delay' then
   if type(args.group)~='string' or type(args.node)~='string' then return {ok=false,error='missing node'} end
   local g,code=api('/proxies/'..enc(args.group));local found=false
   for _,n in ipairs(g.all or {}) do if n==args.node then found=true end end
   if code~=200 or not found then return {ok=false,error='invalid group/node'} end
   local d,dc=api('/proxies/'..enc(args.node)..'/delay?timeout=2000&url='..enc('https://www.gstatic.com/generate_204'))
   local delay=tonumber(d.delay);local ok=dc==200 and delay~=nil
   local tests=parse(read('/tmp/routerdock-delays.json'));tests[args.node]={delay=ok and delay or 0,failed=not ok,ts=os.time()};savejson('/tmp/routerdock-delays.json',tests)
   return {ok=ok,node=args.node,delay=delay,msg=ok and ('延迟 '..delay..' ms') or nil,error=not ok and '超时或不可达' or nil}
  elseif a=='switch' then
   if type(args.group)~='string' or type(args.node)~='string' then return {ok=false,error='missing node'} end
   local g,code=api('/proxies/'..enc(args.group));local found=false
   for _,n in ipairs(g.all or {}) do if n==args.node then found=true end end
   if code~=200 or g.type~='Selector' or not found then return {ok=false,error='invalid selector/node'} end
   local _,code=api('/proxies/'..enc(args.group),'PUT',{name=args.node});return {ok=code==204,error=code~=204 and 'Clash HTTP '..code or nil}
  elseif a=='restart_clash' or a=='restart_ua' or a=='update' then
   local cmd
   if a=='update' then
    if read('/usr/share/openclash/openclash.sh')=='' then return {ok=false,error='subscription updater missing'} end
    cmd='/usr/share/openclash/openclash.sh'
   else local service=a=='restart_clash' and 'openclash' or 'ua3f';if read('/etc/init.d/'..service)=='' then return {ok=false,error='service not installed'} end;cmd='/etc/init.d/'..service..' restart' end
   sys.call('('..cmd..' > /tmp/routerdock-action.log 2>&1) </dev/null >/dev/null 2>&1 &')
   return {ok=true,msg='任务已提交，稍后刷新状态'}
  end
  return {ok=false,error='unknown_action'}
 end
 return {ok=false,error='unknown endpoint'}
end
local ok,o=pcall(main);if ok then emit(o) else emit({ok=false,error='collector failed'},'500 Internal Server Error') end
