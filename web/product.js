// Product controls extend the dashboard: every existing card stays in place.
let productConfig=null, productLoading=null, catalogTracks=[];
const productModeNames=['Network radio','Bluetooth','DFPlayer'];
function ptext(id,value){const node=document.getElementById(id);if(node)node.textContent=value}
function durationText(seconds){return `${Math.floor(seconds/60)}:${String(seconds%60).padStart(2,'0')}`}
async function productLoad(savedCards=null){
  if(productLoading)return productLoading;
  productLoading=(async()=>{
    const loaded=await api('/api/product');
    const retained=savedCards?$$('.product-card input,.product-card select,#alternativeUrl,#alternativeIndex,#verifiedStreams')
      .filter(n=>n.type!=='file'&&!savedCards.has(n.closest('article')))
      .map(n=>({node:n,value:n.value,checked:n.checked})):[];
    productConfig=loaded;renderProductConfig();
    retained.forEach(({node,value,checked})=>{node.value=value;node.checked=checked});return productConfig;
  })();
  try{return await productLoading}finally{productLoading=null}
}
async function productAction(action,extra={}){
  try{await api('/api/product',{method:'POST',body:{action,...extra}});await refresh();return true}
  catch(e){toast(e.message,true);return false}
}
async function productSave(settings){
  const cards=new Set;
  for(const [key,id] of Object.entries({scenes:'saveScene',favorites:'saveFavorite',volumeMax:'saveComfort',startupVolume:'saveComfort',balance:'saveComfort',mono:'saveComfort',sunriseMinutes:'saveSunrise',alternatives:'saveAlternative',verifiedTls:'saveAlternative'}))
    if(key in settings)cards.add($('#'+id).closest('article'));
  if(await productAction('configure',{settings})){
    try{await productLoad(cards);toast('Saved on speaker')}catch(error){toast('Saved; reconnect to reload settings',true)}
  }
}
function renderProductConfig(){
  const c=productConfig;if(!c)return;
  $('#sceneChips').innerHTML=c.scenes.map((s,i)=>s.enabled?`<button class="btn" data-scene="${i}" style="border-color:#${Number(s.color).toString(16).padStart(6,'0')}">${esc(s.name)}</button>`:'').join('');
  $('#favoriteChips').innerHTML=c.favorites.map((f,i)=>f.enabled?`<button class="btn" data-favorite="${i}">${esc(f.name)}</button>`:'').join('')||'<span class="hint">Add your first favorite below.</span>';
  $$('[data-scene]').forEach(b=>b.onclick=()=>{
    const i=+b.dataset.scene,s=c.scenes[i];
    if(status&&status.mode&&status.mode.id!==s.mode)confirmDo('Apply scene and change profile?','The speaker restarts to change radio profile. Bluetooth mode disconnects this dashboard.',()=>productAction('scene',{index:i}));
    else productAction('scene',{index:i});
  });
  $$('[data-favorite]').forEach(b=>b.onclick=()=>productAction('favorite',{index:+b.dataset.favorite}));
  for(const [id,items] of [['sceneIndex',c.scenes],['favoriteIndex',c.favorites]]){
    const selected=$('#'+id).value;
    $('#'+id).innerHTML=items.map((s,i)=>`<option value="${i}">${i+1}. ${esc(s.name||'Empty')}</option>`).join('');
    if(selected)$('#'+id).value=selected;
  }
  $('#comfortMax').value=c.volumeMax;$('#comfortStart').value=c.startupVolume;
  $('#comfortBalance').value=c.balance;$('#comfortMono').checked=c.mono;
  $('#sunriseMinutes').value=c.sunriseMinutes;$('#verifiedStreams').checked=c.verifiedTls;
  for(const id of ['focusScene','breakScene']){
    const select=$('#'+id),value=select.value;
    select.innerHTML='<option value="-1">Keep current sound</option>'+c.scenes.map((s,i)=>s.enabled?`<option value="${i}">${esc(s.name)}</option>`:'').join('');select.value=value||'-1';
  }
  loadSceneEditor();loadFavoriteEditor();loadAlternativeEditor();
}
function loadSceneEditor(){
  if(!productConfig)return;const s=productConfig.scenes[+$('#sceneIndex').value];
  for(const k of ['name','mode','target','volume','eq','effect','brightness','sleepMinutes','screen'])$('#scene_'+k).value=s[k];
  $('#scene_color').value='#'+Number(s.color).toString(16).padStart(6,'0');$('#scene_enabled').checked=s.enabled;
}
function loadFavoriteEditor(){
  if(!productConfig)return;const f=productConfig.favorites[+$('#favoriteIndex').value];
  for(const k of ['name','mode','folder','track'])$('#favorite_'+k).value=f[k];
  $('#favorite_enabled').checked=f.enabled;
}
function loadAlternativeEditor(){if(productConfig)$('#alternativeUrl').value=productConfig.alternatives[+$('#alternativeIndex').value]||''}
$('#sceneIndex').onchange=loadSceneEditor;$('#favoriteIndex').onchange=loadFavoriteEditor;
$('#alternativeIndex').onchange=loadAlternativeEditor;
$('#saveScene').onclick=async()=>{
  if(!productConfig)await productLoad();const scenes=structuredClone(productConfig.scenes),s=scenes[+$('#sceneIndex').value];
  s.name=$('#scene_name').value.trim();if(!s.name)return toast('Name the scene',true);
  for(const k of ['mode','target','volume','eq','effect','brightness','sleepMinutes','screen'])s[k]=+$('#scene_'+k).value;
  s.enabled=$('#scene_enabled').checked;s.color=parseInt($('#scene_color').value.slice(1),16);
  await productSave({scenes});
};
$('#saveFavorite').onclick=async()=>{
  if(!productConfig)await productLoad();const favorites=structuredClone(productConfig.favorites),f=favorites[+$('#favoriteIndex').value];
  f.name=$('#favorite_name').value.trim();if(!f.name)return toast('Name the favorite',true);
  for(const k of ['mode','folder','track'])f[k]=+$('#favorite_'+k).value;
  f.enabled=$('#favorite_enabled').checked;await productSave({favorites});
};
$('#saveComfort').onclick=()=>productSave({volumeMax:+$('#comfortMax').value,startupVolume:+$('#comfortStart').value,balance:+$('#comfortBalance').value,mono:$('#comfortMono').checked});
$('#saveSunrise').onclick=()=>productSave({sunriseMinutes:+$('#sunriseMinutes').value});
$('#saveAlternative').onclick=()=>{const alternatives=[...productConfig.alternatives];alternatives[+$('#alternativeIndex').value]=$('#alternativeUrl').value.trim();productSave({alternatives,verifiedTls:$('#verifiedStreams').checked})};
$('#focusStart').onclick=()=>productAction('focus',{minutes:+$('#focusMinutes').value,breakMinutes:+$('#focusBreakMinutes').value,workScene:+$('#focusScene').value,breakScene:+$('#breakScene').value});
$('#focusStop').onclick=()=>productAction('focusStop');
$$('[data-noise]').forEach(b=>b.onclick=()=>productAction('noise',{mode:+b.dataset.noise}));
$$('[data-shift]').forEach(b=>b.onclick=()=>productAction('timeShift',{command:b.dataset.shift,seconds:15}));
$('#guestPair').onclick=()=>{
  if(status?.mode?.id!==1)confirmDo('Switch to Bluetooth and pair?','The speaker restarts, opens pairing for two minutes and disconnects this dashboard. Hold BOOT to return to Wi-Fi.',()=>productAction('pair'));
  else productAction('pair');
};
$('#supportDownload').onclick=async()=>{
  try{const doc=await api('/api/support');doc.observedChecks={};$$('[data-observed]').forEach(c=>doc.observedChecks[c.dataset.observed]=c.checked?'operator confirmed':'not checked');downloadJson(doc,'speaker-support.json')}catch(e){toast(e.message,true)}
};
function downloadJson(doc,name){const url=URL.createObjectURL(new Blob([JSON.stringify(doc,null,2)],{type:'application/json'}));const a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000)}
$('#productExport').onclick=async()=>{try{downloadJson(await api('/api/product'),'speaker-scenes.json')}catch(e){toast(e.message,true)}};
$('#productImport').onchange=async e=>{
  const file=e.target.files[0];if(!file)return;
  try{if(file.size>8192)throw Error('Scene file must be under 8 KB');const doc=JSON.parse(await file.text());if(doc.schema!==1)throw Error('Unsupported settings schema');await productSave(doc)}catch(error){toast(error.message,true)}finally{e.target.value=''}
};
async function loadCatalog(){
  try{catalogTracks=await api('/api/catalog');renderCatalog()}catch(e){toast(e.message,true)}
}
function renderCatalog(){
  const query=$('#catalogSearch').value.toLocaleLowerCase();
  $('#catalogRows').innerHTML=catalogTracks.filter(t=>(t.title+' '+t.artist).toLocaleLowerCase().includes(query)).map(t=>`<div class="catalog-track"><span><b>${esc(t.title)}</b><small>${esc(t.artist||'')} · ${t.folder?`Folder ${t.folder}`:'MP3'} / ${t.track}</small></span><button class="btn" data-catalog-folder="${t.folder}" data-catalog-track="${t.track}" aria-label="Play ${esc(t.title)}">Play</button></div>`).join('')||'<p>No matching tracks. Import a catalog to name your music.</p>';
  $$('[data-catalog-track]').forEach(b=>b.onclick=()=>+b.dataset.catalogFolder?df('folder',{folder:+b.dataset.catalogFolder,file:+b.dataset.catalogTrack}):df('mp3',{value:+b.dataset.catalogTrack}));
  ptext('catalogCount',`${catalogTracks.length} named tracks`);
}
$('#catalogSearch').oninput=renderCatalog;$('#catalogReload').onclick=loadCatalog;
$('#catalogImport').onchange=async e=>{
  const file=e.target.files[0];if(!file)return;
  try{if(file.size>14000)throw Error('Catalog must be under 14 KB');const tracks=JSON.parse(await file.text());if(!Array.isArray(tracks))throw Error('Catalog must contain a track array');if(await productAction('catalog',{tracks}))await loadCatalog()}catch(error){toast(error.message,true)}finally{e.target.value=''}
};
$('#catalogExport').onclick=()=>downloadJson(catalogTracks,'speaker-catalog.json');
function productRender(s){
  const p=s.product;if(!p)return;
  if(!productConfig&&!productLoading)productLoad().catch(()=>{});
  ptext('activeScene',p.sceneName);ptext('focusCountdown',durationText(p.focusSeconds||0));
  ptext('focusPhase',p.focusSeconds?(p.focusBreak?'Break':'Focus'):'Ready for a session');
  ptext('soundscapeState',['Off','Pink noise','Brown noise'][p.noise]||'Off');
  if(p.noise){
    $('#volume').disabled=false;$('#playButton').disabled=false;
    ptext('title',p.noise===1?'Pink noise':p.noise===2?'Brown noise':'Audio channel test');
    ptext('artist','Soundscapes');ptext('streamLabel','Local audio');
    ptext('controlHint','Adjust the volume or pause to stop the soundscape.');
  }
  ptext('comfortSummary',`Maximum ${Math.round(p.volumeMax/127*100)}%`);
  ptext('healthHeap',fmtBytes(p.minInternalHeap));ptext('healthBlock',fmtBytes(p.minInternalBlock));
  ptext('healthUpdate',p.updateHealth);ptext('healthStorage',p.storage?'Ready':'Unavailable');
  ptext('pairingState',p.pairingSeconds?`${p.pairingSeconds}s remaining`:'Pairing closed');
  ptext('sunriseSummary',p.sunriseMinutes?`${p.sunriseMinutes} minute sunrise`:'Sunrise off');
  const t=p.timeShift||{};
  const radio=s.radio;
  if(radio?.available&&!p.noise&&(!s.mode?.dfplayer||!['idle','error'].includes(radio.state))){
    const active=!['idle','error'].includes(radio.state);
    $('#volume').disabled=false;
    $$('[data-media]').forEach(b=>b.disabled=['forward','rewind'].includes(b.dataset.media)?!t.available:false);
    $('#playButton').setAttribute('aria-label',active&&!t.paused?'Pause':'Play');
    $('#playButton svg').innerHTML=paths[active&&!t.paused?'pause':'play'];
    ptext('streamLabel',t.paused?'Radio paused':active?'Live radio':'Radio ready');
    ptext('codec',radio.codec||'—');
    ptext('controlHint',t.available?'Pause and rewind this live MP3 stream.':active?'Live stream: pause stops the connection.':'Choose a favorite or start a station on the Radio page.');
  }
  if(p.alternativeStation>=0)ptext('radioOvDetail',`Using backup URL for station ${p.alternativeStation+1}`);
  ptext('shiftPosition',t.available?(t.paused?'Paused':t.behindSeconds>2?`${t.behindSeconds}s behind live`:'Live'):'Live MP3 on WROVER');
  ptext('shiftDetail',t.available?`${t.retainedSeconds}s retained · ${t.overruns} buffer overruns`:'Starts automatically with a supported stream');
  $$('[data-shift]').forEach(b=>b.disabled=!t.available);
  $('#shiftBuffer').max=Math.max(1,t.retainedSeconds||1);$('#shiftBuffer').value=t.behindSeconds||0;
  const title=$('#title').textContent;
  ptext('compactTitle',title);$('#compactVolume').value=$('#volume').value;
  $('#persistentPlayer').classList.toggle('show',!$('#page-overview').classList.contains('active'));
  const artwork=localStorage.getItem('speakerArtwork');
  if(artwork&&/^data:image\/(png|jpeg|webp);base64,/.test(artwork)){const img=$('#playerArtwork');img.src=artwork;img.hidden=false}
}
$('#compactPlay').onclick=()=>media('toggle');$('#compactVolume').onchange=e=>media('volume',+e.target.value);
$('#artworkFile').onchange=async e=>{
  const file=e.target.files[0];if(!file)return;
  if(!['image/png','image/jpeg','image/webp'].includes(file.type)||file.size>150000)return toast('Use a PNG, JPEG or WebP under 150 KB',true);
  const reader=new FileReader;reader.onload=()=>{try{localStorage.setItem('speakerArtwork',reader.result);if(status)productRender(status)}catch{toast('Browser storage is full',true)}};reader.readAsDataURL(file);
};
$('#clearArtwork').onclick=()=>{localStorage.removeItem('speakerArtwork');$('#playerArtwork').hidden=true};

// Preserve the card layout while making controls usable without a mouse.
function accessibilityPass(){
  $$('label').forEach(label=>{const field=label.querySelector('input,select,textarea')||label.parentElement.querySelector('input,select,textarea');if(field&&field.id&&!label.htmlFor)label.htmlFor=field.id});
  $$('button').forEach(button=>{
    if(!button.textContent.trim()&&!button.getAttribute('aria-label'))button.setAttribute('aria-label',button.title||button.dataset.media||button.dataset.icon||button.id.replace(/([A-Z])/g,' $1'));
  });
  $$('.modal').forEach(modal=>{modal.setAttribute('role','dialog');modal.setAttribute('aria-modal','true');const heading=modal.querySelector('h2');if(heading){if(!heading.id)heading.id=modal.id+'Heading';modal.setAttribute('aria-labelledby',heading.id)}});
  $('#toast').setAttribute('role','status');$('#toast').setAttribute('aria-live','polite');
}
let modalReturnFocus=null;
new MutationObserver(records=>{for(const record of records){const modal=record.target;if(!modal.classList.contains('modal'))continue;if(modal.classList.contains('show')){modalReturnFocus=document.activeElement;modal.querySelector('input,button,[tabindex]')?.focus()}else modalReturnFocus?.focus()}}).observe(document.body,{subtree:true,attributes:true,attributeFilter:['class']});
document.addEventListener('keydown',e=>{
  const modal=$('.modal.show');if(!modal)return;
  if(e.key==='Escape'&&modal.id==='confirmModal'){$('#cancelConfirm').click();return}
  if(e.key!=='Tab')return;
  const nodes=[...modal.querySelectorAll('button:not(:disabled),input:not(:disabled),select:not(:disabled),[tabindex="0"]')].filter(n=>n.offsetParent!==null);
  if(!nodes.length)return;
  if(e.shiftKey&&document.activeElement===nodes[0]){nodes.at(-1).focus();e.preventDefault()}
  else if(!e.shiftKey&&document.activeElement===nodes.at(-1)){nodes[0].focus();e.preventDefault()}
});
accessibilityPass();

$$('[data-selftest]').forEach(b=>b.onclick=()=>productAction('selfTest',{test:b.dataset.selftest}));

$('#logoutSpeaker').onclick=()=>{auth='';sessionStorage.removeItem('speakerAuth');clearTimeout(pollTimer);$('#loginModal').classList.add('show')};
