-- Linux/OpenWrt collectors. No CPU-architecture-specific binaries are used.
local M={}
local json=require('luci.jsonc')
local nixio=require('nixio')
local function read(path) local f=io.open(path);if not f then return '' end local s=f:read('*a');f:close();return s end
local function run(command) local p=io.popen(command..' 2>/dev/null');if not p then return '' end local s=p:read('*a');p:close();return s end
local function quote(s) return "'"..tostring(s):gsub("'","'\\''").."'" end
local function parse(s) return json.parse(s) or {} end
function M.uci(key) return (run('uci -q get '..quote(key)):gsub('%s+$','')) end
local function value(path) return tonumber(read(path)) end
local function cpu()
 local t,idle=0,0;local line=read('/proc/stat'):match('^cpu%s+([^\n]+)') or '';local i=0
 for n in line:gmatch('%d+') do i=i+1;n=tonumber(n);if i<=8 then t=t+n end;if i==4 or i==5 then idle=idle+n end end
 return t,idle
end
local function temperature()
 local preferred,other={},{ }
 for _,p in ipairs({'/sys/class/thermal/thermal_zone*','/sys/class/hwmon/hwmon*'}) do
  for dir in run('ls -d '..p):gmatch('[^\n]+') do
   local name=(read(dir..'/type')..read(dir..'/name')):lower();local v=value(dir..'/temp') or value(dir..'/temp1_input')
   if v then if math.abs(v)>=1000 then v=v/1000 end;if v>=-20 and v<=150 then
    if name:match('cpu') or name:match('package') or name:match('soc') or name:match('coretemp') then preferred[#preferred+1]=v elseif not name:match('drive') and not name:match('nvme') then other[#other+1]=v end
   end end
  end
 end
 local list=#preferred>0 and preferred or other;if #list==0 then return nil end;table.sort(list);return list[#list]
end
function M.net(logical)
 local configured=M.uci('routerdock.main.'..logical..'_interface');if configured~='' then logical=configured end
 if not logical:match('^[%w_%-]+$') then return {up=false} end
 local s=parse(run('ubus -S call '..quote('network.interface.'..logical)..' status'));local dev=s.l3_device or s.device
 if not dev and logical=='wan' then for line in read('/proc/net/route'):gmatch('[^\n]+') do local d,dest=line:match('^(%S+)%s+(%x+)');if dest=='00000000' then dev=d;break end end end
 if not dev or not dev:match('^[%w%._%-]+$') then return {up=false} end
 local root='/sys/class/net/'..dev
 return {device=dev,up=s.up==true,ip=((s['ipv4-address'] or {})[1] or {}).address,rx_b=value(root..'/statistics/rx_bytes'),tx_b=value(root..'/statistics/tx_bytes'),speed_mbps=value(root..'/speed') or 0,mtu=value(root..'/mtu')}
end
function M.status()
 local total,idle=cpu();nixio.nanosleep(0,150000000);local total2,idle2=cpu();local dt=total2-total
 local pct=dt>0 and math.max(0,math.min(100,math.floor((dt-idle2+idle)*100/dt+.5))) or 0
 local info=read('/proc/cpuinfo');local cores=0;for _ in info:gmatch('processor%s*:') do cores=cores+1 end
 local model=info:match('model name%s*:%s*([^\n]+)') or info:match('Hardware%s*:%s*([^\n]+)') or info:match('system type%s*:%s*([^\n]+)') or 'OpenWrt'
 local mem={};for k,v in read('/proc/meminfo'):gmatch('([%w_]+):%s*(%d+)') do mem[k]=tonumber(v) end
 local available=mem.MemAvailable or ((mem.MemFree or 0)+(mem.Buffers or 0)+(mem.Cached or 0));local used=math.max(0,(mem.MemTotal or 0)-available)
 local disk={};for line in run('df -Pk /'):gmatch('[^\n]+') do local t,u,p=line:match('^%S+%s+(%d+)%s+(%d+)%s+%d+%s+(%d+)%%');if t then disk={total_kb=tonumber(t),used_kb=tonumber(u),used_pct=tonumber(p)} end end
 local up=tonumber(read('/proc/uptime'):match('^[%d%.]+')) or 0
 local processes={};local hz=tonumber(run('getconf CLK_TCK')) or 100;local pagesize=tonumber(run('getconf PAGESIZE')) or 4096
 -- Fixed command, streamed /proc reads; CPU is lifetime average, not interval usage.
 local raw=run([[awk 'FILENAME ~ /\/stat$/ {s=$0; sub(/^.*\) /,"",s); split(s,a," "); pid=FILENAME;sub("/stat$","",pid);name=$0;sub(/^[^(]*\(/,"",name);sub(/\).*/,"",name);ticks=a[12]+a[13];rss=a[22];printf "%.0f\t%.0f\t%s\n",ticks,rss,name}' /proc/[0-9]*/stat | sort -nr | head -5]])
 for line in raw:gmatch('[^\n]+') do local ticks,rss,name=line:match('^(%d+)%s+(%d+)%s+(.+)$');if ticks then processes[#processes+1]={name=name,cpu_pct=up>0 and tonumber(ticks)/hz/up*100 or 0,mem_kb=tonumber(rss)*pagesize/1024} end end
 return {v=4,ts=os.time(),host=read('/proc/sys/kernel/hostname'):gsub('%s+$',''),uptime_s=math.floor(up),cpu={pct=pct,cores=cores,model=model:sub(1,48),mhz=tonumber(info:match('cpu MHz%s*:%s*([%d%.]+)')),temp_c=temperature()},mem={total_kb=mem.MemTotal,used_kb=used,used_pct=mem.MemTotal and math.floor(used/mem.MemTotal*100+.5)},disk=disk,proc={top=processes,scope='lifetime_average'},net={wan=M.net('wan'),lan=M.net('lan')}}
end
function M.ua()
 local dir='/var/log/ua3f';local function count(file) return tonumber(run('wc -l < '..quote(dir..'/'..file))) or 0 end
 local installed=read('/etc/init.d/ua3f')~='';local pid=tonumber(run('pidof ua3f'):match('%d+'))
 local conn,pass,rewrite=count('conn_stats'),count('pass_stats'),count('rewrite_stats');local recent={}
 for line in run('tail -4 '..quote(dir..'/conn_stats')):gmatch('[^\n]+') do local p,src,dst,ms=line:match('^(%S+)%s+(%S+)%s+(%S+)%s+(%d+)');if dst then recent[#recent+1]={proto=p,src=src,dst=dst,ms=tonumber(ms)} end end
 return {v=4,installed=installed,running=pid~=nil,pid=pid,server_mode=M.uci('ua3f.main.server_mode'),port=tonumber(M.uci('ua3f.main.port')),stats_scope='retained_log_lines',stats_available=conn>0 or pass>0 or rewrite>0,stats={conn_total=conn,pass_total=pass,rewrite_total=rewrite,rewrite_ratio_pct=(pass+rewrite)>0 and rewrite/(pass+rewrite)*100 or 0},recent=recent}
end
return M
