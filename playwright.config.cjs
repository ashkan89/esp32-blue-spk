const {defineConfig}=require('@playwright/test');
module.exports=defineConfig({
  testDir:'tests/browser',workers:1,timeout:30000,
  use:{baseURL:'http://127.0.0.1:8765',headless:true},
  webServer:{command:'node scripts/preview_web.cjs',url:'http://127.0.0.1:8765',reuseExistingServer:false},
  reporter:'list'
});
