// Read-only input; write reports only to an explicitly selected output directory.
import fs from 'node:fs';
import path from 'node:path';
const [input,output]=process.argv.slice(2);
if(!input||!output)throw Error('Usage: node tools/inspect-grass-capture.mjs <capture-directory> <output-directory>');
const source=path.resolve(input),destination=path.resolve(output);
if(source===destination)throw Error('Choose a separate output directory');
const blob=name=>{
 if(path.basename(name)!==name)throw Error('Invalid capture filename');
 return fs.readFileSync(path.join(source,name));
};
const records=fs.readFileSync(path.join(source,'frame.jsonl'),'utf8').trim().split(/\r?\n/).map(JSON.parse);
if(records.find(x=>x.kind==='header')?.mode!=='grass')throw Error('Not a grass capture');
fs.mkdirSync(destination,{recursive:true});
const catalog=new Map();
for(const item of JSON.parse(fs.readFileSync(new URL('./grass-materials.json',import.meta.url))))
 for(const hash of [item.uploadHash,item.decodedHash])if(hash)catalog.set(hash,item);
const textures=new Map(records.filter(x=>x.kind==='texture').map(t=>[t.id,{...t,material:catalog.get(t.firstMipSHA256)}]));
const draws=records.filter(x=>x.kind==='draw'),groups=new Map();
for(const d of draws){
 const t=textures.get(d.textures[0]),key=JSON.stringify([d.fvf,d.vs,d.ps,t?.firstMipSHA256,d.states?.blend?.[1],d.stages]);
 const group=groups.get(key)||{draws:0,fvf:d.fvf,vs:d.vs,ps:d.ps,texture:t?.id,
  asset:t?.material?.file??null,expectedKind:t?.material?.kind??null,cachedKind:t?.grassKind,
  stages:d.stages,alphaArgs:d.alphaArgs,colorWrites:d.colorWrites,first:d.index};
 group.draws++;groups.set(key,group);
}
function bitmap(name,width,height,pixels){
 if(!/^[a-z0-9_.-]+$/i.test(name))throw Error('Invalid output filename');
 if(pixels.length!==width*height*4)throw Error('Unexpected image size: '+name);
 const header=Buffer.alloc(54);header.write('BM');header.writeUInt32LE(54+pixels.length,2);header.writeUInt32LE(54,10);
 header.writeUInt32LE(40,14);header.writeInt32LE(width,18);header.writeInt32LE(-height,22);
 header.writeUInt16LE(1,26);header.writeUInt16LE(32,28);
 fs.writeFileSync(path.join(destination,name+'.bmp'),Buffer.concat([header,pixels]));
}
const frames=records.filter(x=>x.kind==='grass-frame');
for(const frame of frames){
 frame.started=!!(frame.status&1);frame.failed=!!(frame.status&2);frame.winter=!!(frame.status&4);
 frame.playerCentered=!!(frame.status&8);frame.enabled=!!(frame.status&16);
 const image=records.find(x=>x.kind==='grass-image'&&x.file===frame.prefix+'-ground.rgba32f');
 if(image){
  const raw=blob(image.file);
  if(raw.length!==image.width*image.height*16||raw.length>64*1024*1024)throw Error('Invalid ground map size');
  const pixels=Buffer.alloc(image.width*image.height*4);let valid=0,covered=0,sum=0;
  for(let i=0;i<pixels.length/4;i++){
   const value=raw.readFloatLE(i*16+4),c=Number.isFinite(value)?Math.max(0,Math.min(1,value)):0;
   if(Number.isFinite(value))valid++;if(c>.02)covered++;sum+=c;
   pixels[i*4]=0;pixels[i*4+1]=Math.round(c*255);pixels[i*4+2]=0;pixels[i*4+3]=255;
  }
  frame.coverage={valid,coveredFraction:covered/(pixels.length/4),mean:sum/(pixels.length/4)};
  bitmap(frame.prefix+'-coverage',image.width,image.height,pixels);
 }
}
for(const image of records.filter(x=>x.kind==='grass-image'&&x.pixelBytes===4&&x.file.endsWith('.argb')))
 bitmap(image.file.replace('.argb',''),image.width,image.height,blob(image.file));
const report={source,footer:records.find(x=>x.kind==='footer'),frames,drawGroups:[...groups.values()].sort((a,b)=>b.draws-a.draws),textures:[...textures.values()]};
fs.writeFileSync(path.join(destination,'report.json'),JSON.stringify(report,null,2));
console.log(JSON.stringify({frames:frames.map(({frame,status,coverage})=>({frame,status,coverage})),draws:draws.length,output:destination},null,2));
