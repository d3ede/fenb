import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
const root=process.argv[2]||'X:/Games/Homesickness';
const records=[];
function dds(file){
 const b=fs.readFileSync(file);if(b.length<128||b.toString('ascii',0,4)!=='DDS ')return null;
 const u=p=>b.readUInt32LE(p),w=u(16),h=u(12),four=u(84),bits=u(88),r=u(92),g=u(96),blue=u(100),a=u(104);
 let format=four,row=0,rows=h;
 const tag=b.toString('ascii',84,88);
 if(tag==='DXT1'){row=Math.ceil(w/4)*8;rows=Math.ceil(h/4);}
 else if(tag==='DXT3'||tag==='DXT5'){row=Math.ceil(w/4)*16;rows=Math.ceil(h/4);}
 else if(!four){
   row=Math.ceil(w*bits/8);
   if(bits===32&&r===0xff0000&&g===0xff00&&blue===0xff)format=a?21:22;
   else if(bits===16&&r===0xf800&&g===0x7e0&&blue===0x1f)format=23;
   else if(bits===16&&r===0x7c00&&g===0x3e0&&blue===0x1f)format=25;
   else if(bits===16&&r===0xf00&&g===0xf0&&blue===0xf)format=26;
 }
 const payload=b.subarray(128,128+row*rows);
 return{width:w,height:h,format,fourCC:tag.replace(/\0/g,''),mips:u(28),firstMipBytes:payload.length,
   firstMipSHA256:row&&payload.length===row*rows?crypto.createHash('sha256').update(payload).digest('hex'):null};
}
function tga(file){
 const b=fs.readFileSync(file);if(b.length<18||b[1]!==0||b[2]!==2)return null;
 const w=b.readUInt16LE(12),h=b.readUInt16LE(14),bits=b[16],offset=18+b[0];
 if(![24,32].includes(bits)||b.length<offset+w*h*bits/8)return null;
 const pixels=Buffer.alloc(w*h*4);for(let y=0;y<h;y++)for(let x=0;x<w;x++){
   let sy=(b[17]&32)?y:h-1-y,sx=(b[17]&16)?w-1-x:x;
   const src=offset+(sy*w+sx)*(bits/8),dst=(y*w+x)*4;
   pixels[dst]=b[src];pixels[dst+1]=b[src+1];pixels[dst+2]=b[src+2];pixels[dst+3]=bits===32?b[src+3]:255;
 }
 const keyed=Buffer.from(pixels);for(let i=0;i<keyed.length;i+=4)if(!keyed[i]&&!keyed[i+1]&&!keyed[i+2])keyed[i+3]=0;
 return{width:w,height:h,format:21,decoded:'TGA to BGRA8',firstMipBytes:pixels.length,firstMipSHA256:crypto.createHash('sha256').update(pixels).digest('hex'),hashVariants:[{name:'black color key',sha256:crypto.createHash('sha256').update(keyed).digest('hex')}]};
}
for(const dir of ['Weather','World/Texture']){
 const absolute=path.join(root,dir);if(!fs.existsSync(absolute))continue;
 for(const name of fs.readdirSync(absolute)){
   if(dir==='World/Texture'&&!/water|sky|cloud|sun|shadow/i.test(name))continue;
   const file=path.join(absolute,name);if(!fs.statSync(file).isFile())continue;
   const ext=path.extname(name).toLowerCase();const info=ext==='.dds'?dds(file):ext==='.tga'?tga(file):null;
   records.push({path:dir+'/'+name,bytes:fs.statSync(file).size,dds:info});
 }
}
const executable=fs.readFileSync(path.join(root,'Neuz.exe'));
const strings=[...executable.toString('latin1').matchAll(/[\x20-\x7e]{5,}/g)].map(m=>m[0]);
const hints=[...new Set(strings.filter(s=>s.length<220&&/water|skybox|shadow|bump|reflect|specular|\bfog\b|shader|weather|\.fx\b|\.vsh\b|\.psh\b/i.test(s)))];
const report={client:root,exeSHA256:crypto.createHash('sha256').update(executable).digest('hex'),assets:records,executableStringHints:hints};
fs.mkdirSync('artifacts/pipeline',{recursive:true});
fs.writeFileSync('artifacts/pipeline/client-inventory.json',JSON.stringify(report,null,2));
console.log(JSON.stringify({assets:records.length,dds:records.filter(r=>r.dds).length,water:records.filter(r=>/\/water/i.test(r.path)).length,executableStringHints:hints},null,2));
