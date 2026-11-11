import fs from 'node:fs';
const b=fs.readFileSync(process.argv[2] || 'C:/Windows/SysWOW64/d3d9.dll');
const u16=p=>b.readUInt16LE(p),u32=p=>b.readUInt32LE(p),pe=u32(60),opt=pe+24;
const sections=Array.from({length:u16(pe+6)},(_,i)=>{
 const p=opt+u16(pe+20)+40*i;return {va:u32(p+12),size:Math.max(u32(p+8),u32(p+16)),raw:u32(p+20)};
});
const rva=v=>{const s=sections.find(s=>v>=s.va&&v<s.va+s.size);return s?v-s.va+s.raw:v;};
const str=p=>b.toString('ascii',p,b.indexOf(0,p));
const d=rva(u32(opt+(u16(opt)===0x20b?112:96))),base=u32(d+16),count=u32(d+20),names=u32(d+24);
const fn=rva(u32(d+28)),nm=rva(u32(d+32)),ord=rva(u32(d+36)),map=new Map();
for(let i=0;i<names;i++)map.set(u16(ord+i*2),str(rva(u32(nm+i*4))));
const exports=[];
for(let i=0;i<count;i++)if(u32(fn+i*4))exports.push({name:map.get(i)||null,ordinal:base+i});
fs.writeFileSync('tools/system-exports.json',JSON.stringify(exports,null,2)+'\n');
console.log(`Captured ${exports.length} exports (${exports.filter(x=>!x.name).length} ordinal-only)`);
