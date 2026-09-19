const {test,expect}=require('@playwright/test');
const scene={name:'Evening',enabled:true,mode:0,target:0,volume:40,eq:0,effect:1,brightness:50,screen:0,color:0xff9933,sleepMinutes:0};
async function speaker(page,board='wrover'){
  const config={schema:1,storage:true,volumeMax:100,startupVolume:40,balance:0,mono:false,sunriseMinutes:10,verifiedTls:true,
    scenes:Array.from({length:8},(_,i)=>({...scene,enabled:i===0})),favorites:Array.from({length:6},()=>({name:'Favorite',enabled:false,mode:2,folder:1,track:1})),alternatives:Array(8).fill('')};
  const state={firmware:{version:'4.0.0'},system:{uptimeMs:42000,heapFree:123000},wifi:{connected:true,ssid:'Listening room',ip:'192.168.1.8',rssi:-48,apClients:0},
    bluetooth:{active:false,connected:false},media:{title:'Evening jazz',artist:'Live radio',volume:40,state:'playing'},update:{busy:false,available:false},mode:{id:0},battery:{wired:true,enabled:true,present:true,percent:78,volts:3.95,cells:1,state:'discharging'},alarm:{state:'idle',next:-1},
    product:{sceneName:'Evening',focusSeconds:1500,focusBreak:false,noise:0,volumeMax:100,minInternalHeap:100000,minInternalBlock:64000,updateHealth:'Application health confirmed',storage:true,pairingSeconds:0,sunriseMinutes:10,timeShift:{available:board==='wrover',retainedSeconds:62,behindSeconds:0,overruns:0}}};
  const commands=[],errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.addInitScript(()=>sessionStorage.setItem('speakerAuth','Bearer test'));
  await page.route('**/api/**',async route=>{
    const req=route.request(),path=new URL(req.url()).pathname;let body={};
    if(req.method()==='POST'&&path==='/api/update/upload'){
      commands.push({path,contentType:req.headers()['content-type'],bytes:req.postDataBuffer().length});body={ok:true};
    }else if(req.method()==='POST'){
      const command=req.postDataJSON();commands.push({path,...command});
      if(path==='/api/product'&&command.action==='configure')Object.assign(config,command.settings);
      body={ok:true};
    }else if(path==='/api/auth')body={token:'a'.repeat(64)};
    else if(path==='/api/status')body=state;
    else if(path==='/api/product')body=config;
    else if(path==='/api/capabilities')body={board:{module:board==='wrover'?'ESP32-WROVER-E':'ESP32-WROOM-32D'},capabilities:{radio:{available:true}},profiles:[]};
    else if(path==='/api/catalog')body=[{folder:1,track:1,title:'Quiet morning',artist:'Local music'}];
    else if(path==='/api/settings')return route.fulfill({status:503,json:{error:'Settings fixture unavailable'}});
    await route.fulfill({json:body});
  });
  await page.goto('/');await expect(page.locator('#title')).toHaveText('Evening jazz');
  await expect(page.locator('#sceneChips')).toContainText('Evening');
  return {commands,errors,state};
}
test('original cards and new glanceable data stay on the dashboard',async({page})=>{
  const {errors}=await speaker(page);
  for(const id of ['wifiName','firmwareVersion','volume','batPercent','activeScene','focusCountdown','healthHeap'])await expect(page.locator('#page-overview #'+id)).toBeVisible();
  await expect(page.locator('#focusCountdown')).toHaveText('25:00');
  await expect(page.locator('#toast')).not.toHaveClass(/show/);
  await page.screenshot({path:'test-results/dashboard-desktop.png',fullPage:true,animations:'disabled'});
  expect(errors).toEqual([]);
});
test('scene edits, focus and soundscape controls send real API commands',async({page})=>{
  const {commands,errors}=await speaker(page);
  await page.locator('#sceneIndex').evaluate(e=>e.closest('details').open=true);
  await page.locator('#scene_name').fill('Reading');await page.locator('#saveScene').click();
  await expect(page.locator('#sceneChips')).toContainText('Reading');
  await page.locator('#focusStart').click();await page.locator('[data-noise="2"]').click();
  expect(commands.some(c=>c.action==='configure'&&c.settings.scenes[0].name==='Reading')).toBeTruthy();
  expect(commands.some(c=>c.action==='focus')).toBeTruthy();expect(commands.some(c=>c.action==='noise'&&c.mode===2)).toBeTruthy();
  expect(errors).toEqual([]);
});
test('mobile dashboard fits viewport and WROOM disables rewind',async({page})=>{
  await page.setViewportSize({width:390,height:844});const {errors}=await speaker(page,'wroom');
  await expect(page.locator('[data-shift="rewind"]')).toBeDisabled();
  expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBeTruthy();
  await expect(page.locator('#toast')).not.toHaveClass(/show/);
  await page.screenshot({path:'test-results/dashboard-mobile.png',fullPage:true,animations:'disabled'});expect(errors).toEqual([]);
});
test('concurrent refresh requests coalesce and failed connections show recovery state',async({page})=>{
  await speaker(page);let requests=0;
  await page.route('**/api/status',async route=>{requests++;await new Promise(r=>setTimeout(r,200));await route.fulfill({status:503,json:{error:'Temporarily unavailable'}})});
  await page.evaluate(()=>Promise.all([refresh(),refresh(),refresh()]));expect(requests).toBe(1);
  await expect(page.locator('#connectionBanner')).toHaveClass(/show/);
});
test('saving comfort preserves an unfinished scene edit',async({page})=>{
  await speaker(page);
  await page.locator('#sceneIndex').evaluate(e=>e.closest('details').open=true);
  await page.locator('#scene_name').fill('Unfinished scene');
  await page.locator('#comfortMax').fill('90');await page.locator('#saveComfort').click();
  await expect(page.locator('#comfortSummary')).toBeVisible();
  await expect(page.locator('#scene_name')).toHaveValue('Unfinished scene');
});
test('radio playback uses the overview transport on WROOM',async({page})=>{
  const {state,commands}=await speaker(page,'wroom');
  state.radio={available:true,state:'playing',name:'Jazz',codec:'MP3',volume:40};
  await page.evaluate(()=>refresh());
  await expect(page.locator('#volume')).toBeEnabled();await expect(page.locator('#playButton')).toBeEnabled();
  await page.locator('#playButton').click();expect(commands.some(c=>c.path==='/api/media'&&c.action==='toggle')).toBeTruthy();
});
test('firmware uploads use bounded raw binary transport',async({page})=>{
  const {commands}=await speaker(page);await page.evaluate(()=>page('updates'));
  await page.locator('input[type=file][accept*=".spk"]').setInputFiles({name:'firmware-wroom.spk',mimeType:'application/octet-stream',buffer:Buffer.alloc(256,1)});
  await page.locator('#uploadFirmware').click();await page.locator('#acceptConfirm').click();
  await expect.poll(()=>commands.find(c=>c.path==='/api/update/upload')).toMatchObject({contentType:'application/octet-stream',bytes:256});
});
