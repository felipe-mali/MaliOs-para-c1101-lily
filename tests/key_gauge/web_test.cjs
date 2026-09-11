// Browser integration tests against the real WebUI assets and an in-memory API fixture.
// Set PLAYWRIGHT_MODULE to an installed playwright module path when not on NODE_PATH.
const {chromium}=require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const fs=require('fs'),path=require('path'),assert=require('node:assert/strict');
const zlib=require('node:zlib');
const embedded=process.env.KEY_GAUGE_EMBEDDED==='1';
function asset(name){
 if(!embedded)return fs.readFileSync(path.resolve('embedded_resources/web_interface',name));
 const header=fs.readFileSync('include/webFiles.h','utf8');
 const match=header.match(new RegExp(`const uint8_t ${name.replace('.', '_')}\\[\\] PROGMEM = \\{([\\s\\S]*?)\\};`));
 assert.ok(match,`embedded ${name}`);
 return zlib.gunzipSync(Buffer.from(match[1].match(/0x[0-9a-f]+/gi).map(n=>parseInt(n,16))));
}
(async()=>{
 const browser=await chromium.launch({headless:true});
 try {
  for(const mobile of [false,true]) {
   const context=await browser.newContext({viewport:mobile?{width:390,height:844}:{width:1440,height:1000},isMobile:mobile,hasTouch:mobile,deviceScaleFactor:mobile?2:1});
   const page=await context.newPage(),errors=[],writes=[];
   const original={name:'DEVICE_001',points:6,thickness:5,profileWidth:90,levels:[2,2,5,3,6,1]};
   const profiles=new Map([[original.name,original]]);let broken=false;
   page.on('pageerror',e=>errors.push(e.message));page.on('dialog',d=>d.accept());
   await page.route('http://keygauge.test/**',async route=>{
    const r=route.request(),u=new URL(r.url()),p=u.pathname;
    const json=(data,status=200)=>route.fulfill({status,contentType:'application/json',body:JSON.stringify(data)});
    if(p.startsWith('/api/keygauge/')){
     if(p.endsWith('/profiles'))return json({items:[...profiles.keys()],nextName:'PROFILE_001'});
     const name=u.searchParams.get('name');
     if(p.endsWith('/profile')&&r.method()==='GET')return broken?json({points:99}):profiles.has(name)?json(profiles.get(name)):json({error:'Perfil inexistente'},404);
     if(r.method()==='DELETE'){profiles.delete(name);writes.push({method:'DELETE',name});return json({message:'Excluido'});}
     const body=new URLSearchParams(r.postData()),profile=JSON.parse(body.get('profile'));writes.push({path:p,profile});
     if(p.endsWith('/preview'))return json({message:'Enviado em RAM; nao salvo.'},202);
     profiles.set(profile.name,profile);return json(profile);
    }
    if(p==='/'||p==='/index.js'||p==='/index.css')return route.fulfill({contentType:p==='/'?'text/html':p.endsWith('.js')?'text/javascript':'text/css',body:asset(p==='/'?'index.html':p.slice(1))});
    if(p==='/theme.css')return route.fulfill({contentType:'text/css',body:''});
    if(p==='/systeminfo')return json({MALIOS_VERSION:'test',SD:{used:'0 B',total:'0 B'},LittleFS:{used:'1 KB',total:'1 MB'}});
    if(p==='/listfiles')return route.fulfill({contentType:'text/plain',body:''});
    return json({items:[],files:[],status:'OK',free:1,total:2});
   });
   await page.goto('http://keygauge.test/');
   await page.locator('[data-webui-target="keygauge"]').click();
   await page.waitForFunction(()=>!document.getElementById('kg-load').disabled);
   await page.selectOption('#kg-list','DEVICE_001');await page.click('#kg-load');
   await page.waitForFunction(()=>document.getElementById('kg-name').value==='DEVICE_001');
   assert.equal(await page.inputValue('#kg-level-2'),'5');
   // Scalar changes must not write, nor change point levels.
   await page.locator('#kg-thickness').fill('10');await page.locator('#kg-width').fill('50');
   assert.equal(await page.inputValue('#kg-level-2'),'5');assert.equal(writes.length,0);
   await page.selectOption('#kg-points','10');assert.equal(await page.locator('.kg-level').count(),10);
   await page.locator('#kg-width').fill('90');
   await page.locator('#kg-canvas').scrollIntoViewIfNeeded();
   const box=await page.locator('#kg-canvas').boundingBox();
   const span=(box.width-24)*.9,start=(box.width-span)/2+span/9;
   const x=box.x+start,y=box.y+32+(box.height-68)/2*2/9;
   const scrollBefore=await page.evaluate(()=>window.scrollY);
   if(mobile){
    const cdp=await context.newCDPSession(page);
    await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x,y}]});
    await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x,y:box.y+box.height+30}]});
    await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
   } else {
    await page.mouse.move(x,y);await page.mouse.down();await page.mouse.move(x,box.y+box.height+30,{steps:5});await page.mouse.up();
   }
   assert.equal(await page.inputValue('#kg-level-0'),'9','drag clamps outside canvas');assert.equal(writes.length,0,'drag never writes');
   if(mobile)assert.equal(await page.evaluate(()=>window.scrollY),scrollBefore,'touch drag does not scroll the page');
   // Cancelled touches release ownership; a new drag can reach the upper bound.
   const maxY=box.y+32+(box.height-68)/2;
   if(mobile){
    const cdp=await context.newCDPSession(page);
    await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x,y:maxY}]});
    await cdp.send('Input.dispatchTouchEvent',{type:'touchCancel',touchPoints:[]});
    await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x,y:maxY}]});
    await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x,y:box.y+1}]});
    await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
   }else{
    await page.mouse.move(x,maxY);await page.mouse.down();await page.mouse.move(x,box.y-15,{steps:5});await page.mouse.up();
   }
   assert.equal(await page.inputValue('#kg-level-0'),'0','upper drag bound');assert.equal(writes.length,0);
   await page.click('#kg-show');await page.waitForFunction(()=>!document.getElementById('kg-show').disabled);
   assert.equal(writes.at(-1).path,'/api/keygauge/preview');assert.deepEqual(profiles.get('DEVICE_001'),original);
   await page.click('#kg-duplicate');await page.waitForFunction(()=>document.getElementById('kg-name').value!=='DEVICE_001');
   const duplicate=await page.inputValue('#kg-name');assert.equal(profiles.size,1);
   await page.click('#kg-save');await page.waitForFunction(()=>document.getElementById('kg-state').textContent==='SALVO');
   assert.ok(profiles.has(duplicate));assert.deepEqual(profiles.get('DEVICE_001'),original);
   // Corrupt response must leave current profile untouched and display a clear error.
   broken=true;await page.selectOption('#kg-list','DEVICE_001');await page.click('#kg-load');
   await page.waitForFunction(()=>document.getElementById('kg-status').dataset.error==='true');
   assert.equal(await page.inputValue('#kg-name'),duplicate);broken=false;
   await page.selectOption('#kg-list',duplicate);await page.click('#kg-delete');
   await page.waitForFunction(()=>document.getElementById('kg-state').textContent==='NOVO · NAO SALVO');assert.equal(profiles.size,1);
   await page.locator('#kg-name').fill('../bad');const count=writes.length;await page.click('#kg-save');
   await page.waitForFunction(()=>document.getElementById('kg-status').dataset.error==='true');assert.equal(writes.length,count);
   await page.click('#kg-new');await page.waitForFunction(()=>!document.getElementById('kg-new').disabled);
   assert.equal(await page.inputValue('#kg-thickness'),'5');assert.equal(await page.inputValue('#kg-width'),'90');
   await page.locator('#kg-canvas').scrollIntoViewIfNeeded();
   await page.screenshot({path:`tests/key_gauge/${mobile?'mobile':'desktop'}.png`,fullPage:true});
   assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth),'no horizontal page overflow');
   assert.deepEqual(errors,[]);
   console.log(`${embedded?'embedded gzip':'source'} / ${mobile?'mobile touch':'desktop mouse'}: PASS (drag, bounds, no implicit writes, load/save/delete/duplicate/preview, invalid response/name)`);
   await context.close();
  }
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
