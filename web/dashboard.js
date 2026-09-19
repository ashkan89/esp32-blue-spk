
const $=s=>document.querySelector(s),$$=s=>[...document.querySelectorAll(s)];
const paths={speaker:'<path d="M4 9v6h4l5 4V5L8 9H4m13 1c1.5 2 1.5 4 0 6m3-9c3.5 3.5 3.5 6.5 0 10"/>',home:'<path d="M3 11 12 3l9 8v9a1 1 0 0 1-1 1h-5v-7H9v7H4a1 1 0 0 1-1-1z"/>',bluetooth:'<path d="m7 7 10 10-5 4V3l5 4L7 17"/>',wifi:'<path d="M3 8.5a14 14 0 0 1 18 0M6.5 12a9 9 0 0 1 11 0M10 15.5a4 4 0 0 1 4 0M12 19h.01"/>',download:'<path d="M12 3v12m-5-5 5 5 5-5M4 21h16"/>',settings:'<circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.7 1.7 0 0 0 .34 1.88l.06.06-2.83 2.83-.06-.06a1.7 1.7 0 0 0-1.88-.34 1.7 1.7 0 0 0-1.03 1.56V21h-4v-.08A1.7 1.7 0 0 0 9 19.37a1.7 1.7 0 0 0-1.88.34l-.06.06-2.83-2.83.06-.06A1.7 1.7 0 0 0 4.63 15 1.7 1.7 0 0 0 3.08 14H3v-4h.08A1.7 1.7 0 0 0 4.63 9a1.7 1.7 0 0 0-.34-1.88l-.06-.06 2.83-2.83.06.06A1.7 1.7 0 0 0 9 4.63h.02A1.7 1.7 0 0 0 10 3.08V3h4v.08a1.7 1.7 0 0 0 1 1.55 1.7 1.7 0 0 0 1.88-.34l.06-.06 2.83 2.83-.06.06A1.7 1.7 0 0 0 19.37 9v.02A1.7 1.7 0 0 0 20.92 10H21v4h-.08A1.7 1.7 0 0 0 19.4 15z"/>',refresh:'<path d="M20 6v5h-5M4 18v-5h5M18.5 9a7 7 0 0 0-12-2L4 11m16 2-2.5 4a7 7 0 0 1-12-2"/>',scan:'<path d="M3 8V3h5m8 0h5v5M3 16v5h5m8 0h5v-5M8 12h8"/>',play:'<path fill="currentColor" stroke="none" d="m8 5 11 7-11 7z"/>',pause:'<path fill="currentColor" stroke="none" d="M7 5h4v14H7zm7 0h4v14h-4z"/>',stop:'<rect x="6" y="6" width="12" height="12" rx="1" fill="currentColor" stroke="none"/>',previous:'<path d="M19 20 9 12l10-8zM5 19V5"/>',next:'<path d="m5 4 10 8-10 8zm14 1v14"/>',rewind:'<path d="m11 19-9-7 9-7zm11 0-9-7 9-7z"/>',forward:'<path d="m13 5 9 7-9 7zM2 5l9 7-9 7z"/>',volume:'<path d="M4 10v4h4l5 4V6l-5 4zm13 0c1.5 1.5 1.5 2.5 0 4m2-7c4 3.5 4 6.5 0 10"/>',clock:'<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>',sdcard:'<path d="M7 3h7l4 4v13a1 1 0 0 1-1 1H7a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1z"/><path d="M10 6v3m2.5-3v3M15 6v3"/>',battery:'<rect x="2" y="8" width="17" height="9" rx="2"/><path d="M21 11v3" stroke-width="3"/>',tune:'<path d="M4 6h10M18 6h2M4 12h4M12 12h8M4 18h12M20 18h0"/><circle cx="16" cy="6" r="2"/><circle cx="10" cy="12" r="2"/><circle cx="18" cy="18" r="2"/>',radio:'<path d="M12 11v10M8 21h8"/><path d="M8.5 7.5a5 5 0 0 1 7 0M5.5 4.5a9 9 0 0 1 13 0"/><circle cx="12" cy="11" r="1.6"/>',alarm:'<path d="M12 8v5l3 2"/><circle cx="12" cy="13" r="8"/><path d="m5 3 3 2M19 3l-3 2"/>',chart:'<path d="M4 20V6M4 20h16"/><path d="m7 15 4-5 3 3 5-7"/>',hass:'<path d="m3 11 9-8 9 8v9a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1z"/><path d="M12 9v5m-3-2h6"/>',bulb:'<path d="M9 18h6m-5 3h4M12 3a6 6 0 0 1 3.7 10.7c-.5.4-.7 1-.7 1.6V17H9v-1.7c0-.6-.2-1.2-.7-1.6A6 6 0 0 1 12 3z"/>'};
// The phone bar mirrors the sidebar rather than keeping its own list: the
// sidebar grew to twelve pages while the bar stayed at a hand-picked seven, and
// five pages were simply unreachable on a phone. Labels are shortened where the
// full name would not fit under an icon.
(function(){const SHORT={overview:'Home',lighting:'Lights',updates:'Update',hass:'HA'};$('#mobileNav').innerHTML=$$('#nav button').map(b=>`<button${b.classList.contains('active')?' class="active"':''} data-page="${b.dataset.page}" data-icon="${b.dataset.icon}">${SHORT[b.dataset.page]||b.textContent}</button>`).join('')})();
function icons(){$$('[data-icon]').forEach(e=>{let n=e.dataset.icon;if(paths[n]&&!e.querySelector('svg'))e.insertAdjacentHTML('afterbegin',`<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">${paths[n]}</svg>`)})}icons();
/* Capabilities. Fetched once, unauthenticated, before anything else: it
   describes the board and the feature set, never the network or what is
   playing. The dashboard is served by both targets and has to decide what to
   draw before it knows which one it is talking to.

   This only decides what is DRAWN. Every restriction is enforced again in the
   handler that would act on it, so a stale cached page cannot reach a feature
   this board does not have -- it just gets a 409 with the same reason. */
let caps=null;
/* Capability name -> page id, for the ones that own a whole page. Written out
   rather than derived: 'dfplayer' is the Media page and 'bluetooth' is the
   Devices page, and neither is ever hidden, so a name-equals-page rule would
   be wrong in both directions. */
const CAP_PAGES={radio:'radio'};
async function applyCapabilities(){
 try{const r=await fetch('/api/capabilities');if(!r.ok)return;caps=await r.json()}catch(e){return}
 if(!caps||!caps.capabilities)return;
 const b=caps.board||{};
 if(b.module)$('#sideVersion').textContent=b.module;
 let hidActive=false;
 for(const [cap,pg] of Object.entries(CAP_PAGES)){
  const c=caps.capabilities[cap];
  if(!c||c.available)continue;
  const navs=$$(`#nav [data-page="${pg}"],#mobileNav [data-page="${pg}"]`),sec=$(`#page-${pg}`);
  for(const nav of navs){if(nav.classList.contains('active'))hidActive=true;nav.style.display='none'}
  if(sec){sec.classList.remove('active');sec.dataset.reason=c.reason||''}
 }
 /* If the page that was open is the one just hidden, go somewhere real rather
    than leaving an empty pane. */
 if(hidActive)page('overview');
 /* Radio profiles the board will not switch into are removed; the one that has
    a dashboard but no audio source keeps its button and says so on hover. */
 $$('[data-mode]').forEach(el=>{
  const pr=(caps.profiles||[]).find(x=>x.id===+el.dataset.mode);
  if(!pr)return;
  if(pr.offerable===false){el.style.display='none';return}
  if(pr.audio===false&&pr.note)el.title=pr.note;
 });
 /* The build expected hardware it did not find. Loud, because every capability
    underneath it is now answering a different question than the owner expects. */
 if(caps.degraded&&caps.degradedReason)toast(caps.degradedReason,true);
}
const MODES=[{name:'Wi-Fi only',badge:'Wi-Fi',hint:'The dashboard has the radio to itself. Bluetooth is not running, so nothing can pair.',warn:'The speaker restarts and rejoins your network; this dashboard comes back in a few seconds.'},{name:'Bluetooth only',badge:'Bluetooth',hint:'The A2DP sink has the radio to itself. Wi-Fi is never started in this mode.',warn:'The speaker restarts, Wi-Fi shuts down and this dashboard becomes unreachable. Hold BOOT on the speaker to cycle back.'},{name:'DFPlayer + Wi-Fi',badge:'SD + Wi-Fi',hint:'A DFPlayer Mini plays from its own microSD card or a USB drive and this dashboard drives it. Neither Bluetooth radio is started, so the whole controller is handed back — the largest heap saving any mode makes.',warn:'The speaker restarts with both Bluetooth radios off, so anything paired stops working until you switch back. Wi-Fi behaves exactly as it does now, including the setup hotspot when no network is saved, so this dashboard comes back in a few seconds.'}];
let auth=sessionStorage.getItem('speakerAuth')||'',status=null,settings=null,selectedFile=null,confirmAction=null,volumeTimer=0,pollTimer=0;
const esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const pendingGets=new Map();
async function api(url,opt={}){
 const isGet=!opt.method||opt.method==='GET';if(isGet&&pendingGets.has(url))return pendingGets.get(url);
 const request=(async()=>{const controller=new AbortController;const timeout=setTimeout(()=>controller.abort(),12000);
 try{opt.headers={...(opt.headers||{}),Authorization:auth};if(opt.body&&!(opt.body instanceof FormData)){opt.headers['Content-Type']='application/json';opt.body=JSON.stringify(opt.body)}
 const r=await fetch(url,{...opt,signal:controller.signal});let d={};try{d=await r.json()}catch{}
 if(r.status===401){sessionStorage.removeItem('speakerAuth');auth='';clearTimeout(pollTimer);$('#loginModal').classList.add('show');throw Error('Sign in required')}
 if(!r.ok)throw Error(d.error||`Request failed (${r.status})`);return d;
 }finally{clearTimeout(timeout)}})();
 if(isGet)pendingGets.set(url,request);try{return await request}finally{if(isGet)pendingGets.delete(url)}
}
function toast(msg,error=false){let t=$('#toast');t.textContent=msg;t.className='toast show'+(error?' error':'');clearTimeout(t._timer);t._timer=setTimeout(()=>t.className='toast',3200)}
function fmtTime(ms){let s=Math.max(0,Math.floor((ms||0)/1000)),m=Math.floor(s/60);return `${m}:${String(s%60).padStart(2,'0')}`}
function fmtUp(ms){let m=Math.floor(ms/60000);if(m<60)return `${m}m`;let h=Math.floor(m/60);if(h<48)return `${h}h ${m%60}m`;return `${Math.floor(h/24)}d ${h%24}h`}
function fmtBytes(n){if(n>1048576)return`${(n/1048576).toFixed(1)} MB`;return`${Math.round(n/1024)} KB`}
/* Internal heap and external RAM, together, because separately they lie.
   s.system.heapFree is MALLOC_CAP_INTERNAL and always has been -- on a board
   with 8 MB of PSRAM fitted, showing it alone under "Memory" is how the
   dashboard tells somebody they are low on memory they have plenty of. */
function fmtMem(sys){
 const internal=fmtBytes(sys.heapFree);
 const p=sys.psram;
 if(!p||!p.present)return internal;
 return `${internal} + ${fmtBytes(p.free)} PSRAM`;
}
function render(s){status=s;let w=s.wifi,b=s.bluetooth,m=s.media,u=s.update;$('#sideVersion').textContent=(caps&&caps.board&&caps.board.module)?`${caps.board.module} · v${s.firmware.version}`:`Firmware ${s.firmware.version}`;$('#firmwareVersion').textContent=`v${s.firmware.version}`;$('#uptime').textContent=fmtUp(s.system.uptimeMs);$('#heap').textContent=fmtMem(s.system);$('#rssi').textContent=w.connected?`${w.rssi} dBm`:'—';$('#apClients').textContent=w.apClients;$('#wifiDot').className='dot '+(w.connected?'good':'');$('#sideDot').className='dot '+(b.connected?'good':'');$('#sideStatus').textContent=s.system.powerSaving?'Power saving':(s.mode&&s.mode.dfplayer)?(!s.dfplayer||!s.dfplayer.running?'DFPlayer starting':(s.dfplayer.asleep?'DFPlayer in standby':(!s.dfplayer.online?'DFPlayer not answering':(s.dfplayer.busy?'Playing from '+s.dfplayer.sourceName:(s.dfplayer.pc?'Card on a computer':'Ready to play'))))):(b.connected?(b.streaming?'Streaming audio':'Bluetooth connected'):(b.active?'Ready to pair':'Wi-Fi only, Bluetooth off'));$('#topWifi').textContent=w.connected?w.ssid:(w.apRunning?w.apSsid:'Offline');$('#wifiName').textContent=w.connected?w.ssid:(w.apSsid||'Offline');$('#wifiBadge').textContent=w.connected?'Connected':(w.apRunning?'Setup AP':'Offline');$('#wifiBadge').className='badge '+((w.connected||w.apRunning)?'good':'');$('#wifiDetail').textContent=w.connected?`${w.ip} · ${w.rssi} dBm`:(w.apRunning?`${w.apIp} · ${w.apClients} client${w.apClients===1?'':'s'}`:'No network');$('#signalMeter').style.width=w.connected?`${Math.max(5,Math.min(100,2*(w.rssi+100)))}%`:'0';$('#btDevice').textContent=b.connected?(b.device||'Connected device'):(b.active?'No device':'Bluetooth off');$('#btDetail').textContent=b.connected?`${b.address} · ${b.sampleRate/1000} kHz`:(b.active?'Discoverable and ready to pair':'Not running in Wi-Fi only mode');$('#btBadge').textContent=b.streaming?'Streaming':(b.connected?'Connected':(b.active?'Waiting':'Off'));$('#btBadge').className='badge '+(b.connected?'good':'');renderMode(s.mode||{},b,s.dfplayer||{});renderBattery(s.battery||{});if($('#page-media').classList.contains('active')){renderDfPage(s.dfplayer||{});dfNowLine()}$('#btHeldOff').style.display=b.active?'none':'';$('#title').textContent=m.title||'Nothing playing';let df=s.dfplayer||{},viaDf=!!(s.mode&&s.mode.dfplayer);$('#artist').textContent=m.artist||(m.title?'':(viaDf?(df.running?(df.online?'Pick a track on the Media page':'The module is not answering — check TX and RX'):'The DFPlayer driver did not start'):(b.connected?'Waiting for track information':'Connect a Bluetooth device to begin')));$('#streamLabel').textContent=viaDf?(df.busy?'Playing from the card':(DFLABEL[df.state]||'Ready to play')):(b.streaming?'Live audio':(b.connected?'Connected':'Ready to play'));let hz=viaDf?(df.running&&df.online?44100:0):(b.sampleRate||0);$('#codec').textContent=hz?`${(hz/1000).toFixed(1)} kHz`:'—';$('#position').textContent=fmtTime(m.positionMs);$('#duration').textContent=fmtTime(m.durationMs);$('#trackBar').style.width=m.durationMs?`${Math.min(100,m.positionMs/m.durationMs*100)}%`:'0';if(document.activeElement!==$('#volume')){$('#volume').value=m.volume;$('#volumeText').textContent=`${Math.round(m.volume/127*100)}%`}let pb=$('#playButton');pb.dataset.icon=m.state==='playing'?'pause':'play';pb.querySelector('svg').outerHTML=`<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8">${paths[pb.dataset.icon]}</svg>`;if(settings&&settings.power&&$('#page-settings').classList.contains('active')){const on=!!s.system.powerSaving;if(on!==settings.power.saving)loadSettings()}$('#updateBadge').textContent=u.available?'Available':(u.busy?'Working':'Current');$('#updateBadge').className='badge '+(u.available?'':(!u.busy?'good':''));$('#firmwareDetail').textContent=u.busy?u.message:(u.available?`${u.tag||u.releaseName} is newer and ready to install`:(u.tag?`Latest on GitHub is ${u.tag} · nothing newer to install`:'A/B update protection enabled'));$('#overviewInstallRow').style.display=(u.available&&!u.busy)?'':'none';renderUpdate(u);renderCurrentDevice(b);clockFromStatus(s.system)}
function renderMode(md,b,d){let id=md.id||0,cur=MODES[id]||MODES[0];$('#modeName').textContent=cur.name;$('#modeDetail').textContent=cur.hint;$('#modeBadge').textContent=cur.badge;$('#modeBadge').className='badge '+(id===2?'good':'');let dfBuilt=md.dfBuilt!==false;$$('[data-mode]').forEach(x=>{let m=+x.dataset.mode;x.classList.toggle('on',m===id);x.disabled=m===id||(m===2&&!dfBuilt)});$('#modeNote').textContent=dfBuilt?'':'This firmware was built with the DFPlayer driver switched off.';let dfm=!!md.dfplayer;$('#btCard').style.display=dfm?'none':'';$('#dfCard').style.display=dfm?'':'none';if(dfm)renderDf(d);let live,ctl,skip,seek;if(dfm){live=!!(d.running&&d.online&&!d.asleep);ctl=live;skip=live;seek=false}else{live=!!b.active;ctl=live&&!!b.avrcp;skip=ctl;seek=ctl}$$('[data-media]').forEach(x=>{let a=x.dataset.media;x.disabled=(a==='volume'||a==='mute')?!live:(SEEKS.includes(a)?!seek:(SKIPS.includes(a)?!skip:!ctl))});$('#volume').disabled=!live;$('#controlHint').textContent=dfm?(!d.running?'The DFPlayer driver did not start this boot; check the serial log.':(d.asleep?'The module is in standby. Wake it on the Media page.':(!d.online?'The module is not answering. The usual cause is TX and RX swapped; the hardware pin buttons on the Media page work regardless.':(d.pc?'A computer has the card mounted. Unplug the USB cable to play from it again.':(!d.totalTracks?'No files found on this source. Check the card, or pick another source on the Media page.':'Seeking needs a position, and the module reports none — use next and previous.'))))):(!live?'Playback control needs the Bluetooth stack running. Choose Bluetooth only under Radio mode.':(!b.connected?'Pair a phone to control playback from here.':(!ctl?'This device offers no AVRCP channel, so only volume can be set from here.':'')))}const SKIPS=['next','previous'],SEEKS=['forward','rewind'];const DFLABEL={stopped:'Stopped',playing:'Playing',paused:'Paused',standby:'Standby'};
function renderDf(d){if(d.stale)return;let bad=!d.running||(!d.online&&!d.asleep);$('#dfBadge').textContent=!d.running?'Off':(d.asleep?'Standby':(!d.online?'No reply':(DFLABEL[d.state]||d.state||'Idle')));$('#dfBadge').className='badge '+(bad?'bad':(d.asleep?'warn':(d.busy?'good':'')));$('#dfNow').textContent=!d.running?'Driver not started':(d.asleep?'Module in standby':(!d.online?'Module not answering':(d.pc?'Card on a computer':(d.track?(d.folder?`Folder ${d.folder} · track ${d.track}`:`Track ${d.track}`):'Nothing playing'))));$('#dfDetail').textContent=bad?'':`${d.sourceName||''}`+(d.totalTracks?` · ${d.totalTracks} file${d.totalTracks===1?'':'s'}`:' · no files found')+(d.eqName?` · ${d.eqName} EQ`:'')+(d.loop&&d.loopName!=='off'?` · repeat ${d.loopName}`:'');$$('[data-dfsource]').forEach(x=>{let k=x.dataset.dfsource;x.classList.toggle('on',SRCID[k]===d.source);x.disabled=bad||SRCID[k]===d.source||(k==='sd'&&!d.sd)||(k==='usb'&&!d.usb)||(k==='flash'&&!d.flash)});$('#dfHint').textContent=!d.running?'Check the serial log: the driver failed to claim UART2 or ran out of memory.':(d.asleep?'Its decoder is powered down — worth about 20 mA. Wake it on the Media page to play again.':(!d.online?'No frame has arrived from the module. Check TX/RX (they cross over), the 1k resistor on its RX pin, and that it has 5 V.':(d.error||'')))}const SRCID={usb:1,sd:2,aux:3,flash:5};function renderDfPage(d){if(d.stale)return;let up=!!(d.running&&d.online&&!d.asleep);$('#dfOffline').style.display=d.running?'none':'';$('#dfPanels').style.display=d.running?'':'none';if(!d.running)return;$$('[data-dfsrc]').forEach(x=>{let k=x.dataset.dfsrc;x.classList.toggle('on',SRCID[k]===d.source);x.disabled=!up||SRCID[k]===d.source});let media=(ok,label)=>ok?'Present':label;$('#dfSdState').textContent=media(d.sd,'Not detected');$('#dfUsbState').textContent=media(d.usb,'Not detected');$('#dfFlashState').textContent=media(d.flash,'Not on this module');$('#dfFiles').textContent=d.totalTracks?String(d.totalTracks):'Unknown';$('#dfFolders').textContent=d.folders?String(d.folders):'Unknown';$('#dfPc').style.display=d.pc?'':'none';if(d.queriedFolder)$('#dfFolderCount').textContent=d.folderTracks?`Folder ${d.queriedFolder} holds ${d.folderTracks} track${d.folderTracks===1?'':'s'}.`:`Folder ${d.queriedFolder} is empty, or the module has no such folder.`;if(document.activeElement!==$('#dfVolume')){$('#dfVolume').value=d.volume;$('#dfVolume').max=d.volumeMax||30}$('#dfVolText').textContent=`${d.volume} / ${d.volumeMax||30}`;$$('[data-dfeq]').forEach(x=>x.classList.toggle('on',+x.dataset.dfeq===d.eq));$$('[data-dfloop]').forEach(x=>x.classList.toggle('on',x.dataset.dfloop===d.loopName));if(document.activeElement!==$('#dfDac'))$('#dfDac').checked=d.dac!==false;let pins=d.pins||{};$$('[data-dfpin]').forEach(x=>{let ok=pins[x.dataset.dfpin]!==false;x.disabled=!ok;x.title=ok?'':'Not wired on this board — see hw_config.h'});$('#dfBusyPin').textContent=pins.busy===false?'Not wired':(d.busy?'Low — playing':'High — idle');$$('[data-dfled]').forEach(x=>{x.classList.toggle('on',x.dataset.dfled===LEDKEY[d.led]);x.disabled=pins.led===false});$('#dfLedDot').className=d.ledOn?'on':'';$('#dfLedState').textContent=pins.led===false?'No DFPlayer LED wired on this board':`Currently ${d.ledOn?'lit':'dark'}, mode ${LEDKEY[d.led]||'auto'}`;$('#dfLink').textContent=d.asleep?'Standby (nothing is asked of it)':(d.online?'Answering':'No reply');$('#dfVersion').textContent=d.version?String(d.version):'Not reported';$('#dfTrackNow').textContent=d.track?(d.folder?`${d.folder} / ${d.track}`:String(d.track)):'—';$('#dfFinished').textContent=String(d.finished||0);$('#dfError').textContent=d.error||'None';$$('#dfPanels button').forEach(x=>{if(x.dataset.dfpin!==undefined||x.dataset.dfsrc!==undefined||x.dataset.dfled!==undefined||x.id==='dfReset'||x.id==='dfWake')return;x.disabled=!up});$('#dfVolume').disabled=!up;$('#dfDac').disabled=!up}const LEDKEY={0:'auto',1:'off',2:'on',3:'blink'};
function renderBattery(b){let card=$('#batteryCard');card.style.display=b.wired===false?'none':'';if(!b.enabled){$('#batBadge').textContent='Gauge off';$('#batBadge').className='badge';$('#batPercent').textContent='Off';$('#batDetail').textContent='The sense pin is configured but the gauge is not switched on.';$('#batMeter').style.width='0';$('#batMeter').className='flat';$('#batHint').textContent='Turn it on under Settings → Battery. It starts off so a board with no divider fitted cannot invent a flat battery and sit there flashing the status LED about it.';return}if(!b.present){$('#batBadge').textContent='Not detected';$('#batBadge').className='badge bad';$('#batPercent').textContent='—';$('#batDetail').textContent=`Sense pin reads ${b.pinMillivolts||0} mV, too low for a cell`;$('#batMeter').style.width='0';$('#batMeter').className='flat';$('#batHint').textContent='Check the divider and that the pack is connected. The ratio and the trim are on the Settings page.';return}let st=b.state||'unknown',crit=st==='critical',low=st==='low';$('#batBadge').textContent={charging:'Charging',full:'Full',low:'Low',critical:'Critical',discharging:'On battery'}[st]||'Unknown';$('#batBadge').className='badge '+(crit?'bad':(low?'warn':(st==='charging'||st==='full'?'good':'')));$('#batPercent').textContent=`${b.percent}%`;$('#batDetail').textContent=`${(+b.volts).toFixed(2)} V`+(b.cells>1?` · ${(+b.cellVolts).toFixed(2)} V per cell · ${b.cells} cells`:'')+(b.chargeDone?' · charge complete':(b.charging?' · charging':''));$('#batMeter').style.width=`${Math.max(2,b.percent)}%`;$('#batMeter').className=crit?'bad':(low?'warn':'');$('#batHint').textContent=crit?`Below ${b.criticalPercent}% — the status LED is flashing about it. Nothing is switched off automatically; that is deliberate, so the speaker never cuts out mid-track on a reading that sagged under load.`:(low?`Below ${b.lowPercent}% — time to charge.`:(b.chargePins?'':'No charger status pins are wired, so charging cannot be detected — the voltage and the percentage are still right.'))}
function renderUpdate(u){$('#releaseName').textContent=u.releaseName||u.tag||'Not checked yet';$('#releaseAsset').textContent=u.asset||'Configure a GitHub repository in Settings.';$('#updateMessage').textContent=u.message||u.phase;let pct=u.total?Math.min(100,u.done/u.total*100):0;$('#updateProgress').style.width=`${pct}%`;$('#installUpdate').disabled=u.busy||!u.asset||!u.available;$('#checkUpdate').disabled=u.busy;$('#uploadFirmware').disabled=u.busy||!selectedFile;let link=$('#releaseLink');link.href=u.releaseUrl||'#';link.style.display=u.releaseUrl?'inline-flex':'none'}
function renderCurrentDevice(b){let box=$('#currentDevice');if(!b.connected){box.className='empty';box.textContent='No phone connected';return}box.className='device';box.innerHTML=`<div class="avatar"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8">${paths.bluetooth}</svg></div><div class="deviceinfo"><b>${esc(b.device||'Connected device')}</b><small>${esc(b.address)} · ${b.streaming?'Streaming audio':'Idle'}</small></div><button class="btn danger" data-disconnect>Disconnect</button>`;box.querySelector('[data-disconnect]').onclick=()=>confirmDo('Disconnect device?','Playback will stop, but the phone will stay paired.',()=>deviceAction('disconnect'))}
/*
 * The speaker's clock, drawn from the epoch the status poll carries rather than
 * from this browser's own time: the two are only the same if somebody has
 * pressed Sync, and the whole point of showing it is to make that visible.
 * Polls land every two seconds, so the seconds are ticked locally from the
 * moment the epoch arrived -- the reading never drifts from the speaker, and it
 * never sits still between polls either.
 */
let devClock=null;const CLK_MON=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'],CLK_DAY=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'],CLK_SRC={ntp:'Network time (NTP)',ds3231:'DS3231 module',nvs:'Restored after power cut',set:'Set by hand',build:'Build timestamp (unconfirmed)'};
function clockFromStatus(sys){if(!sys||!sys.epoch)return;devClock={epoch:sys.epoch,offset:sys.tzOffsetMinutes||0,h24:sys.clock24h!==false,trusted:sys.clockTrusted!==false,source:sys.clockSource||''};devClock.at=Date.now();drawClock()}
// The offset is already in the epoch we add it to, so every reader below is a
// UTC one: getHours() would apply the *browser's* zone a second time.
function clockText(d,h24,secs){let h=d.getUTCHours(),m=String(d.getUTCMinutes()).padStart(2,'0'),s=String(d.getUTCSeconds()).padStart(2,'0'),ap='';if(h24)h=String(h).padStart(2,'0');else{ap=h<12?' AM':' PM';h=h%12||12}return `${h}:${m}${secs?':'+s:''}${ap}`}
function drawClock(){if(!devClock)return;let d=new Date(devClock.epoch*1000+(Date.now()-devClock.at)+devClock.offset*60000);$('#topDate').textContent=`${CLK_DAY[d.getUTCDay()]} ${d.getUTCDate()} ${CLK_MON[d.getUTCMonth()]}`;$('#topClock').textContent=clockText(d,devClock.h24,true);$('#clockDot').className='dot '+(devClock.trusted?'good':'');$('#clockPill').title=`Speaker clock · ${CLK_SRC[devClock.source]||devClock.source||'unknown source'}`;$('#clockNow').textContent=`${d.getUTCFullYear()}-${String(d.getUTCMonth()+1).padStart(2,'0')}-${String(d.getUTCDate()).padStart(2,'0')} ${clockText(d,devClock.h24,true)}`}
setInterval(drawClock,1000);

/*
 * The timeout menus for the panel and the ring. One list, because the two
 * settings mean the same thing and offering different numbers for each would
 * only invite the question of why.
 */
const TIMEOUTS=[[10,'10 seconds'],[30,'30 seconds'],[60,'1 minute'],[120,'2 minutes'],[300,'5 minutes'],[600,'10 minutes'],[900,'15 minutes'],[1800,'30 minutes'],[3600,'1 hour'],[7200,'2 hours'],[14400,'4 hours'],[43200,'12 hours']];
function fillTimeouts(sel,lo,hi){if(sel.options.length)return;sel.innerHTML=TIMEOUTS.filter(([v])=>v>=(lo||0)&&v<=(hi||86400)).map(([v,t])=>`<option value="${v}">${t}</option>`).join('')}
// A stored value that is not one of the offered steps -- set over the API, or
// left behind by a firmware with a different list -- gets an option of its own
// rather than being silently rounded to whichever one happens to be selected.
function setTimeout_(sel,secs){if(!sel.querySelector(`option[value="${secs}"]`)){const o=document.createElement('option');o.value=secs;o.textContent=fmtSecs(secs);sel.insertBefore(o,sel.firstChild)}sel.value=String(secs)}
function fmtIdle(s){s=Math.max(0,Math.round(s));if(s<60)return `${s}s`;const m=Math.floor(s/60);return m<60?`${m}m ${s%60}s`:`${Math.floor(m/60)}h ${m%60}m`}
function fmtSecs(s){if(s<60)return `${s} seconds`;if(s<3600)return `${Math.round(s/60)} minutes`;return `${(s/3600).toFixed(s%3600?1:0)} hours`}

let refreshBusy=false,pollDelay=2000;
function scheduleRefresh(delay=pollDelay){clearTimeout(pollTimer);if(auth&&!document.hidden)pollTimer=setTimeout(async()=>{await refresh();scheduleRefresh()},delay)}
document.addEventListener('visibilitychange',()=>{if(document.hidden)clearTimeout(pollTimer);else scheduleRefresh(100)});
async function refresh(show=false){
 if(refreshBusy)return;refreshBusy=true;
 try{let s=await api('/api/status');render(s);ledLiveStatus(s.leds);renderRadioOverview(s);renderAlarmOverview(s);refreshActivePage(s);productRender(s);pollDelay=2000;$('#connectionBanner').classList.remove('show');if(show)toast('Dashboard refreshed')}
 catch(e){pollDelay=Math.min(30000,pollDelay*2);$('#connectionBanner').classList.add('show');if(show)toast(e.message,true)}
 finally{refreshBusy=false}
}
async function loadSettings(){try{settings=await api('/api/settings');paintZoneStatus();$('#deviceName').value=settings.deviceName;$('#hostname').value=settings.hostname;$('#apAlways').checked=settings.apAlways;$('#githubRepo').value=settings.githubRepo;$('#githubAsset').value=settings.githubAsset;$('#wifiSsid').value=settings.savedSsid;$('#clockAutoSync').checked=settings.clockAutoSync!==false;let pw=settings.power||{};renderPowerMode(pw.mode|0);if(document.activeElement!==$('#powerThreshold')){$('#powerThreshold').value=pw.threshold??20;$('#powerThresholdText').textContent=`${pw.threshold??20}%`}$('#powerState').textContent=pw.saving?'Saving':'Not saving';$('#powerState').style.color=pw.saving?'var(--mint)':'';$('#powerReason').textContent=pw.reason||'';$('#powerThresholdRow').classList.toggle('off',(pw.mode|0)!==2);fillTimeouts($('#sleepAfter'),pw.sleepMinSeconds,pw.sleepMaxSeconds);setTimeout_($('#sleepAfter'),pw.sleepAfterSeconds??1800);renderSleepMode(pw.sleepMode|0);fillTimeouts($('#indicatorAfter'),pw.indicatorMinSeconds,pw.indicatorMaxSeconds);setTimeout_($('#indicatorAfter'),pw.indicatorAfterSeconds??300);renderIndicatorMode(pw.indicator??1);$('#indicatorState').textContent=indicatorText(pw);$('#standbyNow').disabled=pw.sleepPossible===false;$('#sleepState').textContent=pw.sleepPossible===false?'No wake button is compiled in, so there would be nothing to bring it back.':((pw.sleepMode|0)===0?'Nothing has played and nobody has touched it for '+fmtIdle(pw.idleSeconds||0)+'.':`Idle for ${fmtIdle(pw.idleSeconds||0)} of ${fmtSecs(pw.sleepAfterSeconds??1800)}.`);let od=settings.display||{};fillTimeouts($('#oledTimeout'),od.blankMinSeconds,od.blankMaxSeconds);setTimeout_($('#oledTimeout'),od.blankAfterSeconds??300);renderOledBlank(od.blankMode|0);$('#oledBlankState').innerHTML=od.present===false?'No panel was found on the I2C bus, so these have nothing to act on.':(od.blanked?'The panel is off right now. Press BOOT, or change anything here, to bring it back.':`The panel is on. Nothing has played for <b>${fmtIdle(od.idleSeconds||0)}</b>; nobody has touched it for <b>${fmtIdle(od.untouchedSeconds||0)}</b>.<br>Analyser peak <b>${od.audioPeakDb??'—'} dBFS</b>${od.dfBusy?' · the DFPlayer says it is playing':''} — silence reads about −78, so anything near that with the first timer stuck is a fault worth reporting.`);let h24=settings.clock24h!==false;$$('[data-clockfmt]').forEach(x=>x.classList.toggle('on',(x.dataset.clockfmt==='24')===h24));let om=settings.clockOffsetMinutes||0,oa=Math.abs(om);$('#clockZone').textContent=`UTC${om<0?'-':'+'}${String(Math.floor(oa/60)).padStart(2,'0')}:${String(oa%60).padStart(2,'0')}`;$('#clockSource').textContent=(CLK_SRC[settings.clockSource]||settings.clockSource||'—')+(settings.clockNetworkSynced?' · synced':'');let d=settings.dfplayer||{};$('#dfDefSource').value=String(d.source||2);$('#dfDefVolume').max=d.volumeMax||30;$('#dfDefVolume').value=d.volume??20;$('#dfDefVolText').textContent=`${d.volume??20} / ${d.volumeMax||30}`;$('#dfDefEq').value=String(d.eq||0);$('#dfDefLoop').value=String(d.loop||0);$('#dfDefLoopFolder').value=d.loopFolder||1;$('#dfDefAutoplay').checked=!!d.autoplay;let b=settings.battery||{};$('#batEnabled').checked=b.enabled!==false;$('#batCells').value=b.cells||1;$('#batDivider').value=b.divider??2;$('#batFull').value=b.full??4.2;$('#batEmpty').value=b.empty??3.3;$('#batPack').textContent=(b.cells||1)>1?`That is ${((b.full??4.2)*(b.cells||1)).toFixed(2)} V to ${((b.empty??3.3)*(b.cells||1)).toFixed(2)} V across the whole pack.`:'';$('#batLow').value=b.low??20;$('#batCritical').value=b.critical??7;$('#batTrim').textContent=`Trim in force: ${(+(b.calibration??1)).toFixed(4)}. Put a meter across the pack, type what it says, and the correction is computed and stored.`;$('#batChargePins').textContent=b.chargePins?'Wired':'Not wired';if(b.sensePin!==undefined&&b.sensePin<0)$('#batSensePin').textContent='This firmware was built with no battery sense pin (PIN_BATTERY_SENSE is -1), so these settings have no effect until one is configured in hw_config.h.';if(status&&status.battery)$('#batPin').textContent=`${status.battery.pinMillivolts||0} mV`;if(settings.defaultAdminPassword)toast('Change the default dashboard password',true)}catch(e){toast(e.message,true)}}
/*
 * The DFPlayer library browser.
 *
 * Everything here is numbers, because that is all the module has: folders 01-99
 * and a file count for each, learned one round trip at a time by a scan. The
 * index is fetched on opening the Media page and then only while a scan is
 * running -- it changes at no other time, and the two-second status poll has no
 * business carrying ninety-nine numbers it does not need.
 */
let dfLib=null,dfOpenFolder=0,dfLibTimer=null,dfStarted=null;
async function loadDfLibrary(){try{dfLib=await api('/api/dfplayer/library');renderDfLibrary()}catch(e){dfLib=null}}
function dfLibPoll(on){clearTimeout(dfLibTimer);dfLibTimer=on?setTimeout(async()=>{if(document.hidden)return;await loadDfLibrary();dfLibPoll($('#page-media').classList.contains('active')&&!!dfLib?.scanning)},1500):null}
function renderDfLibrary(){const l=dfLib;if(!l)return;
 const pct=l.scanTotal?Math.round(l.scanDone/l.scanTotal*100):0;
 $('#dfScanBarWrap').style.display=l.scanning?'':'none';$('#dfScanBar').style.width=`${pct}%`;$('#dfScan').disabled=!!l.scanning;
 $('#dfScanState').textContent=l.scanning?`Asking folder ${l.scanDone} of ${l.scanTotal}…`:(l.knownFolders?`${l.knownFolders} folder${l.knownFolders===1?'':'s'} with files on the ${l.source}${l.totalTracks?` · ${l.totalTracks} files in total`:''}`:(l.scanned?'The scan finished and found no numbered folders. Files in the root are reachable by their flat index below.':'Not scanned yet.'));
 // Polling stops itself the moment the scan does, rather than being left to a
 // timer somewhere else to notice.
 dfLibPoll(!!l.scanning);
 const fs=l.folders||[];
 $('#dfFolderEmpty').style.display=fs.length?'none':'';
 $('#dfFolderGrid').innerHTML=fs.map(f=>`<button data-dffolder="${f.folder}" class="${f.folder===dfOpenFolder?'on':''}"><b>${String(f.folder).padStart(2,'0')}</b><small>${f.files} file${f.files===1?'':'s'}</small></button>`).join('');
 $$('[data-dffolder]').forEach(b=>b.onclick=()=>dfOpen(+b.dataset.dffolder));
 dfRenderTracks();dfNowLine()}
function dfOpen(folder){dfOpenFolder=dfOpenFolder===folder?0:folder;$$('[data-dffolder]').forEach(b=>b.classList.toggle('on',+b.dataset.dffolder===dfOpenFolder));dfRenderTracks()}
function dfRenderTracks(){const pane=$('#dfTrackPane');const f=(dfLib&&(dfLib.folders||[]).find(x=>x.folder===dfOpenFolder));
 if(!f){pane.style.display='none';return}
 pane.style.display='';$('#dfTrackLabel').textContent=`Folder ${String(f.folder).padStart(2,'0')} — ${f.files} file${f.files===1?'':'s'}`;
 let html='';for(let i=1;i<=f.files;i++){const on=dfStarted&&dfStarted.folder===f.folder&&dfStarted.file===i;html+=`<button data-dftrack="${i}" class="${on?'on':''}"><b>${String(i).padStart(3,'0')}</b></button>`}
 $('#dfTracks').innerHTML=html;
 $$('[data-dftrack]').forEach(b=>b.onclick=()=>dfPlayFrom(dfOpenFolder,+b.dataset.dftrack))}
async function dfPlayFrom(folder,file){dfStarted={folder,file};dfRenderTracks();dfNowLine();await df('folder',{folder,file})}
/*
 * What is playing, in the module's own terms.
 *
 * `track` is a flat index over the whole source and is the only position the
 * module reports -- it does not say which folder a file came from. So the
 * folder and track shown are the ones the dashboard itself started, and are
 * dropped as soon as the module stops, rather than being left to describe
 * something that finished ten tracks ago.
 */
function dfNowLine(){const d=(status&&status.dfplayer)||{},l=dfLib||{};
 if(!d.busy)dfStarted=null;
 $('#dfNowLine').textContent=!d.running?'The driver did not start':(!d.online?'The module is not answering':(d.busy?(dfStarted?`Folder ${String(dfStarted.folder).padStart(2,'0')} · track ${String(dfStarted.file).padStart(3,'0')}`:`Track ${d.track||l.track||'?'}`):'Nothing playing'));
 $('#dfNowSub').textContent=d.busy?`Playing from the ${d.sourceName||l.source||'card'}${d.track?` · file ${d.track} of ${d.totalTracks||'?'} on this source`:''}`:'Pick a folder above, then a track.';const t=$('#dfToggle'),want=d.busy?'pause':'play';if(t.dataset.icon!==want){t.dataset.icon=want;t.querySelector('svg').outerHTML=`<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8">${paths[want]}</svg>`}}
async function loadDevices(){try{let d=await api('/api/devices'),box=$('#pairedDevices');if(!d.devices.length){box.innerHTML='<div class="empty">No paired devices yet</div>';return}box.innerHTML=d.devices.map(x=>`<div class="device"><div class="avatar"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8">${paths.bluetooth}</svg></div><div class="deviceinfo"><b>${esc(x.name)}</b><small>${esc(x.address)}${x.connected?' · Connected':''}</small></div><button class="btn danger" data-forget="${esc(x.address)}">Forget</button></div>`).join('');$$('[data-forget]').forEach(b=>b.onclick=()=>confirmDo('Forget this device?','Its pairing keys will be removed from the speaker.',()=>deviceAction('forget',b.dataset.forget)))}catch(e){toast(e.message,true)}}
async function deviceAction(action,address){try{await api('/api/devices',{method:'POST',body:{action,address}});toast(action==='forget'?'Device forgotten':'Device disconnected');setTimeout(()=>{refresh();loadDevices()},700)}catch(e){toast(e.message,true)}}
let ledCfg=null,ledPatch={},ledTimer=null;const LED_SWATCHES=['#FF3B30','#FF9500','#FFD60A','#34C759','#00E0FF','#0A84FF','#7A5CFF','#FF2D96','#FFFFFF'];function ledLabels(l){$('#ledBrightValue').textContent=Math.round(l.brightness/255*100)+'%';$('#ledSpeedValue').textContent=Math.round(l.speed/255*100)+'%';$('#ledReactValue').textContent=l.reactivity+'%'}function ledAudioLine(l){let e=$('#ledAudio');if(!e)return;if(l.audioPath===false){e.innerHTML='This speaker is in <b>DFPlayer mode</b>. That module decodes its own card and hands out analog audio that never passes through the ESP32, so there is nothing for the analyser to hear and the reactive effects rest at their idle brightness. Every other mode feeds it.';return}e.innerHTML=l.hearingAudio?'<b>Hearing audio.</b> The reactive effects are live.':'<b>Silent.</b> Play something and the reactive effects pick it up within a frame.'}function renderLighting(l){ledCfg=l;$('#ledUnwired').style.display=l.wired?'none':'';$('#ledBody').style.display=l.wired?'':'none';if(!l.wired)return;$('#ledEnabled').checked=!!l.enabled;$('#ledColor').value=l.color;$('#ledColor2').value=l.color2;$('#ledBrightness').value=l.brightness;$('#ledSpeed').value=l.speed;$('#ledReactivity').value=l.reactivity;ledLabels(l);$('#ledPins').textContent=`${l.count} pixel${l.count===1?'':'s'} on GPIO${l.pin}`;$('#ledEffects').innerHTML=(l.effects||[]).map((e,i)=>`<button data-fx="${i}" class="${i===l.effect?'on':''}">${esc(e.name)}</button>`).join('');$$('[data-fx]').forEach(b=>b.onclick=()=>ledPick(+b.dataset.fx));$('#ledHint').textContent=((l.effects||[])[l.effect]||{}).hint||'';fillTimeouts($('#ledIdleAfter'),l.idleMinSeconds,l.idleMaxSeconds);setTimeout_($('#ledIdleAfter'),l.idleAfterSeconds??300);$('#ledIdleOff').checked=!!l.idleOff;$('#ledIdleRow').classList.toggle('off',!l.idleOff);ledAudioLine(l);ledRestLine(l)}function ledRestLine(l){if(l.powerSaving){$('#ledIdleState').textContent='Ring held dark by power saving.';return}if(l.present===false){$('#ledIdleState').textContent='Ring driver did not start. Download a support report from Device health.';return}if(l.outputErrors){$('#ledIdleState').textContent=`Ring output reported ${l.outputErrors} transfer errors. Check Device health.`;return}$('#ledIdleState').textContent=!l.idleOff?'The ring stays lit for as long as the speaker is powered.':(l.resting?'The ring is resting now. The first note, or any change here, brings it back.':`The ring is lit. Nothing has played and nothing has changed for ${fmtIdle(l.idleSeconds||0)}.`)}function ledPick(i){if(!ledCfg)return;ledCfg.effect=i;$$('[data-fx]').forEach(x=>x.classList.toggle('on',+x.dataset.fx===i));$('#ledHint').textContent=((ledCfg.effects||[])[i]||{}).hint||'';sendLeds({effect:i})}function sendLeds(patch){Object.assign(ledPatch,patch);if(ledTimer)return;ledTimer=setTimeout(async()=>{let body=ledPatch;ledPatch={};ledTimer=null;try{let r=await api('/api/leds',{method:'POST',body});if(ledCfg){Object.assign(ledCfg,body);ledCfg.hearingAudio=r.hearingAudio;ledCfg.resting=r.resting;ledAudioLine(ledCfg);ledRestLine(ledCfg)}}catch(e){toast(e.message,true)}},120)}function ledLiveStatus(l){if(!l||!ledCfg)return;ledCfg.hearingAudio=l.hearingAudio;ledCfg.resting=l.resting;ledCfg.powerSaving=l.powerSaving;ledCfg.present=l.present;ledCfg.outputErrors=l.outputErrors;ledAudioLine(ledCfg);ledRestLine(ledCfg)}async function loadLighting(){try{settings=await api('/api/settings');renderLighting(settings.leds||{wired:false})}catch(e){toast(e.message,true)}}$('#ledSwatchRow').innerHTML=LED_SWATCHES.map(c=>`<button data-swatch="${c}" style="background:${c}" title="${c}"></button>`).join('');$$('[data-swatch]').forEach(b=>b.onclick=()=>{$('#ledColor').value=b.dataset.swatch;sendLeds({color:b.dataset.swatch})});$('#ledEnabled').onchange=e=>sendLeds({enabled:e.target.checked});$('#ledColor').oninput=e=>sendLeds({color:e.target.value});$('#ledColor2').oninput=e=>sendLeds({color2:e.target.value});$('#ledBrightness').oninput=e=>{if(ledCfg){ledCfg.brightness=+e.target.value;ledLabels(ledCfg)}sendLeds({brightness:+e.target.value})};$('#ledSpeed').oninput=e=>{if(ledCfg){ledCfg.speed=+e.target.value;ledLabels(ledCfg)}sendLeds({speed:+e.target.value})};$('#ledReactivity').oninput=e=>{if(ledCfg){ledCfg.reactivity=+e.target.value;ledLabels(ledCfg)}sendLeds({reactivity:+e.target.value})};

/* =====================================================================
   Sound, Radio, Alarms, Graphs and Home Assistant.

   Each page keeps its own last-fetched document in a module-level variable and
   re-renders from it, rather than reading values back out of the DOM. That is
   the same shape the Lighting and Media pages already use here, and it is what
   makes a page safe to repaint underneath somebody who is halfway through
   editing: the editor writes to the document, the document paints the page.
   ===================================================================== */

let audioCfg=null,radioCfg=null,alarmCfg=null,mqttCfg=null,graphData=null;
let radioEditIndex=-1,alarmEditIndex=-1,alarmDraft=null;
let radioVolBusy=0,soundVolBusy=0;

/* --- the equaliser ---------------------------------------------------- */

const EQ_LABELS=hz=>hz>=1000?`${hz/1000}k`:`${hz}`;

function eqDraft(){
  /* The sliders are the truth while the page is open; the saved document is
     only the starting point. Reading them back here keeps "move three sliders
     then press Save" from posting the first one three times. */
  const gain=$$('#eqBank input[type=range]').map(i=>+i.value);
  return {enabled:$('#eqEnabled').checked,preamp:+$('#eqPreamp').value,
          autoPreamp:$('#eqAuto').checked,gain};
}

function eqPaintCurve(){
  /* A drawn approximation of the response, not a computed one.

     Each band is a bell (or a shelf at the ends) whose height is its gain, and
     the curve is their sum. That is not the transfer function -- the real one
     depends on Q and on how the sections interact -- but it is the shape the
     sliders describe, and the point of the picture is to make five numbers
     legible at a glance rather than to be a measurement. */
  const c=$('#eqCurve');if(!c)return;const ctx=c.getContext('2d');
  const w=c.width,h=c.height,mid=h/2;
  ctx.clearRect(0,0,w,h);
  ctx.strokeStyle='#223040';ctx.lineWidth=1;
  for(let db=-12;db<=12;db+=6){const y=mid-(db/14)*mid;ctx.globalAlpha=db===0?.9:.35;
    ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(w,y);ctx.stroke()}
  ctx.globalAlpha=1;
  const gains=$$('#eqBank input[type=range]').map(i=>+i.value);
  const bands=(audioCfg&&audioCfg.eq.bands)||[60,250,1000,4000,12000];
  if(!gains.length)return;
  /* Log frequency axis from 20 Hz to 20 kHz, which is the only axis on which a
     tone control looks like the thing it does. */
  const lo=Math.log10(20),hi=Math.log10(20000);
  const xOf=hz=>((Math.log10(hz)-lo)/(hi-lo))*w;
  ctx.beginPath();
  for(let x=0;x<=w;x++){
    const hz=Math.pow(10,lo+(x/w)*(hi-lo));
    let db=0;
    bands.forEach((bhz,i)=>{
      const oct=Math.log2(hz/bhz);
      if(i===0&&hz<bhz)db+=gains[i];
      else if(i===bands.length-1&&hz>bhz)db+=gains[i];
      else db+=gains[i]*Math.exp(-(oct*oct)/1.6);
    });
    const y=mid-(db/14)*mid;
    x?ctx.lineTo(x,y):ctx.moveTo(x,y);
  }
  ctx.strokeStyle=$('#eqEnabled').checked?'#72f1b8':'#4b5b6b';
  ctx.lineWidth=2;ctx.stroke();
}

function eqPaintLabels(){
  const gains=$$('#eqBank input[type=range]');
  gains.forEach(i=>{
    const v=+i.value,out=i.parentElement.parentElement.querySelector('.eqdb');
    out.textContent=(v>0?'+':'')+v;
    out.classList.toggle('up',v>0);out.classList.toggle('down',v<0);
  });
  $('#eqPreampText').textContent=`${$('#eqPreamp').value>0?'+':''}${$('#eqPreamp').value} dB`;
  eqPaintCurve();
}

function eqMarkCustom(){
  /* Moving a slider is what makes the curve custom. Reflected in the picker
     immediately rather than waiting for the round trip, because a preset that
     stays highlighted while the sliders no longer match it is a lie. */
  if(!audioCfg)return;
  const custom=audioCfg.eq.presets.length-1;
  $$('#eqPresets button').forEach((b,i)=>b.classList.toggle('on',i===custom));
}

function renderAudio(d){
  audioCfg=d;
  const eq=d.eq,v=d.voice;

  $('#eqHardware').style.display=eq.inHardware?'':'none';
  $('#eqEnabled').checked=eq.enabled;
  $('#eqPreamp').value=eq.preamp;
  $('#eqAuto').checked=eq.autoPreamp;
  $('#eqActiveHint').textContent=!eq.enabled?'Bypassed.'
    :eq.active?(+eq.headroomDb<0?`Active. ${eq.headroomDb} dB of level given back for headroom.`:'Active.')
    :'On, but every band is at zero, so nothing is being changed.';

  if(!$('#eqPresets').children.length){
    $('#eqPresets').innerHTML=eq.presets.map((pr,i)=>`<button data-preset="${i}">${esc(pr.name)}</button>`).join('');
    $$('#eqPresets button').forEach(b=>b.onclick=()=>{
      const pr=audioCfg.eq.presets[+b.dataset.preset];
      $$('#eqBank input[type=range]').forEach((inp,i)=>inp.value=pr.gain[i]);
      $$('#eqPresets button').forEach(o=>o.classList.toggle('on',o===b));
      $('#eqEnabled').checked=true;
      eqPaintLabels();
      saveAudio({eq:{...eqDraft(),preset:+b.dataset.preset}});
    });
  }
  $$('#eqPresets button').forEach((b,i)=>b.classList.toggle('on',i===eq.preset));

  if(!$('#eqBank').children.length){
    $('#eqBank').innerHTML=eq.bands.map((hz,i)=>
      `<div class="eqband"><span class="eqdb">0</span><div class="eqslot"><input type="range" min="${eq.gainMin}" max="${eq.gainMax}" step="1" value="0" data-band="${i}"></div><span class="eqhz">${EQ_LABELS(hz)}</span></div>`).join('');
    $$('#eqBank input[type=range]').forEach(inp=>{
      inp.oninput=()=>{eqPaintLabels();eqMarkCustom()};
      inp.onchange=()=>saveAudio({eq:eqDraft()});
    });
  }
  $$('#eqBank input[type=range]').forEach((inp,i)=>inp.value=eq.gain[i]);
  eqPaintLabels();

  $('#voiceEnabled').checked=v.enabled;
  $('#voiceVolume').value=v.volume;$('#voiceVolText').textContent=`${v.volume}%`;
  $('#voiceDuck').value=v.duck;$('#voiceDuckText').textContent=v.duck?`${v.duck}%`:'not at all';
  $('#voiceDevices').value=(v.devices||'').split(',').filter(Boolean)
    .map(x=>x.replace(':',' = ').trim()).join('\n');

  const CATS=[[1,'System','Boot, shutdown and the radio mode.'],
              [2,'Connections','A phone or the network coming and going. Off by default: a phone at the edge of range would otherwise have the speaker talking to an empty room.'],
              [4,'Battery','Low, critical, charging and full. The one worth interrupting music for.'],
              [8,'Internet radio','Connecting to a station, and failing to.'],
              [16,'Alarms','The alarm, snooze and the sleep timer.']];
  $('#voiceCats').innerHTML=CATS.map(([bit,name,hint])=>
    `<label class="rowitem" style="cursor:pointer"><input type="checkbox" data-cat="${bit}" ${v.categories&bit?'checked':''}><span class="grow"><b>${name}</b><small>${hint}</small></span></label>`).join('');
  $$('#voiceCats input').forEach(i=>i.onchange=()=>saveAudio({voice:voiceDraft()}));

  $('#voiceClips').innerHTML=(v.clips||[]).map(c=>
    `<div class="rowitem"><span class="grow"><b>${esc(c.text)}</b><small>${esc(c.id)}</small></span><span class="acts"><button class="btn ghost" data-say="${esc(c.id)}">Play</button></span></div>`).join('');
  $$('#voiceClips [data-say]').forEach(b=>b.onclick=async()=>{
    try{await api('/api/audio',{method:'POST',body:{action:'say',clip:b.dataset.say}});toast('Playing')}
    catch(e){toast(e.message,true)}
  });
}

function voiceDraft(){
  let bits=0;$$('#voiceCats input:checked').forEach(i=>bits|=+i.dataset.cat);
  /* The mapping is typed as "address = phrase" a line at a time because that is
     readable; the firmware stores it as one comma-separated string because that
     is one NVS key. This is the only place the two forms meet. */
  const devices=$('#voiceDevices').value.split('\n').map(l=>l.trim()).filter(Boolean)
    .map(l=>{const [mac,clip]=l.split('=').map(x=>(x||'').trim());
             return mac&&clip?`${mac.replace(/[:\-]/g,'').toLowerCase()}:${clip}`:''})
    .filter(Boolean).join(',');
  return {enabled:$('#voiceEnabled').checked,volume:+$('#voiceVolume').value,
          duck:+$('#voiceDuck').value,categories:bits,devices};
}

async function loadAudio(){try{renderAudio(await api('/api/audio'))}catch(e){toast(e.message,true)}}
async function saveAudio(body){try{renderAudio(await api('/api/audio',{method:'POST',body}))}catch(e){toast(e.message,true)}}

/* --- internet radio --------------------------------------------------- */

function fmtStreamBytes(n){
  if(!n)return '0 B';
  if(n<1024)return `${n} B`;
  if(n<1048576)return `${(n/1024).toFixed(0)} kB`;
  return `${(n/1048576).toFixed(1)} MB`;
}

function renderRadio(d){
  radioCfg=d;
  const off=!d.available;
  $('#radioOffline').style.display=off?'':'none';
  $('#radioPanels').style.display=off?'none':'';
  /* Three different negatives, and they need different words. `supported` false
     means this board does not have internet radio at all -- the code is not in
     the image -- and the backend already sent the sentence to say so. Falling
     through to the memory or the Bluetooth message would send the reader to
     look at a serial log for an allocation that never happened. */
  if(off&&d.supported===false){
    $('#radioOffline').textContent=d.reason||'Internet radio is not available on this board.';
    return;
  }
  if(off){
    $('#radioOffline').innerHTML=d.modeHasWifi
      ?'Internet radio did not start this boot &mdash; there was not enough memory left for its buffer. The serial log says so at boot; a restart usually clears it.'
      :'Internet radio needs Wi-Fi, and <b>Bluetooth only</b> mode never starts it &mdash; one antenna, one radio. Pick <b>Wi-Fi only</b> or <b>DFPlayer + Wi-Fi</b> under <b>Radio mode</b> on the Overview page and the station list comes back.';
    return;
  }

  const n=d.now||{},live=n.state==='playing'||n.state==='buffering';
  $('#radioNow').textContent=n.name||'Nothing playing';
  $('#radioNowSub').textContent=n.title||n.error||{connecting:'Opening the stream…',buffering:'Filling the buffer…',reconnecting:'The stream dropped; trying again…',idle:'Pick a station below.'}[n.state]||' ';
  $('#radioState').textContent={playing:'Playing',buffering:'Buffering',connecting:'Connecting',reconnecting:'Reconnecting',error:'Stopped',idle:'Idle'}[n.state]||'—';
  $('#radioFormat').textContent=n.sampleRate?`${n.codec} ${n.bitrate?n.bitrate+' kbps ':''}${(n.sampleRate/1000).toFixed(1)} kHz ${n.channels===1?'mono':'stereo'}`:'—';
  $('#radioUnder').textContent=n.underruns||0;
  $('#radioRecon').textContent=n.reconnects||0;
  $('#radioBytes').textContent=fmtStreamBytes(n.bytes||0);
  $('#radioBuf').style.width=`${n.buffer||0}%`;
  $('#radioBufText').textContent=live?`${n.buffer||0}%`:'—';
  /* Amber below a third: at that level a stream is about to run dry, and the
     colour is the only warning that arrives before the sound stops. */
  $('#radioBufWrap').classList.toggle('warn',live&&(n.buffer||0)<33);
  $('#radioToggle').dataset.icon=live?'stop':'play';
  $('#radioToggle').innerHTML='';icons();
  if(!radioVolBusy){$('#radioVolume').value=d.volume;$('#radioVolText').textContent=`${Math.round(d.volume/1.27)}%`}
  $('#radioAutostart').checked=!!d.autostart;

  const list=d.stations||[];
  $('#radioListEmpty').style.display=list.length?'none':'';
  $('#radioList').innerHTML=list.map((st,i)=>
    `<div class="rowitem${n.station===i&&live?' on':''}"><span class="grow"><b>${esc(st.name)}</b><small>${esc(st.url)}</small></span><span class="acts"><button class="btn" data-play="${i}">${n.station===i&&live?'Stop':'Play'}</button><button class="btn ghost" data-edit="${i}" title="Edit">Edit</button><button class="btn ghost" data-up="${i}" title="Move up">&uarr;</button><button class="btn ghost" data-del="${i}" title="Remove">&times;</button></span></div>`).join('');
  $$('#radioList [data-play]').forEach(b=>b.onclick=()=>{
    const i=+b.dataset.play;
    radioPost(n.station===i&&live?{action:'stop'}:{action:'play',index:i});
  });
  $$('#radioList [data-edit]').forEach(b=>b.onclick=()=>{
    const i=+b.dataset.edit;radioEditIndex=i;
    $('#radioName').value=list[i].name;$('#radioEditUrl').value=list[i].url;
    $('#radioEditTitle').textContent=`Edit ${list[i].name}`;
    $('#radioCancel').style.display='';
    $('#radioName').focus();
  });
  $$('#radioList [data-up]').forEach(b=>b.onclick=()=>radioPost({action:'move',index:+b.dataset.up,up:true}));
  $$('#radioList [data-del]').forEach(b=>b.onclick=()=>{
    confirmDo('Remove this station?',`${esc(list[+b.dataset.del].name)} will be deleted from the favourites.`,
      ()=>radioPost({action:'delete',index:+b.dataset.del}));
  });
}

function radioClearEditor(){
  radioEditIndex=-1;$('#radioName').value='';$('#radioEditUrl').value='';
  $('#radioEditTitle').textContent='Add a station';$('#radioCancel').style.display='none';
}
async function loadRadio(){try{renderRadio(await api('/api/radio'))}catch(e){toast(e.message,true)}}
async function radioPost(body){try{renderRadio(await api('/api/radio',{method:'POST',body}))}catch(e){toast(e.message,true)}}

/* --- alarms and the sleep timer ---------------------------------------- */

const DAY_NAMES=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
const ALARM_SOURCES=['Chime','Internet radio','Card folder'];

function fmtCountdown(sec){
  if(!sec)return '—';
  const h=Math.floor(sec/3600),m=Math.floor((sec%3600)/60);
  if(h>=24)return `${Math.floor(h/24)}d ${h%24}h`;
  if(h)return `${h}h ${m}m`;
  return m?`${m}m`:`${sec}s`;
}
function fmtDays(days){
  if(!days)return 'Once';
  if(days===0x7F)return 'Every day';
  if(days===0x3E)return 'Weekdays';
  if(days===0x41)return 'Weekends';
  return DAY_NAMES.filter((_,i)=>days&(1<<i)).join(' ');
}
function fmtWallTime(h,m){
  const d=new Date();d.setHours(h,m,0,0);
  return d.toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'});
}

let sleepStandbyDirty=false;
function renderAlarms(d){
  alarmCfg=d;
  const now=d.now||{},list=d.alarms||[];
  const ringing=now.state==='ringing',snoozed=now.state==='snoozed';

  $('#alarmDismiss').style.display=(ringing||snoozed)?'':'none';
  $('#alarmSnooze').style.display=ringing?'':'none';
  $('#alarmNextLine').innerHTML=ringing?'<b style="color:var(--mint)">An alarm is going off now.</b>'
    :snoozed?`Snoozed &mdash; back in ${fmtCountdown(now.snoozeLeftSeconds)}.`
    :now.next>=0?`Next alarm in ${fmtCountdown(now.nextInSeconds)}.`
    :'No alarm is armed.';

  $('#alarmEmpty').style.display=list.length?'none':'';
  $('#alarmAdd').style.display=list.length>=d.max?'none':'';
  /* Rebuilt only when it differs. innerHTML every two seconds throws away
     and recreates every row and every handler underneath the page, which is
     visible as a flicker on a phone and loses a half-pressed button. */
  const alarmHtml=list.map(a=>{
    const bits=[fmtDays(a.days),ALARM_SOURCES[a.source]||'Chime'];
    if(a.fadeSeconds)bits.push(`fades up over ${a.fadeSeconds<60?a.fadeSeconds+'s':Math.round(a.fadeSeconds/60)+' min'}`);
    if(a.skipNext)bits.push('skipping the next one');
    return `<div class="rowitem${a.enabled?' on':''}"><label class="switch" style="padding:0;border:0;background:none;width:auto;margin:0"><input type="checkbox" data-arm="${a.index}" ${a.enabled?'checked':''}></label><span class="grow"><b>${fmtWallTime(a.hour,a.minute)}${a.label?' · '+esc(a.label):''}</b><small>${esc(bits.join(' · '))}</small></span><span class="acts"><button class="btn ghost" data-aedit="${a.index}">Edit</button><button class="btn ghost" data-adel="${a.index}">&times;</button></span></div>`;
  }).join('');
  if($('#alarmList').innerHTML!==alarmHtml){
    $('#alarmList').innerHTML=alarmHtml;
    $$('#alarmList [data-arm]').forEach(i=>i.onchange=()=>alarmPost({action:'save',index:+i.dataset.arm,enabled:i.checked}));
    $$('#alarmList [data-aedit]').forEach(b=>b.onclick=()=>alarmEdit(+b.dataset.aedit));
    $$('#alarmList [data-adel]').forEach(b=>b.onclick=()=>confirmDo('Delete this alarm?','It will be removed from the speaker.',()=>{alarmCloseEditor();alarmPost({action:'delete',index:+b.dataset.adel})}));
  }

  $('#sleepRunning').style.display=now.sleepRunning?'':'none';
  $('#sleepIdle').style.display=now.sleepRunning?'none':'';
  if(now.sleepRunning){
    const left=now.sleepLeftSeconds,total=now.sleepTotalSeconds||1;
    $('#sleepLeft').textContent=fmtCountdown(left);
    $('#sleepSub').textContent=left<=60?'Fading out…':`of ${Math.round(total/60)} minutes`;
    $('#sleepMeter').style.width=`${Math.max(0,Math.min(100,100*left/total))}%`;
  }
  /* Only echoed until the owner touches it. It is read when a sleep preset
     is pressed, which can be a while after it is ticked, and a two-second
     poll that keeps resetting it means the box will not stay ticked. */
  if(!sleepStandbyDirty)$('#sleepStandby').checked=!!d.sleepStandbyDefault;

  /* The editor is NOT repainted from here, and that is the whole point.

     refresh() calls loadAlarms() every two seconds while this page is open,
     and this function used to end by repainting the open editor from the
     draft. Type 09:30 into the time field and two seconds later it was
     07:00 again -- because nothing wrote the typed value back into the
     draft, so the repaint restored what the draft still said.

     Binding the inputs to the draft (below) fixes the value being lost, but
     not the behaviour: a background poll writing into a field somebody is
     typing in is wrong even when it writes the right thing. It closes a
     native time picker, it moves the caret, and on a phone it dismisses the
     keyboard.

     So the split is by ownership. The list, the countdown and the ringing
     state are the speaker's and refresh freely. The editor belongs to
     whoever opened it until they save or cancel, and only alarmEdit() and
     the editor's own controls paint it. */
}

function alarmEdit(index){
  const found=(alarmCfg.alarms||[]).find(a=>a.index===index);
  alarmEditIndex=index;
  alarmDraft=found?{...found}:{index:255,enabled:true,hour:7,minute:0,days:0x3E,source:0,
    target:0,volume:90,fadeSeconds:60,durationSeconds:1800,snoozeMinutes:9,skipNext:false,label:''};
  $('#alarmEditor').style.display='';
  $('#alarmEditTitle').textContent=found?'Edit alarm':'New alarm';
  $('#alarmTest').style.display=found?'':'none';
  alarmPaintEditor();
  $('#alarmEditor').scrollIntoView({behavior:'smooth',block:'nearest'});
}
function alarmCloseEditor(){alarmEditIndex=-1;alarmDraft=null;$('#alarmEditor').style.display='none'}

function alarmPaintEditor(){
  const a=alarmDraft;if(!a)return;
  $('#alarmTime').value=`${String(a.hour).padStart(2,'0')}:${String(a.minute).padStart(2,'0')}`;
  $('#alarmLabel').value=a.label||'';
  $('#alarmDays').innerHTML=DAY_NAMES.map((n,i)=>`<button data-day="${i}" class="${a.days&(1<<i)?'on':''}">${n[0]}${n[1]}</button>`).join('')
    +`<button data-daypreset="62" style="width:auto;padding:8px 12px">Weekdays</button><button data-daypreset="65" style="width:auto;padding:8px 12px">Weekends</button><button data-daypreset="127" style="width:auto;padding:8px 12px">Every day</button><button data-daypreset="0" style="width:auto;padding:8px 12px">Once</button>`;
  $$('#alarmDays [data-day]').forEach(b=>b.onclick=()=>{a.days^=(1<<+b.dataset.day);alarmPaintEditor()});
  $$('#alarmDays [data-daypreset]').forEach(b=>b.onclick=()=>{a.days=+b.dataset.daypreset;alarmPaintEditor()});

  $$('#alarmSource button').forEach(b=>b.classList.toggle('on',+b.dataset.src===a.source));
  const radioOff=!alarmCfg.radioAvailable,dfOff=!alarmCfg.dfAvailable;
  $('#alarmSourceHint').innerHTML=a.source===1
    ?(radioOff?'This mode has no internet radio, so this alarm would fall straight through to the chime.':'If the station will not connect within twenty seconds, the chime takes over.')
    :a.source===2
    ?(dfOff?'No DFPlayer is running in this mode, so this alarm would fall straight through to the chime.':'Plays the first track in the folder you pick.')
    :'A repeating two-note figure through the speaker itself. Works in every mode and needs nothing.';

  const targetRow=$('#alarmTargetRow');
  targetRow.style.display=a.source===0?'none':'';
  if(a.source===1){
    $('#alarmTargetLabel').textContent='Station';
    $('#alarmTarget').innerHTML=(alarmCfg.stations||[]).map((n,i)=>`<option value="${i}">${esc(n)}</option>`).join('')||'<option value="0">No stations saved</option>';
  }else if(a.source===2){
    $('#alarmTargetLabel').textContent='Folder on the card';
    $('#alarmTarget').innerHTML=Array.from({length:99},(_,i)=>`<option value="${i+1}">${String(i+1).padStart(2,'0')}</option>`).join('');
  }
  $('#alarmTarget').value=String(a.target||(a.source===2?1:0));

  $('#alarmVolume').value=a.volume;$('#alarmVolText').textContent=`${Math.round(a.volume/1.27)}%`;
  $('#alarmFade').value=a.fadeSeconds;
  $('#alarmFadeText').textContent=a.fadeSeconds?(a.fadeSeconds<60?`${a.fadeSeconds} seconds`:`${Math.round(a.fadeSeconds/60)} minutes`):'no fade';
  $('#alarmDuration').value=String(a.durationSeconds);
  $('#alarmSnoozeMins').value=String(a.snoozeMinutes);
  $('#alarmSkip').checked=!!a.skipNext;
}

async function loadAlarms(){try{renderAlarms(await api('/api/alarms'))}catch(e){toast(e.message,true)}}
async function alarmPost(body){try{renderAlarms(await api('/api/alarms',{method:'POST',body}))}catch(e){toast(e.message,true)}}

/* --- graphs ------------------------------------------------------------ */

function drawChart(id,axisId,values,flags,opts){
  const c=$(id);if(!c)return;
  /* Sized from the element rather than from an attribute, so the curve is not
     stretched on a narrow window or blurred on a high-density screen. */
  const rect=c.getBoundingClientRect(),dpr=window.devicePixelRatio||1;
  c.width=Math.max(1,Math.round(rect.width*dpr));
  c.height=Math.max(1,Math.round(rect.height*dpr));
  const ctx=c.getContext('2d');ctx.scale(dpr,dpr);
  const w=rect.width,h=rect.height,pad=6;
  ctx.clearRect(0,0,w,h);

  const real=values.filter(v=>v!==null&&v!==undefined);
  if(real.length<2){
    $(axisId).textContent='';
    ctx.fillStyle='#8fa0b3';ctx.font='13px system-ui';ctx.textAlign='center';
    ctx.fillText('Not enough history yet',w/2,h/2);
    return;
  }
  let lo=Math.min(...real),hi=Math.max(...real);
  /* A flat line needs a band around it or it sits on the floor of the chart and
     reads as zero. */
  if(hi-lo<opts.minSpan){const mid=(hi+lo)/2;lo=mid-opts.minSpan/2;hi=mid+opts.minSpan/2}
  const pad2=(hi-lo)*0.12;lo-=pad2;hi+=pad2;
  const xOf=i=>pad+(i/(values.length-1))*(w-pad*2);
  const yOf=v=>h-pad-((v-lo)/(hi-lo))*(h-pad*2);

  /* The stretches where something was playing, behind the curve. On a battery
     graph this is usually the whole explanation for a curve that steepens. */
  if(flags&&opts.shade){
    ctx.fillStyle='rgba(101,169,255,.10)';
    let start=-1;
    for(let i=0;i<=flags.length;i++){
      const on=i<flags.length&&(flags[i]&2);
      if(on&&start<0)start=i;
      if(!on&&start>=0){ctx.fillRect(xOf(start),pad,Math.max(1,xOf(i-1)-xOf(start)),h-pad*2);start=-1}
    }
  }

  ctx.strokeStyle='#223040';ctx.lineWidth=1;
  for(let k=0;k<=2;k++){const y=pad+((h-pad*2)*k)/2;ctx.beginPath();ctx.moveTo(pad,y);ctx.lineTo(w-pad,y);ctx.stroke()}

  ctx.beginPath();let started=false;
  values.forEach((v,i)=>{
    if(v===null||v===undefined){started=false;return}
    const x=xOf(i),y=yOf(v);
    started?ctx.lineTo(x,y):ctx.moveTo(x,y);started=true;
  });
  ctx.strokeStyle=opts.color;ctx.lineWidth=2;ctx.lineJoin='round';ctx.stroke();

  /* A dot on the newest sample, because "which end is now" is otherwise a
     convention the reader has to remember. */
  const lastIdx=values.length-1-[...values].reverse().findIndex(v=>v!==null&&v!==undefined);
  if(lastIdx>=0&&lastIdx<values.length){
    ctx.fillStyle=opts.color;ctx.beginPath();
    ctx.arc(xOf(lastIdx),yOf(values[lastIdx]),3,0,7);ctx.fill();
  }
  $(axisId).innerHTML=`${opts.fmt(hi)}<br>${opts.fmt(lo)}`;
}

function renderGraphs(d){
  graphData=d;
  const now=d.now||{};
  $('#gUptime').textContent=fmtUp(d.uptimeSeconds*1000);
  $('#gRuntime').textContent=d.runtimeSeconds>=3600?`${Math.floor(d.runtimeSeconds/3600)} h`:`${Math.round(d.runtimeSeconds/60)} min`;
  $('#gBoots').textContent=d.bootCount||'—';
  $('#gBlock').textContent=now.largestBlock?`${now.largestBlock} kB`:'—';

  const volts=(d.volts||[]).map(v=>v===null?null:+v);
  $('#gVoltsNow').textContent=now.volts?`${(+now.volts).toFixed(2)} V · ${now.percent}%${now.charging?' · charging':''}`:'No battery gauge is fitted or enabled.';
  drawChart('#gVoltsChart','#gVoltsAxis',volts,d.flags,{color:'#72f1b8',minSpan:0.15,shade:true,fmt:v=>`${v.toFixed(2)} V`});

  const temp=(d.temperature||[]).map(v=>v===null?null:+v);
  $('#gTempNow').textContent=now.temperature?`${(+now.temperature).toFixed(1)} °C at the die`:'The sensor did not answer.';
  drawChart('#gTempChart','#gTempAxis',temp,d.flags,{color:'#f8c76a',minSpan:2,shade:true,fmt:v=>`${v.toFixed(1)} °C`});

  $('#gHeapNow').textContent=now.heap?`${now.heap} kB free · ${now.heapLow} kB was the lowest since boot`:'—';
  drawChart('#gHeapChart','#gHeapAxis',d.heap||[],d.flags,{color:'#65a9ff',minSpan:8,shade:false,fmt:v=>`${Math.round(v)} kB`});

  const rssi=(d.rssi||[]).map(v=>v===null?null:+v);
  $('#gRssiNow').textContent=now.rssi?`${now.rssi} dBm`:'Not associated with a network.';
  drawChart('#gRssiChart','#gRssiAxis',rssi,d.flags,{color:'#ff6b7a',minSpan:6,shade:false,fmt:v=>`${Math.round(v)} dBm`});
}

async function loadGraphs(){try{renderGraphs(await api('/api/telemetry'))}catch(e){toast(e.message,true)}}

/* --- UPnP / DLNA renderer ---------------------------------------------- */
let dlnaCfg=null;
function renderDlna(d){
 dlnaCfg=d;
 const card=$('#dlnaCard');
 if(!d||!d.supported){card.style.display='none';return}
 card.style.display='';
 $('#dlnaEnabled').checked=!!d.enabled;
 const bits=[];
 if(!d.enabled)bits.push('Off &mdash; nothing on the network can see this speaker.');
 else if(!d.running)bits.push('Waiting for the network.');
 else{
  bits.push(`Visible as <b>${esc(d.name)}</b> on port ${d.port}.`);
  bits.push(`Transport: <b>${esc(d.transport)}</b>`);
  if(d.title)bits.push(`Now: ${esc(d.title)}`);
  else if(d.uri)bits.push(`Now: ${esc(d.uri)}`);
  if(d.controller)bits.push(`Last controller: ${esc(d.controller)}`);
  bits.push(`${d.subscriptions} subscription(s), ${d.actions} action(s) served`);
 }
 /* Said plainly and always: "it appeared but would not play my FLAC" is the
    question this answers before it is asked. */
 bits.push(`<span class="muted">Accepts ${esc(d.formats||'')}.</span>`);
 $('#dlnaState').innerHTML=bits.join('<br>');
}
async function loadDlna(){try{renderDlna(await api('/api/dlna'))}catch(e){}}
$('#dlnaEnabled').onchange=async e=>{
 const on=e.target.checked;
 try{renderDlna(await api('/api/dlna',{method:'POST',body:{enabled:on}}));
  toast(on?'Renderer on \u2014 look for the speaker in your player app':'Renderer off')}
 catch(err){e.target.checked=!on;toast(err.message,true)}
};
/* --- Home Assistant ---------------------------------------------------- */

/* Set as soon as anything in the broker form is touched, cleared when the
   page is opened or a Save succeeds. This form is polled every ten seconds
   and saved with a button, which is the same shape as the alarm editor and
   had the same bug: type a broker address, and ten seconds later it is the
   stored one again. The connection status above the form is the speaker's
   and keeps refreshing; the fields belong to whoever is filling them in. */
let mqttFormDirty=false;
function renderMqtt(d){
  mqttCfg=d;
  $('#mqttOffline').style.display=d.modeHasWifi?'none':'';
  $('#mqttPassword').placeholder=d.passwordSet?'Leave blank to keep current':'No password';
  if(!mqttFormDirty){
    $('#mqttEnabled').checked=d.enabled;
    $('#mqttHost').value=d.host||'';
    $('#mqttPort').value=d.port||1883;
    $('#mqttUser').value=d.user||'';
    $('#mqttTopic').value=d.baseTopic||'';
    $('#mqttDiscovery').checked=d.discovery;
    $('#mqttDiscoveryPrefix').value=d.discoveryPrefix||'homeassistant';
    $('#mqttPublish').value=String(d.publishSeconds||15);
  }

  const label={off:'Off',unavailable:'Waiting for the network',connecting:'Connecting…',connected:'Connected',failed:'Could not connect'}[d.state]||d.state;
  $('#mqttState').textContent=label;
  $('#mqttStateHint').textContent=d.state==='connected'?'Connected to the broker.':label;
  $('#mqttUptime').textContent=d.connectedForSeconds?fmtUp(d.connectedForSeconds*1000):'—';
  $('#mqttConnects').textContent=d.connects||0;
  $('#mqttPublished').textContent=d.published||0;
  $('#mqttReceived').textContent=d.received||0;
  $('#mqttDisc').textContent=!d.discovery?'Switched off':d.discoveryDone?'Sent':'Pending';
  $('#mqttError').style.display=d.error?'':'none';
  $('#mqttError').textContent=d.error||'';
}
async function loadMqtt(){try{renderMqtt(await api('/api/mqtt'))}catch(e){toast(e.message,true)}}

/* --- the time-zone list ------------------------------------------------ */
/*
   Kept in the page rather than on the device. There is no zone database in the
   firmware -- it is 700 kB and it goes stale -- so the rules live in the half of
   the system that is rewritten by every firmware update anyway, and the speaker
   stores the one string it was handed.
*/
const ZONES=[['','Fixed offset (no daylight saving)'],
['GMT0BST,M3.5.0/1,M10.5.0','United Kingdom'],
['CET-1CEST,M3.5.0,M10.5.0/3','Central Europe (Paris, Berlin, Madrid)'],
['EET-2EEST,M3.5.0/3,M10.5.0/4','Eastern Europe (Athens, Helsinki)'],
['WET0WEST,M3.5.0/1,M10.5.0','Portugal'],
['MSK-3','Moscow'],
['EST5EDT,M3.2.0,M11.1.0','US Eastern'],
['CST6CDT,M3.2.0,M11.1.0','US Central'],
['MST7MDT,M3.2.0,M11.1.0','US Mountain'],
['MST7','Arizona'],
['PST8PDT,M3.2.0,M11.1.0','US Pacific'],
['AKST9AKDT,M3.2.0,M11.1.0','Alaska'],
['HST10','Hawaii'],
['AST4ADT,M3.2.0,M11.1.0','Atlantic Canada'],
['NST3:30NDT,M3.2.0,M11.1.0','Newfoundland'],
['<-03>3','Brazil (São Paulo)'],
['<-03>3','Argentina'],
['<-05>5','Colombia, Peru'],
['<-06>6','Mexico City'],
['<+0330>-3:30<+0430>,J80/0,J264/0','Iran'],
['<+04>-4','Dubai'],
['<+05>-5','Pakistan'],
['IST-5:30','India, Sri Lanka'],
['<+06>-6','Bangladesh'],
['<+07>-7','Thailand, Vietnam'],
['CST-8','China, Singapore, Hong Kong'],
['JST-9','Japan'],
['KST-9','Korea'],
['AEST-10AEDT,M10.1.0,M4.1.0/3','Sydney, Melbourne'],
['AEST-10','Brisbane'],
['ACST-9:30ACDT,M10.1.0,M4.1.0/3','Adelaide'],
['AWST-8','Perth'],
['NZST-12NZDT,M9.5.0,M4.1.0/3','New Zealand'],
['SAST-2','South Africa'],
['EAT-3','East Africa'],
['WAT-1','West Africa'],
['<+02>-2','Israel'],
['<+03>-3','Turkey, Saudi Arabia'],
['UTC0','UTC']];

function paintZones(){
  const sel=$('#clockZoneRule');if(!sel||sel.children.length)return;
  sel.innerHTML=ZONES.map(([tz,name])=>`<option value="${esc(tz)}">${esc(name)}</option>`).join('');
  sel.onchange=async()=>{
    try{await api('/api/clock',{method:'POST',body:{zone:sel.value}});toast('Time zone set');loadSettings()}
    catch(e){toast(e.message,true);loadSettings()}
  };
}
function paintZoneStatus(){
  if(!settings)return;
  paintZones();
  const sel=$('#clockZoneRule');
  if(sel&&document.activeElement!==sel)sel.value=settings.clockZone||'';
  const mins=settings.clockOffsetMinutes||0,sign=mins<0?'-':'+',abs=Math.abs(mins);
  $('#clockZoneNow').textContent=settings.clockZone?(settings.clockZoneAbbrev||'—'):'Fixed offset';
  $('#clockZoneDst').textContent=!settings.clockZone?'No rule installed':settings.clockDst?'In force':'Not in force';
  $('#clockZoneOffset').textContent=`UTC${sign}${String(Math.floor(abs/60)).padStart(2,'0')}:${String(abs%60).padStart(2,'0')}`;
  $('#clockZoneSource').textContent=settings.clockSource||'—';
}

/* --- wiring ------------------------------------------------------------ */

$('#soundRefresh').onclick=()=>loadAudio();
$('#eqEnabled').onchange=()=>saveAudio({eq:eqDraft()});
$('#eqAuto').onchange=()=>saveAudio({eq:eqDraft()});
$('#eqPreamp').oninput=()=>eqPaintLabels();
$('#eqPreamp').onchange=()=>saveAudio({eq:eqDraft()});
$('#eqSave').onclick=()=>saveAudio({eq:eqDraft()});
$('#eqFlat').onclick=()=>saveAudio({eq:{...eqDraft(),preset:0}});
$('#voiceEnabled').onchange=()=>saveAudio({voice:voiceDraft()});
$('#voiceVolume').oninput=()=>$('#voiceVolText').textContent=`${$('#voiceVolume').value}%`;
$('#voiceVolume').onchange=()=>saveAudio({voice:voiceDraft()});
$('#voiceDuck').oninput=()=>{const v=+$('#voiceDuck').value;$('#voiceDuckText').textContent=v?`${v}%`:'not at all'};
$('#voiceDuck').onchange=()=>saveAudio({voice:voiceDraft()});
$('#voiceSave').onclick=()=>saveAudio({voice:voiceDraft()});
$('#soundVolume').oninput=()=>{soundVolBusy=Date.now();$('#soundVolumeText').textContent=`${Math.round($('#soundVolume').value/1.27)}%`};
$('#soundVolume').onchange=async()=>{
  try{await api('/api/media',{method:'POST',body:{action:'volume',value:+$('#soundVolume').value}})}
  catch(e){toast(e.message,true)}
  soundVolBusy=0;refresh();
};

$('#radioRefresh').onclick=()=>loadRadio();
$('#radioToggle').onclick=()=>radioPost({action:'toggle'});
$('#radioNext').onclick=()=>radioPost({action:'next'});
$('#radioPrev').onclick=()=>radioPost({action:'previous'});
$('#radioVolume').oninput=()=>{radioVolBusy=Date.now();$('#radioVolText').textContent=`${Math.round($('#radioVolume').value/1.27)}%`};
$('#radioVolume').onchange=()=>{radioVolBusy=0;radioPost({action:'volume',value:+$('#radioVolume').value})};
$('#radioAutostart').onchange=()=>radioPost({action:'autostart',value:$('#radioAutostart').checked});
$('#radioPlayUrl').onclick=()=>{
  const url=$('#radioUrl').value.trim();
  if(!url)return toast('Paste a stream address first',true);
  radioPost({action:'url',url});
};
$('#radioSave').onclick=()=>{
  const url=$('#radioEditUrl').value.trim();
  if(!url)return toast('A station needs a stream address',true);
  radioPost({action:'save',index:radioEditIndex<0?255:radioEditIndex,
             name:$('#radioName').value.trim(),url}).then(radioClearEditor);
};
$('#radioCancel').onclick=()=>radioClearEditor();

$('#alarmRefresh').onclick=()=>loadAlarms();
$('#alarmAdd').onclick=()=>alarmEdit(255);
$('#alarmCancel').onclick=()=>alarmCloseEditor();
$('#alarmDismiss').onclick=()=>alarmPost({action:'dismiss'});
$('#alarmSnooze').onclick=()=>alarmPost({action:'snooze'});
$('#alarmTest').onclick=()=>{if(alarmEditIndex>=0&&alarmEditIndex<255)alarmPost({action:'test',index:alarmEditIndex})};
$$('#alarmSource button').forEach(b=>b.onclick=()=>{
  if(!alarmDraft)return;
  alarmDraft.source=+b.dataset.src;
  alarmDraft.target=alarmDraft.source===2?1:0;
  alarmPaintEditor();
});
$('#alarmTarget').onchange=()=>{if(alarmDraft)alarmDraft.target=+$('#alarmTarget').value};
$('#alarmVolume').oninput=()=>{if(alarmDraft){alarmDraft.volume=+$('#alarmVolume').value;$('#alarmVolText').textContent=`${Math.round(alarmDraft.volume/1.27)}%`}};
$('#alarmFade').oninput=()=>{if(alarmDraft){alarmDraft.fadeSeconds=+$('#alarmFade').value;
  $('#alarmFadeText').textContent=alarmDraft.fadeSeconds?(alarmDraft.fadeSeconds<60?`${alarmDraft.fadeSeconds} seconds`:`${Math.round(alarmDraft.fadeSeconds/60)} minutes`):'no fade'}};
$('#alarmSkip').onchange=()=>{if(alarmDraft)alarmDraft.skipNext=$('#alarmSkip').checked};
/* The time, the label and the two durations feed the draft as they are
   typed. Without this they were read only when Save was pressed, so any
   repaint in between -- clicking Weekdays, changing the source -- restored
   the value the draft still held and silently discarded what was typed.
   That was true before the poll ever entered into it. */
$('#alarmTime').oninput=()=>{
 if(!alarmDraft)return;
 const [h,m]=($('#alarmTime').value||'').split(':').map(Number);
 if(Number.isFinite(h)&&Number.isFinite(m)){alarmDraft.hour=h;alarmDraft.minute=m}
};
$('#alarmLabel').oninput=()=>{if(alarmDraft)alarmDraft.label=$('#alarmLabel').value};
$('#alarmDuration').onchange=()=>{if(alarmDraft)alarmDraft.durationSeconds=+$('#alarmDuration').value};
$('#alarmSnoozeMins').onchange=()=>{if(alarmDraft)alarmDraft.snoozeMinutes=+$('#alarmSnoozeMins').value};
$('#sleepStandby').onchange=()=>{sleepStandbyDirty=true};
$('#alarmSave').onclick=()=>{
  if(!alarmDraft)return;
  const [h,m]=($('#alarmTime').value||'07:00').split(':').map(Number);
  alarmPost({action:'save',index:alarmEditIndex<0?255:alarmEditIndex,enabled:true,
    hour:h,minute:m,days:alarmDraft.days,source:alarmDraft.source,
    target:+$('#alarmTarget').value||0,volume:+$('#alarmVolume').value,
    fadeSeconds:+$('#alarmFade').value,durationSeconds:+$('#alarmDuration').value,
    snoozeMinutes:+$('#alarmSnoozeMins').value,skipNext:$('#alarmSkip').checked,
    label:$('#alarmLabel').value.trim()}).then(alarmCloseEditor);
};
$$('#sleepPresets button').forEach(b=>b.onclick=()=>{sleepStandbyDirty=false;alarmPost({action:'sleep',minutes:+b.dataset.min,standby:$('#sleepStandby').checked})});
$('#sleepStart').onclick=()=>{
  const m=+$('#sleepCustom').value;
  if(!m)return toast('How many minutes?',true);
  alarmPost({action:'sleep',minutes:m,standby:$('#sleepStandby').checked});
};
$('#sleepPlus').onclick=()=>alarmPost({action:'sleepExtend',minutes:15});
$('#sleepCancel').onclick=()=>alarmPost({action:'sleepCancel'});
$('#sleepStandby').onchange=()=>alarmPost({action:'sleepStandbyDefault',value:$('#sleepStandby').checked});

$('#alarmOvStop').onclick=()=>alarmPost({action:'dismiss'});
$('#alarmOvSnooze').onclick=()=>alarmPost({action:'snooze'});
$('#alarmOvSleepCancel').onclick=()=>alarmPost({action:'sleepCancel'});
$('#graphRefresh').onclick=()=>loadGraphs();
$('#mqttRefresh').onclick=()=>loadMqtt();
$('#mqttAnnounce').onclick=async()=>{
  try{await api('/api/mqtt',{method:'POST',body:{action:'announce'}});toast('Sent to Home Assistant')}
  catch(e){toast(e.message,true)}
};
/* One listener per field rather than a delegated one, because these are not
   inside a shared container that nothing else lives in. */
['mqttEnabled','mqttHost','mqttPort','mqttUser','mqttTopic','mqttDiscovery',
 'mqttDiscoveryPrefix','mqttPublish'].forEach(id=>{
  const el=$('#'+id);
  if(el)el.addEventListener('input',()=>{mqttFormDirty=true});
});
$('#mqttSave').onclick=async()=>{
  const body={enabled:$('#mqttEnabled').checked,host:$('#mqttHost').value.trim(),
    port:+$('#mqttPort').value||1883,user:$('#mqttUser').value.trim(),
    baseTopic:$('#mqttTopic').value.trim(),discovery:$('#mqttDiscovery').checked,
    discoveryPrefix:$('#mqttDiscoveryPrefix').value.trim(),publishSeconds:+$('#mqttPublish').value};
  /* Only send the password when one was typed: an empty field means "leave the
     stored one alone", and the field starts empty on every load because the
     firmware never sends it back. */
  const pw=$('#mqttPassword').value;
  if(pw)body.password=pw;
  /* The dirty flag is cleared only once the speaker has accepted the form.
     Clearing it before the request would let the next poll overwrite the
     fields with the stored values if the save failed, which is the moment
     somebody most needs to still see what they typed. */
  try{const saved=await api('/api/mqtt',{method:'POST',body});
    mqttFormDirty=false;renderMqtt(saved);$('#mqttPassword').value='';toast('Saved')}
  catch(e){toast(e.message,true)}
};
$('#mqttEnabled').onchange=()=>$('#mqttSave').click();

/* Charts are drawn at the size of their element, so a resized window needs a
   repaint. Debounced, because a drag fires this continuously. */
let graphResize;
addEventListener('resize',()=>{
  clearTimeout(graphResize);
  graphResize=setTimeout(()=>{if(graphData&&$('#page-graphs').classList.contains('active'))renderGraphs(graphData)},200);
});


/* --- keeping the new pages live ---------------------------------------- */
/*
   The two-second status poll already runs; these hang off it rather than
   starting timers of their own, so a dashboard left open on the Radio page
   makes two requests every two seconds instead of one plus a second poll that
   nobody remembered to stop when the page changed.

   Only the visible page is fetched. The graphs are the exception: two hours of
   history is the largest document this firmware assembles, and refetching it
   every two seconds would have the speaker building 4 kB of JSON continuously
   for a picture that changes once every thirty seconds.
*/
let pageTick=0;
function refreshActivePage(s){
  if($('#page-sound').classList.contains('active')){
    /* The transport on the Sound page mirrors the Overview's, and both are
       driven from the status document rather than from a second request. */
    const src=s.radio&&s.radio.state&&s.radio.state!=='idle'?'radio'
             :(s.mode&&s.mode.dfplayer)?'card':'bluetooth';
    const m=s.media||{};
    $('#soundNow').textContent=m.title||{radio:'Internet radio',card:'The card',bluetooth:'Bluetooth'}[src];
    $('#soundNowSub').textContent=m.artist||(s.radio&&s.radio.name)||' ';
    if(!soundVolBusy||Date.now()-soundVolBusy>2500){
      const v=src==='radio'&&s.radio?s.radio.volume:m.volume;
      if(typeof v==='number'){$('#soundVolume').value=v;$('#soundVolumeText').textContent=`${Math.round(v/1.27)}%`}
    }
  }
  /* The page you are looking at, and only that page. The Radio and Alarms
     documents are small and carry numbers that move by the second -- a buffer
     level, a countdown -- so they keep the two-second cadence. The broker's
     connection statistics do not move like that, and two hours of history moves
     once a minute, so both are fetched far more slowly. */
  if($('#page-radio').classList.contains('active'))loadRadio();
  /* Backed off to every ten seconds while the editor is open. Nothing behind
     an open editor is worth two-second freshness, and the fetch itself is
     served from loop() on the speaker. Still polled rather than stopped, so
     an alarm that starts ringing mid-edit still raises its Dismiss button. */
  if($('#page-alarms').classList.contains('active')&&
     (!alarmDraft||(pageTick%5)===0))loadAlarms();
  if($('#page-hass').classList.contains('active')&&(pageTick%5)===0)loadMqtt();
  if($('#page-graphs').classList.contains('active')&&(pageTick%30)===0)loadGraphs();
  pageTick++;
}

/* --- the radio on the Overview ----------------------------------------- */
function renderRadioOverview(s){
  const r=s.radio||{};
  const card=$('#radioCard');
  if(!card)return;
  /* Only shown in the modes that have a radio, and only once there is
     something to say -- an empty card in Bluetooth mode is a card that has to
     be explained. */
  const show=r.available&&r.state&&r.state!=='idle';
  card.style.display=show?'':'none';
  if(!show)return;
  const live=r.state==='playing'||r.state==='buffering';
  $('#radioOvName').textContent=r.name||'Internet radio';
  $('#radioOvDetail').textContent=r.title||r.error||
    {connecting:'Opening the stream…',buffering:`Buffering — ${r.buffer||0}% of the way there`,
     reconnecting:'The stream dropped; trying again…'}[r.state]||
    (r.bitrate?`${r.codec} · ${r.bitrate} kbps`:'Playing');
  $('#radioOvBadge').textContent={playing:'Playing',buffering:'Buffering',connecting:'Connecting',
    reconnecting:'Reconnecting',error:'Stopped'}[r.state]||r.state;
  $('#radioOvBadge').className='badge '+(r.state==='playing'?'good':'');
  $('#radioOvBuf').style.width=`${r.buffer||0}%`;
  $('#radioOvBufWrap').classList.toggle('warn',live&&(r.buffer||0)<33);
}

/* --- the alarm and sleep timer on the Overview -------------------------- */
function renderAlarmOverview(s){
  const a=s.alarm||{},card=$('#alarmCard');
  if(!card)return;
  const show=a.state!=='idle'||a.sleepRunning||a.next>=0;
  card.style.display=show?'':'none';
  if(!show)return;
  if(a.state==='ringing'){
    $('#alarmOvBig').textContent='Alarm';
    $('#alarmOvDetail').textContent='Ringing now.';
    $('#alarmOvBadge').textContent='Ringing';
    $('#alarmOvBadge').className='badge good';
  }else if(a.state==='snoozed'){
    $('#alarmOvBig').textContent=fmtCountdown(a.snoozeLeftSeconds);
    $('#alarmOvDetail').textContent='Snoozed.';
    $('#alarmOvBadge').textContent='Snoozed';
    $('#alarmOvBadge').className='badge';
  }else if(a.sleepRunning){
    $('#alarmOvBig').textContent=fmtCountdown(a.sleepLeftSeconds);
    $('#alarmOvDetail').textContent=a.sleepLeftSeconds<=60?'Sleep timer — fading out…':'Left on the sleep timer.';
    $('#alarmOvBadge').textContent='Sleep timer';
    $('#alarmOvBadge').className='badge good';
  }else{
    $('#alarmOvBig').textContent=fmtCountdown(a.nextInSeconds);
    $('#alarmOvDetail').textContent='Until the next alarm.';
    $('#alarmOvBadge').textContent='Armed';
    $('#alarmOvBadge').className='badge';
  }
  $('#alarmOvStop').style.display=(a.state==='ringing'||a.state==='snoozed')?'':'none';
  $('#alarmOvSnooze').style.display=a.state==='ringing'?'':'none';
  $('#alarmOvSleepCancel').style.display=a.sleepRunning?'':'none';
}

function page(name){$$('[data-page]').forEach(b=>b.classList.toggle('active',b.dataset.page===name));{const nav=$('#mobileNav'),btn=nav&&nav.querySelector(`[data-page="${name}"]`);if(btn&&nav.scrollWidth>nav.clientWidth)nav.scrollTo({left:btn.offsetLeft-(nav.clientWidth-btn.offsetWidth)/2,behavior:'smooth'})}$$('.page').forEach(p=>p.classList.toggle('active',p.id===`page-${name}`));$('#pageTitle').textContent=name[0].toUpperCase()+name.slice(1);$('#eyebrow').textContent={overview:'Your audio, at a glance',devices:'Pairing and connections',media:'The card, the drive and the module',wifi:'Network management',updates:'Reliable A/B firmware',settings:'Make it yours',lighting:'Colour, motion and music',sound:'Tone, level and what it says out loud',radio:'Stations from the network',alarms:'Waking up, and going to sleep',graphs:'Two hours of history',hass:'MQTT and home automation'}[name];
if(name==='hass')$('#pageTitle').textContent='Home Assistant';if(name==='devices')loadDevices();if(name==='settings')loadSettings();if(name==='lighting')loadLighting();
if(name==='sound')loadAudio();
if(name==='radio'){loadRadio();loadDlna()};
if(name==='alarms'){loadAlarms();loadSettings()}
if(name==='graphs')loadGraphs();
if(name==='hass'){mqttFormDirty=false;loadMqtt()}if(name==='media'){if(status)renderDfPage(status.dfplayer||{});refresh();loadDfLibrary()}else{dfLibPoll(false)}if(name==='settings'&&settings&&settings.power&&settings.power.wokeFromSleep)toast('This speaker woke from standby')}
$$('[data-page]').forEach(b=>b.onclick=()=>page(b.dataset.page));
$('#loginForm').onsubmit=async e=>{e.preventDefault();let raw=`admin:${$('#loginPassword').value}`;auth='Basic '+btoa(unescape(encodeURIComponent(raw)));try{const session=await api('/api/auth');auth='Bearer '+session.token;sessionStorage.setItem('speakerAuth',auth);$('#loginPassword').value='';$('#loginModal').classList.remove('show');$('#loginError').textContent='';await refresh();loadSettings();clearInterval(pollTimer);scheduleRefresh()}catch(err){auth='';$('#loginError').textContent='Incorrect password. Please try again.'}};
async function media(action,value){try{await api('/api/media',{method:'POST',body:{action,value}});setTimeout(refresh,180)}catch(e){toast(e.message,true)}}
$$('[data-media]').forEach(b=>b.onclick=()=>media(b.dataset.media));$('#volume').oninput=e=>{$('#volumeText').textContent=`${Math.round(e.target.value/127*100)}%`;clearTimeout(volumeTimer);volumeTimer=setTimeout(()=>media('volume',+e.target.value),80)};$('#refresh').onclick=()=>refresh(true);async function df(action,extra={}){try{await api('/api/dfplayer',{method:'POST',body:{action,...extra}});setTimeout(refresh,220)}catch(e){toast(e.message,true)}}$$('[data-dfsource]').forEach(b=>b.onclick=()=>dfSource(b.dataset.dfsource));$$('[data-dfsrc]').forEach(b=>b.onclick=()=>dfSource(b.dataset.dfsrc));async function dfSource(v){dfOpenFolder=0;dfStarted=null;await df('source',{value:v});setTimeout(loadDfLibrary,600)}$$('[data-dfeq]').forEach(b=>b.onclick=()=>df('eq',{value:+b.dataset.dfeq}));$$('[data-dfloop]').forEach(b=>b.onclick=()=>df('loop',{value:b.dataset.dfloop,folder:+$('#dfFolderNo').value||1}));$$('[data-dfpin]').forEach(b=>b.onclick=()=>df('pin',{pin:b.dataset.dfpin,long:!!b.dataset.dflong}));$$('[data-dfled]').forEach(b=>b.onclick=()=>df('led',{value:b.dataset.dfled}));$('#dfRefresh').onclick=()=>df('refresh');$('#dfScan').onclick=async()=>{dfOpenFolder=0;await df('scan');toast('Scanning the card, folder by folder…');setTimeout(loadDfLibrary,300);dfLibPoll(true)};$('#dfReset').onclick=()=>confirmDo('Reset the DFPlayer?','Playback stops and the module re-runs its whole start-up sequence, which takes a couple of seconds.',()=>df('reset'));$('#dfStandby').onclick=()=>df('standby');$('#dfWake').onclick=()=>df('wake');$('#dfPlayTrack').onclick=()=>df('track',{value:+$('#dfTrackNo').value});$('#dfPlayMp3').onclick=()=>df('mp3',{value:+$('#dfMp3No').value});$('#dfAdvert').onclick=()=>df('advert',{value:+$('#dfAdvertNo').value});$('#dfAdvertStop').onclick=()=>df('advertStop');$('#dfPlayFolder').onclick=()=>df('folder',{folder:+$('#dfFolderNo').value,file:+$('#dfFileNo').value});$('#dfFolderNo').oninput=e=>{clearTimeout(e.target._t);e.target._t=setTimeout(()=>{let f=+e.target.value;if(f>=1&&f<=99)df('queryFolder',{folder:f})},450)};$('#dfVolume').oninput=e=>{$('#dfVolText').textContent=`${e.target.value} / ${e.target.max}`;clearTimeout(e.target._t);e.target._t=setTimeout(()=>df('volumeRaw',{value:+e.target.value}),110)};$('#dfVolUp').onclick=()=>df('volumeStep',{up:true});$('#dfVolDown').onclick=()=>df('volumeStep',{up:false});$('#dfDac').onchange=e=>df('dac',{value:e.target.checked});$('#dfSaveDefaults').onclick=async()=>{await df('saveDefaults',{autoplay:$('#dfDefAutoplay').checked});toast('Saved. These are sent to the module at every boot.');loadSettings()};$('#dfDefVolume').oninput=e=>$('#dfDefVolText').textContent=`${e.target.value} / ${e.target.max}`;$('#batCalibrate').onclick=async()=>{let v=+$('#batActual').value;if(!v)return toast('Type the voltage your meter reads first',true);try{let r=await api('/api/battery',{method:'POST',body:{action:'calibrate',volts:v}});toast(`Trim ${(+r.calibration).toFixed(4)} stored`);$('#batActual').value='';loadSettings();refresh()}catch(e){toast(e.message,true)}};
$('#reloadDevices').onclick=loadDevices;$('#scanWifi').onclick=async()=>{let b=$('#scanWifi'),old=b.textContent;b.disabled=true;b.innerHTML='<i class="spinner"></i> Scanning';try{let d=await api('/api/wifi/scan'),box=$('#networks');box.innerHTML=d.networks.length?d.networks.map(n=>`<div class="network" data-ssid="${esc(n.ssid)}"><b>${esc(n.ssid||'(hidden network)')}</b><small>Channel ${n.channel} · ${n.secure?'Secured':'Open'}</small><span class="signal">${n.rssi} dBm</span></div>`).join(''):'<div class="empty">No networks found</div>';$$('[data-ssid]').forEach(n=>n.onclick=()=>{$('#wifiSsid').value=n.dataset.ssid;$('#wifiPassword').focus()})}catch(e){toast(e.message,true)}finally{b.disabled=false;b.textContent=old;icons()}};
$('#wifiForm').onsubmit=e=>{e.preventDefault();confirmDo('Change Wi-Fi network?',`The speaker will restart and connect to “${$('#wifiSsid').value}”.`,async()=>{try{await api('/api/wifi',{method:'POST',body:{ssid:$('#wifiSsid').value,password:$('#wifiPassword').value}});toast('Saved. Speaker is restarting…')}catch(e){toast(e.message,true)}})};
$('#checkUpdate').onclick=()=>updateAction('check');$('#overviewInstall').onclick=()=>{page('updates');confirmInstall()};$('#installUpdate').onclick=confirmInstall;function confirmInstall(){confirmDo('Install firmware update?','Playback will pause and the speaker will restart when installation completes.',()=>updateAction('install'))}
async function updateAction(kind){try{await api(`/api/update/${kind}`,{method:'POST'});toast(kind==='check'?'Checking GitHub…':'Update started');refresh()}catch(e){toast(e.message,true)}}
const dz=$('#dropzone'),file=$('#firmwareFile');dz.onclick=()=>file.click();dz.onkeydown=e=>{if(e.key==='Enter'||e.key===' ')file.click()};['dragenter','dragover'].forEach(n=>dz.addEventListener(n,e=>{e.preventDefault();dz.classList.add('over')}));['dragleave','drop'].forEach(n=>dz.addEventListener(n,e=>{e.preventDefault();dz.classList.remove('over')}));dz.ondrop=e=>selectFile(e.dataTransfer.files[0]);file.onchange=e=>selectFile(e.target.files[0]);function selectFile(f){if(!f)return;if(!f.name.toLowerCase().endsWith('.spk'))return toast('Choose a .spk firmware file',true);selectedFile=f;$('#fileName').textContent=`${f.name} · ${fmtBytes(f.size)}`;$('#uploadFirmware').disabled=false}
$('#uploadFirmware').onclick=()=>confirmDo('Install uploaded firmware?','The file will be written to the inactive OTA slot, then the speaker will restart.',uploadFirmware);function uploadFirmware(){let x=new XMLHttpRequest;x.open('POST','/api/update/upload');x.setRequestHeader('Authorization',auth);x.setRequestHeader('X-Firmware-Size',selectedFile.size);x.setRequestHeader('Content-Type','application/octet-stream');x.upload.onprogress=e=>{if(e.lengthComputable){$('#updateProgress').style.width=`${e.loaded/e.total*100}%`;$('#updateMessage').textContent=`Uploading ${Math.round(e.loaded/e.total*100)}%`}};x.onload=()=>{let d={};try{d=JSON.parse(x.responseText)}catch{};x.status<300?toast('Firmware installed. Restarting…'):toast(d.error||'Upload failed',true)};x.onerror=()=>toast('Upload connection failed',true);x.send(selectedFile)}
$('#saveSettings').onclick=async()=>{let body={deviceName:$('#deviceName').value,hostname:$('#hostname').value,apAlways:$('#apAlways').checked,githubRepo:$('#githubRepo').value.trim(),githubAsset:$('#githubAsset').value.trim()||'*.spk',clearGithubToken:$('#clearGithubToken').checked,dfSource:+$('#dfDefSource').value,dfVolume:+$('#dfDefVolume').value,dfEq:+$('#dfDefEq').value,dfLoop:+$('#dfDefLoop').value,dfLoopFolder:+$('#dfDefLoopFolder').value,dfAutoplay:$('#dfDefAutoplay').checked,batteryEnabled:$('#batEnabled').checked,batteryCells:+$('#batCells').value,batteryDivider:+$('#batDivider').value,batteryFull:+$('#batFull').value,batteryEmpty:+$('#batEmpty').value,batteryLow:+$('#batLow').value,batteryCritical:+$('#batCritical').value};if($('#adminPassword').value)body.adminPassword=$('#adminPassword').value;if($('#apPassword').value)body.apPassword=$('#apPassword').value;if($('#githubToken').value)body.githubToken=$('#githubToken').value;try{await api('/api/settings',{method:'POST',body});toast('Settings saved. Restart to apply identity changes.');$('#adminPassword').value=$('#apPassword').value=$('#githubToken').value='';loadSettings()}catch(e){toast(e.message,true)}};
/*
 * Settings backup and restore.
 *
 * The download cannot be a plain link, and is not a GET: every endpoint here
 * wants the dashboard password in an Authorization header, which a link would
 * arrive without, and the backup passphrase must not travel in a query string
 * where it would land in browser history and in logs. So it is a POST read back
 * as a blob, named here rather than from the Content-Disposition header.
 */
$('#backupSettings').onclick=async()=>{const b=$('#backupSettings'),old=b.textContent,pass=$('#backupPass').value;if(pass&&pass.length<8)return toast('A backup passphrase must be at least 8 characters',true);b.disabled=true;b.innerHTML='<i class="spinner"></i> '+(pass?'Encrypting':'Preparing');try{const r=await fetch('/api/settings/backup',{method:'POST',headers:{Authorization:auth,'Content-Type':'application/json'},body:JSON.stringify({passphrase:pass})});if(r.status===401){sessionStorage.removeItem('speakerAuth');$('#loginModal').classList.add('show');throw Error('Sign in required')}if(!r.ok){let d={};try{d=await r.json()}catch{}throw Error(d.error||`Backup failed (${r.status})`)}const text=await r.text(),name=`${(settings&&settings.hostname)||'esp32-blue-spk'}-settings-${new Date().toISOString().slice(0,10)}.json`,url=URL.createObjectURL(new Blob([text],{type:'application/json'})),a=document.createElement('a');a.href=url;a.download=name;document.body.appendChild(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);toast(pass?`Saved ${name} — keep the passphrase, it cannot be recovered`:`Saved ${name} — without the secrets`)}catch(e){toast(e.message,true)}finally{b.disabled=false;b.textContent=old}};
/*
 * The file is read and parsed here before anything is sent, so the wrong file
 * picked by mistake costs nothing -- and so the card can say which speaker and
 * which firmware the backup came from, and whether it needs a passphrase, while
 * there is still time to change your mind. The parsed object goes to the API,
 * not the raw text, because api() stringifies whatever it is given and a string
 * would arrive double-encoded.
 */
let restoreDoc=null;const rdz=$('#restoreDrop'),rfile=$('#restoreFile');rdz.onclick=()=>rfile.click();rdz.onkeydown=e=>{if(e.key==='Enter'||e.key===' ')rfile.click()};['dragenter','dragover'].forEach(n=>rdz.addEventListener(n,e=>{e.preventDefault();rdz.classList.add('over')}));['dragleave','drop'].forEach(n=>rdz.addEventListener(n,e=>{e.preventDefault();rdz.classList.remove('over')}));rdz.ondrop=e=>pickRestore(e.dataTransfer.files[0]);rfile.onchange=e=>pickRestore(e.target.files[0]);
async function pickRestore(f){restoreDoc=null;$('#restoreSettings').disabled=true;$('#restorePassRow').style.display='none';$('#restoreFileName').textContent='No file selected';if(!f)return;let text;try{text=await f.text()}catch{return toast('That file could not be read',true)}let d;try{d=JSON.parse(text)}catch{return toast('That file is not valid JSON',true)}if(!d||typeof d!=='object'||!d.settings)return toast('That file is not a settings backup',true);restoreDoc=d;$('#restoreSettings').disabled=false;const enc=!!d.secrets;$('#restorePassRow').style.display=enc?'':'none';const when=d.createdEpoch?new Date(d.createdEpoch*1000).toISOString().slice(0,10):'date unknown';$('#restoreFileName').textContent=`${d.device||f.name} · firmware ${d.firmware||'?'} · ${when} · ${enc?'encrypted secrets':'no secrets'}`}
$('#restoreSettings').onclick=()=>{if(restoreDoc&&restoreDoc.secrets&&!$('#restorePass').value)return toast('This backup is encrypted — enter its passphrase',true);confirmDo('Restore these settings?','Every stored setting the file carries is overwritten — the Wi-Fi network and, if the backup is encrypted, both passwords — and the speaker restarts. If the backup carries a different dashboard password, signing back in needs that one.',doRestore)};
async function doRestore(){const b=$('#restoreSettings'),old=b.textContent;b.disabled=true;b.innerHTML='<i class="spinner"></i> Restoring';try{const body={...restoreDoc};if(restoreDoc.secrets)body.passphrase=$('#restorePass').value;const r=await api('/api/settings/restore',{method:'POST',body});$('#restorePass').value='';toast(r.message||'Settings restored; restarting…')}catch(e){toast(e.message,true)}finally{b.disabled=false;b.textContent=old}}
function renderPowerMode(mode){$$('[data-power]').forEach(b=>b.classList.toggle('on',+b.dataset.power===mode));$('#powerThresholdRow').classList.toggle('off',mode!==2)}
async function powerPatch(body){try{await api('/api/settings',{method:'POST',body})}catch(e){toast(e.message,true)}finally{loadSettings()}}
$$('[data-power]').forEach(b=>b.onclick=()=>{const m=+b.dataset.power;renderPowerMode(m);powerPatch({powerMode:m,powerThreshold:+$('#powerThreshold').value});toast(m===0?'Power saving off':(m===1?'Power saving on':'Power saving follows the battery'))});
// Debounced like every other slider here: dragging it should not be one POST
// per pixel, and the effects apply the moment the value lands anyway.
$('#powerThreshold').oninput=e=>{$('#powerThresholdText').textContent=`${e.target.value}%`;clearTimeout(e.target._t);e.target._t=setTimeout(()=>powerPatch({powerMode:2,powerThreshold:+e.target.value}),260)};
function renderSleepMode(mode){$$('[data-sleep]').forEach(b=>b.classList.toggle('on',+b.dataset.sleep===mode));$('#sleepTimeoutRow').classList.toggle('off',mode===0)}
$$('[data-sleep]').forEach(b=>b.onclick=()=>{const m=+b.dataset.sleep;renderSleepMode(m);powerPatch({sleepMode:m,sleepAfterSeconds:+$('#sleepAfter').value});toast(m===0?'Standby off':(m===1?'Standby after the timeout':'Standby only while saving'))});
$('#sleepAfter').onchange=e=>powerPatch({sleepAfterSeconds:+e.target.value});
function renderIndicatorMode(mode){$$('[data-indicator]').forEach(b=>b.classList.toggle('on',+b.dataset.indicator===mode));$('#indicatorTimeoutRow').classList.toggle('off',mode!==2)}
// Which of the three layers has the LED dark, in words: the mute (saving) sits
// above the mode, which sits above the timeout, and the card says the one that
// is actually responsible rather than making the owner work it out.
function indicatorText(pw){const m=pw.indicator??1,after=fmtSecs(pw.indicatorAfterSeconds??300);if(pw.indicatorWired===false)return 'No indicator pin is compiled in (PIN_STATUS_LED is -1), so these settings are kept but change nothing.';if(pw.indicatorMuted)return 'Held dark by '+(pw.saving?'power saving':'standby')+' right now, whatever is chosen above. It comes back the moment saving ends.';if(m===0)return 'Off. The speaker still knows its state; it just does not blink it.';if(m===1)return 'Lit, showing the current state.';return pw.indicatorResting?`Dark: nothing has happened for ${after}. The next event lights it again.`:`Lit; goes dark ${after} after the last event.`}
$$('[data-indicator]').forEach(b=>b.onclick=()=>{const m=+b.dataset.indicator;renderIndicatorMode(m);powerPatch({indicatorMode:m,indicatorAfterSeconds:+$('#indicatorAfter').value});toast(m===0?'Indicator off':(m===1?'Indicator always on':'Indicator goes dark after the timeout'))});
$('#indicatorAfter').onchange=e=>powerPatch({indicatorAfterSeconds:+e.target.value});
$('#standbyNow').onclick=()=>confirmDo('Put the speaker into standby?','Everything goes dark, both radios come down and playback stops. Press BOOT on the speaker to bring it back — it restarts, so give it a few seconds.',async()=>{try{const r=await api('/api/system',{method:'POST',body:{action:'standby'}});toast(r.message||'Going into standby')}catch(e){toast(e.message,true)}});
function renderOledBlank(mode){$$('[data-blank]').forEach(b=>b.classList.toggle('on',+b.dataset.blank===mode));$('#oledTimeoutRow').classList.toggle('off',mode===0)}
// The panel settings go through /api/settings rather than /api/display, which
// only carries the actions that change nothing on disk. They still apply at
// once, so the Save button is not part of the deal.
async function oledBlank(body){try{await api('/api/settings',{method:'POST',body});loadSettings()}catch(e){toast(e.message,true);loadSettings()}}
$$('[data-blank]').forEach(b=>b.onclick=()=>{const m=+b.dataset.blank;renderOledBlank(m);oledBlank({oledBlankMode:m,oledBlankAfterSeconds:+$('#oledTimeout').value});toast(m===0?'The panel stays on':(m===1?'The panel switches off when idle':'The panel switches off on a timer'))});
$('#oledTimeout').onchange=e=>oledBlank({oledBlankMode:+($$('[data-blank]').find(b=>b.classList.contains('on'))||{dataset:{blank:0}}).dataset.blank,oledBlankAfterSeconds:+e.target.value});
$('#ledIdleOff').onchange=e=>{$('#ledIdleRow').classList.toggle('off',!e.target.checked);if(ledCfg)ledCfg.idleOff=e.target.checked;sendLeds({idleOff:e.target.checked,idleAfterSeconds:+$('#ledIdleAfter').value});toast(e.target.checked?'The ring rests when idle':'The ring stays lit')};
$('#ledIdleAfter').onchange=e=>sendLeds({idleOff:$('#ledIdleOff').checked,idleAfterSeconds:+e.target.value});
$$('[data-screen]').forEach(b=>b.onclick=()=>display('screen',+b.dataset.screen));$$('[data-display]').forEach(b=>b.onclick=()=>display(b.dataset.display));async function display(action,value){try{await api('/api/display',{method:'POST',body:{action,value}});toast('Display updated')}catch(e){toast(e.message,true)}}$('#brightness').oninput=e=>{$('#brightnessValue').textContent=+e.target.value?e.target.value:'Auto';clearTimeout(e.target._t);e.target._t=setTimeout(()=>display('brightness',+e.target.value),130)};
async function clockPref(body,msg){try{await api('/api/clock',{method:'POST',body});if(msg)toast(msg)}catch(e){toast(e.message,true)}finally{refresh();loadSettings()}}$('#syncClock').onclick=()=>{let d=new Date;clockPref({year:d.getFullYear(),month:d.getMonth()+1,day:d.getDate(),hour:d.getHours(),minute:d.getMinutes(),second:d.getSeconds(),offsetMinutes:-d.getTimezoneOffset()},'Clock synchronized')};$$('[data-clockfmt]').forEach(b=>b.onclick=()=>{let h24=b.dataset.clockfmt==='24';$$('[data-clockfmt]').forEach(x=>x.classList.toggle('on',x===b));if(devClock){devClock.h24=h24;drawClock()}clockPref({use24h:h24},h24?'24-hour clock':'12-hour clock')});$('#clockAutoSync').onchange=e=>clockPref({autoSync:e.target.checked},e.target.checked?'Network time sync on':'Network time sync off; the speaker keeps the time you set');$$('[data-mode]').forEach(el=>el.onclick=()=>{let n=+el.dataset.mode,t=MODES[n];confirmDo('Switch to '+t.name+'?',t.warn,async()=>{try{let r=await api('/api/system',{method:'POST',body:{action:'mode',mode:n}});toast(r.message)}catch(e){n===1?toast('Switching to Bluetooth mode; this page is now offline.'):toast(e.message,true)}})});$('#reboot').onclick=()=>confirmDo('Restart speaker?','Bluetooth and the dashboard will be unavailable briefly.',()=>system('reboot'));$('#factoryReset').onclick=()=>confirmDo('Factory reset everything?','This permanently clears Wi-Fi credentials, dashboard settings, and every Bluetooth bond.',()=>system('factoryReset'));async function system(action){try{await api('/api/system',{method:'POST',body:{action}});toast('Speaker is restarting…')}catch(e){toast(e.message,true)}}
function confirmDo(title,text,fn){$('#confirmTitle').textContent=title;$('#confirmText').textContent=text;confirmAction=fn;$('#confirmModal').classList.add('show')}$('#cancelConfirm').onclick=()=>$('#confirmModal').classList.remove('show');$('#acceptConfirm').onclick=()=>{let fn=confirmAction;$('#confirmModal').classList.remove('show');confirmAction=null;if(fn)fn()};
applyCapabilities();
if(auth){api('/api/auth').then(session=>{auth='Bearer '+session.token;sessionStorage.setItem('speakerAuth',auth);$('#loginModal').classList.remove('show');refresh();loadSettings();scheduleRefresh()}).catch(()=>{})}
