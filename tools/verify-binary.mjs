import fs from 'node:fs';
function pe(path) {
 const b=fs.readFileSync(path), u16=p=>b.readUInt16LE(p),u32=p=>b.readUInt32LE(p),offset=u32(60),opt=offset+24;
 const sections=Array.from({length:u16(offset+6)},(_,i)=>{let p=opt+u16(offset+20)+40*i;return{va:u32(p+12),size:Math.max(u32(p+8),u32(p+16)),raw:u32(p+20)};});
 const rva=v=>{let s=sections.find(s=>v>=s.va&&v<s.va+s.size);return s?v-s.va+s.raw:v;};
 const str=p=>b.toString('ascii',p,b.indexOf(0,p));
 const dir=opt+(u16(opt)===0x20b?112:96),d=rva(u32(dir)),base=u32(d+16),n=u32(d+20),named=u32(d+24),fn=rva(u32(d+28)),nm=rva(u32(d+32)),ord=rva(u32(d+36));
 const map=new Map();for(let i=0;i<named;i++)map.set(u16(ord+i*2),str(rva(u32(nm+i*4))));
 const exports=[];for(let i=0;i<n;i++)if(u32(fn+i*4))exports.push({name:map.get(i)||null,ordinal:base+i});
 const imports=[];let p=rva(u32(dir+8));if(p)while(u32(p+12)){imports.push(str(rva(u32(p+12))));p+=20;}
 return{machine:u16(offset+4),bytes:b.length,exports,imports};
}
const binary=pe(process.argv[2] ?? 'dist/d3d9.dll'),system=pe('C:/Windows/SysWOW64/d3d9.dll');
if(binary.machine!==0x14c)throw Error('DLL is not x86');
if(JSON.stringify(binary.exports)!==JSON.stringify(system.exports))throw Error('Export names / ordinals differ from system d3d9.dll');
if(binary.imports.some(x=>/libc\+\+|libwinpthread|libgcc|msvcp|vcruntime|d3dcompiler|mscoree/i.test(x)))throw Error('Unexpected runtime dependency');
console.log(JSON.stringify({status:'PASS',architecture:'x86',bytes:binary.bytes,exports:binary.exports.length,imports:binary.imports},null,2));
