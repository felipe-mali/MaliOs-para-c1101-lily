const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const fs=require('fs'),path=require('path'),assert=require('node:assert/strict'),zlib=require('zlib');
const embedded=process.env.MALI_EMBEDDED==='1';
function asset(name){if(!embedded)return fs.readFileSync(path.join('embedded_resources/web_interface',name));const header=fs.readFileSync('include/webFiles.h','utf8');const m=header.match(new RegExp(`const uint8_t ${name.replace('.','_')}\\[\\] PROGMEM = \\{([\\s\\S]*?)\\};`));assert.ok(m);return zlib.gunzipSync(Buffer.from(m[1].match(/0x[\da-f]+/gi).map(v=>parseInt(v,16))));}
(async()=>{const browser=await chromium.launch({headless:true});try{
 for(const width of [1440,390]){
  const page=await browser.newPage({viewport:{width,height:950},isMobile:width<760,hasTouch:width<760}),errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.route('http://mali.test/**',route=>{const u=new URL(route.request().url()),p=u.pathname;
   const json=x=>route.fulfill({contentType:'application/json',body:JSON.stringify(x)});
   if(['/','/index.js','/index.css','/login'].includes(p)){const name=p==='/'?'index.html':p==='/login'?'login.html':p.slice(1);return route.fulfill({contentType:name.endsWith('html')?'text/html':name.endsWith('js')?'text/javascript':'text/css',body:asset(name)});}
   if(p==='/theme.css')return route.fulfill({contentType:'text/css',body:''});
   if(p==='/listfiles')return route.fulfill({contentType:'text/plain',body:''});
   if(p==='/systeminfo')return json({MALIOS_VERSION:'1.0',BRUCE_VERSION:'base',SD:{used:'0 B',total:'16 GB'},LittleFS:{used:'1 MB',total:'2 MB'}});
   return json({items:[],model:'LILYGO T-Embed',version:'1.0',heap:165432,uptime:'00:18:42',firmware:'MaliOS',maliVersion:'1.0',uptimeSeconds:1122,memory:{heapFree:165432},storage:{sd:{mounted:true}},status:'READY'});
  });
  await page.goto('http://mali.test/');await page.waitForTimeout(400);
  assert.equal(await page.locator('[data-webui-target]').count(),11);
  assert.equal(await page.locator('.mali-sidebar .active').textContent(),'Painel');
  assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'home fits viewport');
  const sidebar=await page.locator('.mali-sidebar').boundingBox(),home=await page.locator('.home-view').boundingBox();
  if(width>760)assert.ok(home.x>=sidebar.x+sidebar.width,'desktop content next to sidebar');else assert.ok(home.y>sidebar.y,'mobile nav above content');
  await page.screenshot({path:`tests/mali_ui/home-${width}.png`,fullPage:true});
  for(const view of ['tools','radio','files','system','home']){await page.locator(`[data-webui-target=${view}]`).click();await page.waitForTimeout(80);assert.ok(await page.locator(`.${view}-view`).first().isVisible());assert.equal(await page.locator('.mali-sidebar .active').count(),1);assert.equal(await page.locator(`[data-webui-target=${view}]`).getAttribute('aria-current'),'page');}
  await page.locator('.mali-selector [data-open-view=tools]').click();assert.equal(await page.locator('.tools-view').isVisible(),true);
  await page.locator('#mali-tools-help > summary').click();
  for(const topic of await page.locator('#mali-tools-help > details > summary').all()) {
   await topic.click();
   assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'help topic fits viewport');
  }
  assert.match(await page.locator('#mali-tools-help').innerText(),/MaliKeys/);
  assert.match(await page.locator('#mali-tools-help').innerText(),/A \u2212 B/);
  assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'expanded help fits viewport');
  await page.screenshot({path:`tests/mali_keys/help-${width}.png`,fullPage:true});
  await page.emulateMedia({reducedMotion:'reduce'});assert.equal(await page.locator('.mali-orbit').evaluate(e=>getComputedStyle(e).transitionDuration),'0s');
  await page.goto('http://mali.test/login');assert.equal(await page.locator('#password').getAttribute('autocomplete'),'current-password');assert.equal(await page.locator('form').getAttribute('action'),'/login');
  await page.screenshot({path:`tests/mali_ui/login-${width}.png`,fullPage:true});assert.deepEqual(errors,[]);
  console.log(`${embedded?'embedded':'source'} ${width}px PASS: sidebar/mobile layout, navigation, direct shortcuts, reduced motion, login, no overflow`);await page.close();
 }
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exitCode=1});
