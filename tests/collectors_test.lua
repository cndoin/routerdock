-- Deterministic fixtures for Linux collectors, including a non-x86 board.
local replies={};package.preload['luci.jsonc']=function()return {parse=function(s)return replies[s] or {} end}end
package.preload['nixio']=function()return {nanosleep=function()end}end
local files={['/proc/cpuinfo']='processor : 0\nprocessor : 1\nHardware : Example ARM SoC\n', ['/proc/meminfo']='MemTotal: 1000 kB\nMemAvailable: 400 kB\nMemFree: 100 kB\nBuffers: 50 kB\nCached: 200 kB\n',['/proc/uptime']='1000.00 2000.00\n',['/proc/sys/kernel/hostname']='fixture-router\n',['/sys/class/thermal/thermal_zone0/type']='acpitz',['/sys/class/thermal/thermal_zone0/temp']='28000',['/sys/class/thermal/thermal_zone1/type']='cpu-thermal',['/sys/class/thermal/thermal_zone1/temp']='38000',['/sys/class/net/eth2/statistics/rx_bytes']='12000000000',['/sys/class/net/eth2/statistics/tx_bytes']='5000000000',['/sys/class/net/eth2/speed']='2500',['/sys/class/net/eth2/mtu']='1500'}
local sample=0;local custom=false
replies.WAN={up=true,l3_device='eth2',['ipv4-address']={{address='192.0.2.1'}}}
local function handle(s)return {read=function()return s end,close=function()end}end
io.open=function(p)
 if p=='/proc/stat' then sample=sample+1;return handle(sample%2==1 and 'cpu 100 0 50 850 0 0 0 0\n' or 'cpu 120 0 70 1010 0 0 0 0\n') end
 if files[p]~=nil then return handle(files[p]) end
 return nil
end
io.popen=function(cmd)
 local out=''
 if cmd:find('network.interface.wan',1,true) or cmd:find('network.interface.uplink',1,true) then out='WAN'
 elseif custom and cmd:find('routerdock.main.wan_interface',1,true) then out='uplink\n'
 elseif cmd:find('ls -d /sys/class/thermal/',1,true) then out='/sys/class/thermal/thermal_zone0\n/sys/class/thermal/thermal_zone1\n'
 elseif cmd:find('df -Pk',1,true) then out='Filesystem 1024-blocks Used Available Capacity Mounted on\n/dev/root 1000 300 700 30% /\n'
 elseif cmd:find('getconf CLK_TCK',1,true) then out='100'
 elseif cmd:find('getconf PAGESIZE',1,true) then out='4096'
 end
 return handle(out)
end
local M=dofile('openwrt/collectors.lua');local s=M.status()
assert(s.cpu.model=='Example ARM SoC');assert(s.cpu.cores==2);assert(s.cpu.pct==20);assert(s.cpu.temp_c==38)
assert(s.mem.used_pct==60 and s.mem.used_kb==600);assert(s.disk.used_pct==30)
assert(s.net.wan.rx_b==12000000000 and s.net.wan.tx_b==5000000000 and s.net.wan.device=='eth2')
custom=true;assert(M.net('wan').device=='eth2')
files['/sys/class/thermal/thermal_zone0/temp']=nil;files['/sys/class/thermal/thermal_zone1/temp']=nil
files['/proc/meminfo']='MemTotal: 1000 kB\nMemFree: 100 kB\nBuffers: 50 kB\nCached: 200 kB\n'
s=M.status();assert(s.cpu.temp_c==nil);assert(s.mem.used_pct==65)
local u=M.ua();assert(not u.installed and not u.running and u.stats.conn_total==0 and not u.stats_available)
print('PASS: ARM-style cpuinfo, CPU delta, MemAvailable/fallback, CPU-vs-ACPI temperature, unavailable sensors, large counters, custom WAN and absent UA3F (fixtures).')
