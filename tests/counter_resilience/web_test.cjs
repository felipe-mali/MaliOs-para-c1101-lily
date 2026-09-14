const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const fs=require('fs'),path=require('path'),assert=require('node:assert/strict'),zlib=require('node:zlib');
const embedded=process.env.COUNTER_EMBEDDED==='1';
function asset(name){
 const source=fs.readFileSync(path.resolve('embedded_resources/web_interface',name));
 if(!embedded)return source;
 const header=fs.readFileSync('include/webFiles.h','utf8');
 const match=header.match(new RegExp(`const uint8_t ${name.replace('.', '_')}\\[\\] PROGMEM = \\{([\\s\\S]*?)\\};`));
 assert.ok(match,`embedded ${name}`);
 const unpacked=zlib.gunzipSync(Buffer.from(match[1].match(/0x[0-9a-f]+/gi).map(n=>parseInt(n,16))));
 // The existing build minifies assets before gzip; exercise that shipped output.
 assert.ok(unpacked.length>0,`embedded asset decodes: ${name}`);return unpacked;
}
(async()=>{const browser=await chromium.launch({headless:true});try{
 for(const mobile of [false,true]){
  const context=await browser.newContext({viewport:mobile?{width:390,height:844}:{width:1440,height:1000},isMobile:mobile,hasTouch:mobile});
  const page=await context.newPage(),errors=[],requests=[];let stopCalled=false,stallStatus=false,statusBlocked=false,releaseStatus;
  const cats=[{id:0,name:'Wi-Fi',modes:[{id:0,name:'SCAN NETWORKS',minInterval:4000},{id:3,name:'PACKET LOSS TEST',minInterval:500}]},{id:1,name:'BLE',modes:[{id:0,name:'BLE SCANNER',minInterval:1000},{id:4,name:'RECONNECT TEST',minInterval:2000},{id:6,name:'NOTIFICATION TEST',simulationOnly:true,minInterval:1000}]}];
  let state={state:'IDLE',category:0,mode:0,simulation:true,elapsed:0,events:0,success:0,failures:0,tx:0,rx:0,attempts:0,graph:[],sequence:0};
  page.on('pageerror',e=>errors.push(e.message));
  await page.route('http://counter.test/**',async route=>{const r=route.request(),u=new URL(r.url()),p=u.pathname;const json=(data,status=200)=>route.fulfill({status,contentType:'application/json',body:JSON.stringify(data)});
   if(p.startsWith('/api/counter/')){
    if(p.endsWith('/capabilities'))return json({categories:cats,historyLimit:20});
    if(p.endsWith('/status')){if(stallStatus){statusBlocked=true;await new Promise(resolve=>{releaseStatus=resolve;});stallStatus=false;}
     if(state.state==='RUNNING'){state.elapsed+=1000;state.events+=10;state.success+=9;state.failures++;state.graph=[20,30,10];state.successRate=90;state.rate=10;state.attempts=state.events;state.avgTime=20;state.minTime=10;state.maxTime=30;state.tx=state.events;state.rx=state.success;state.retries=state.events-1;}return json(state);}
    if(p.endsWith('/targets'))return json({category:1,items:[{name:'LabSensor',address:'02:11:22:33:44:55',rssi:-55,connectable:true,advType:0}]});
    if(p.endsWith('/history'))return json({items:[]});
    if(p.endsWith('/stop')){stopCalled=true;state.state='STOPPED';state.pending=false;if(releaseStatus)releaseStatus();return json({message:'STOP requested'},202);}
    if(p.endsWith('/start')||p.endsWith('/scan')){const cfg=JSON.parse(new URLSearchParams(r.postData()).get('config'));requests.push(cfg);state={...state,...cfg,state:p.endsWith('/scan')?'CONFIGURING':'RUNNING',pending:false,sequence:state.sequence+1,modeName:'RECONNECT TEST',message:'SIMULATION'};return json({message:'Queued'},202);}
    return json({message:'Saved'});
   }
   if(p==='/'||p==='/index.css'||p==='/index.js')return route.fulfill({contentType:p==='/'?'text/html':p.endsWith('.js')?'text/javascript':'text/css',body:asset(p==='/'?'index.html':p.slice(1))});
   if(p==='/theme.css')return route.fulfill({contentType:'text/css',body:''});
   if(p==='/systeminfo')return json({MALIOS_VERSION:'test',SD:{used:'0 B',total:'0 B'},LittleFS:{used:'1 KB',total:'1 MB'}});
   if(p==='/listfiles')return route.fulfill({contentType:'text/plain',body:''});
   return json({items:[]});
  });
  await page.goto('http://counter.test/');await page.locator('[data-webui-target=counter]').click();
  await page.waitForFunction(()=>document.getElementById('ct-category').options.length===2);
  assert.equal(await page.locator('#ct-category option').count(),2,'only available categories');
  await page.selectOption('#ct-category','1');await page.selectOption('#ct-mode','4');
  assert.equal(await page.locator('label[for="ct-password"]').isVisible(),false);
  assert.equal(await page.locator('label[for="ct-frequency"]').isVisible(),false);
  await page.selectOption('#ct-intensity','2000');assert.equal(await page.inputValue('#ct-interval'),'8000');
  await page.selectOption('#ct-intensity','500');assert.equal(await page.inputValue('#ct-interval'),'2000');
  await page.selectOption('#ct-intensity','1000');
  assert.equal(await page.inputValue('#ct-interval'),'4000');await page.selectOption('#ct-duration','0');
  await page.click('#ct-start');await page.waitForFunction(()=>document.getElementById('ct-state').textContent==='EXECUTANDO');
  assert.equal(requests.at(-1).simulation,true);assert.equal(requests.at(-1).duration,0);assert.equal(requests.at(-1).interval,4000);
  assert.ok(await page.locator('#ct-stop').isEnabled());await page.click('#ct-stop');
  await page.waitForFunction(()=>document.getElementById('ct-state').textContent==='PARADO');assert.equal(stopCalled,true);
  await page.selectOption('#ct-mode','6');assert.equal(await page.isChecked('#ct-simulation'),true);assert.equal(await page.isDisabled('#ct-simulation'),true);
  await page.selectOption('#ct-mode','0');await page.uncheck('#ct-simulation');await page.click('#ct-scan');
  await page.waitForFunction(()=>document.getElementById('ct-targets').options.length===1);await page.selectOption('#ct-targets','0');
  assert.equal(await page.inputValue('#ct-target'),'02:11:22:33:44:55');assert.match(await page.textContent('#ct-target-info'),/conect\u00e1vel/);
  await page.selectOption('#ct-mode','4');await page.click('#ct-start');
  await page.waitForFunction(()=>document.getElementById('ct-message').dataset.error==='true');assert.match(await page.textContent('#ct-message'),/AUTORIZADOS/);
  assert.equal(requests.length,2,'unacknowledged active start rejected locally');
  await page.check('#ct-simulation');await page.click('#ct-start');await page.waitForFunction(()=>document.getElementById('ct-state').textContent==='EXECUTANDO');
  await page.evaluate(()=>window.scrollTo(0,0));
  await page.screenshot({path:`tests/counter_resilience/${mobile?'mobile':'desktop'}.png`,fullPage:true});
  assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth),'no horizontal overflow');
  stopCalled=false;stallStatus=true;
  for(let i=0;i<30&&!statusBlocked;++i)await new Promise(resolve=>setTimeout(resolve,100));
  assert.equal(statusBlocked,true,'status request deliberately stalled');
  await page.click('#ct-stop');assert.equal(stopCalled,true,'STOP bypasses stalled status request');
  await page.waitForFunction(()=>document.getElementById('ct-state').textContent==='PARADO');
  assert.deepEqual(errors,[]);console.log(`${mobile?'mobile':'desktop'} PASS: capabilities, distinct intensities, simulation, scan/select, authorization, live status, STOP during stalled poll`);await context.close();
 }
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exitCode=1;});
