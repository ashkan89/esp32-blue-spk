// Local-only dashboard preview; firmware APIs are deliberately not emulated.
const http=require('node:http'),fs=require('node:fs'),path=require('node:path');
const root=path.resolve(__dirname,'../web');
http.createServer((req,res)=>{
  if(req.url==='/favicon.svg'){res.setHeader('Content-Type','image/svg+xml');res.end(fs.readFileSync(path.join(root,'favicon.svg')));return}
  if(req.url!=='/'){res.writeHead(404);res.end('{}');return}
  let html=fs.readFileSync(path.join(root,'index.html'),'utf8');
  html=html.replace(/\/\* @include ([\w.-]+) \*\//g,(_,file)=>fs.readFileSync(path.join(root,file),'utf8'));
  res.setHeader('Content-Type','text/html; charset=utf-8');res.end(html);
}).listen(8765,'127.0.0.1',()=>process.stdout.write('Preview on http://127.0.0.1:8765\n'));
