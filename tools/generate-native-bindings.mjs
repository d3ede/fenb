import fs from 'node:fs';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
// Private PE inputs remain local. Only normalized ABI probes and reference IDs are emitted.
const referenceFile=process.argv[2],candidateFile=process.argv[3];
if(!referenceFile||!candidateFile)throw Error('Usage: node tools/generate-native-bindings.mjs <reference PE copy> <candidate PE copy>');
fs.mkdirSync('build/native-binding-generator',{recursive:true});
const toolchain=fs.readdirSync('.tools').filter(n=>/^llvm-mingw-.*-ucrt-x86_64$/.test(n)).sort().at(-1);
const objdump=path.resolve('.tools',toolchain,'bin/llvm-objdump.exe');
function disassemble(file,name){const out='build/native-binding-generator/'+name+'.disasm',fd=fs.openSync(out,'w');try{execFileSync(objdump,['-d','--x86-asm-syntax=intel',file],{stdio:['ignore',fd,'pipe']});}finally{fs.closeSync(fd);}return fs.readFileSync(out,'utf8');}
function pe(file){
 const b=fs.readFileSync(file),nt=b.readUInt32LE(0x3c),opt=nt+24,base=b.readUInt32LE(opt+28),size=b.readUInt32LE(opt+56),sections=[];
 for(let i=0;i<b.readUInt16LE(nt+6);i++){const at=opt+b.readUInt16LE(nt+20)+i*40;sections.push({name:b.toString('ascii',at,at+8).replace(/\0/g,''),rva:b.readUInt32LE(at+12),size:b.readUInt32LE(at+8),raw:b.readUInt32LE(at+20),rawSize:b.readUInt32LE(at+16),flags:b.readUInt32LE(at+36)});}
 const section=rva=>sections.find(s=>rva>=s.rva&&rva<s.rva+s.size),offset=rva=>{const s=section(rva);return s&&rva-s.rva<s.rawSize?s.raw+rva-s.rva:null;};
 const reloc=[];let p=offset(b.readUInt32LE(opt+96+5*8)),end=p+b.readUInt32LE(opt+96+5*8+4);while(p<end){let page=b.readUInt32LE(p),n=b.readUInt32LE(p+4);if(n<8)break;for(let q=p+8;q<p+n;q+=2){const v=b.readUInt16LE(q);if((v>>>12)===3)reloc.push(page+(v&4095));}p+=n;}
 return{b,base,size,sections,section,offset,reloc,value(rva){const p=offset(rva);return p===null?0:b.readUInt32LE(p);}};
}
const old=pe(referenceFile),cur=pe(candidateFile);
const mask=new Uint8Array(old.size),xref=new Map();
for(const rva of old.reloc){mask.fill(1,rva,rva+4);const target=old.value(rva)-old.base;if(old.section(rva)?.flags&0x20000000){if(!xref.has(target))xref.set(target,[]);xref.get(target).push(rva);}}
const disasm=disassemble(referenceFile,'reference');const branches=[];
for(const m of disasm.matchAll(/^\s*([0-9a-f]+): ((?:[0-9a-f]{2} )+)\s+\t(?:call|j\w+)\s+0x([0-9a-f]+)/gm)){
 const address=parseInt(m[1],16)-old.base,bytes=m[2].trim().split(' ');if(bytes.length===5&&['e8','e9'].includes(bytes[0])){mask.fill(1,address+1,address+5);branches.push({site:address,operand:1,target:parseInt(m[3],16)-old.base});}else if(bytes.length===6&&bytes[0]==='0f'&&/^8/.test(bytes[1])){mask.fill(1,address+2,address+6);branches.push({site:address,operand:2,target:parseInt(m[3],16)-old.base});}
}
function pattern(start,n){const p=old.offset(start);if(p===null||old.offset(start+n-1)!==p+n-1)return null;return Array.from(old.b.subarray(p,p+n),(v,i)=>mask[start+i]?-1:v);}
function find(image,pat){let best=[],bestAt=0,run=[];for(let i=0;i<=pat.length;i++){if(i<pat.length&&pat[i]>=0)run.push(pat[i]);else {if(run.length>best.length){best=run;bestAt=i-run.length;}run=[];}}if(best.length<4)return[];const needle=Buffer.from(best),result=[];
 for(const s of image.sections.filter(s=>s.flags&0x20000000)){let p=s.raw-1;while((p=image.b.indexOf(needle,p+1))>=0&&p<s.raw+s.rawSize){const start=p-bestAt;if(start<s.raw||start+pat.length>s.raw+s.rawSize)continue;if(pat.every((v,i)=>v<0||v===image.b[start+i]))result.push(s.rva+start-s.raw);if(result.length>1)return result;}}
 return result;
}
function codeProbe(point,operand=null){for(const n of [128,96,64,48,32])for(const back of [16,8,0]){const start=point-back,p=pattern(start,n);if(!p)continue;const a=find(old,p);if(a.length!==1||a[0]!==start)continue;const b=find(cur,p);if(b.length!==1)continue;const value=operand===null?b[0]+back:cur.value(b[0]+back)-cur.base;
 return{rva:value,probe:{start,pattern:p.map(v=>v<0?'??':v.toString(16).padStart(2,'0')).join(' '),offset:back,follow:operand!==null}};}}
const roots=new Set();for(const f of ['native_animation.hpp','native_app.hpp','native_background.hpp','native_camera.hpp','native_layer_time.hpp','native_mover.hpp','native_mover_legacy.hpp','native_mover_services.hpp','native_sfx_follow.hpp','native_sfx_time.hpp','native_world.hpp']){for(const m of fs.readFileSync('src/'+f,'utf8').split('inline bool install()')[1].matchAll(/\b0x[0-9a-f]{5,}\b/gi)){const rva=Number(m[0]);if(old.section(rva))roots.add(rva);}}
// One existing world adapter owns this call's loop/reset semantics. All other
// native CSfxModel::Process calls share the model clock, including Render paths.
const sfxProcessCalls=branches.filter(b=>b.operand===1&&b.target===0x352380&&old.b[old.offset(b.site)]===0xe8&&b.site!==0x32d5fd).map(b=>b.site);
if(!sfxProcessCalls.length)throw Error('No SFX process callers found');
for(const site of sfxProcessCalls)roots.add(site);
const sfx=new Map();
for(const m of fs.readFileSync('src/native_sfx_follow.hpp','utf8').matchAll(/{"([^"\n]+)",(0x[0-9a-f]+),(0x[0-9a-f]+),(0x[0-9a-f]+)/g)) {
 const [,name,table,type,process]=m;const nameBytes=Buffer.from(name+'\0'),pos=cur.b.indexOf(nameBytes);if(pos<0)throw Error('Missing RTTI '+name);
 const cs=cur.sections.find(s=>pos>=s.raw&&pos<s.raw+s.rawSize),typeRva=cs.rva+pos-cs.raw-8;
 const locators=cur.reloc.filter(r=>cur.value(r)===cur.base+typeRva).map(r=>r-12).filter(r=>cur.section(r)?.name==='.rdata'&&cur.value(r)===0&&cur.value(r+4)===0&&cur.value(r+8)===0);
 const tables=cur.reloc.filter(r=>locators.includes(cur.value(r)-cur.base)).map(r=>r+4).filter(r=>cur.section(cur.value(r+24)-cur.base)?.flags&0x20000000);
 if(tables.length!==1)throw Error('Ambiguous RTTI table '+name+' '+tables.length);
 const tr=Number(table),pr=Number(process);roots.add(tr);roots.add(Number(type));roots.add(pr);sfx.set(tr,{rva:tables[0],probe:{tableType:Number(type)}});
 const target=cur.value(tables[0]+24)-cur.base;if(sfx.has(pr)&&sfx.get(pr).rva!==target)throw Error('Shared process mismatch');
 sfx.set(pr,{rva:target,probe:{methodTable:tr,slot:6}});
}
const records=[],unresolved=[];
function extent(image,ref) {
 const p=image.offset(ref);if(p===null)return 0;
 for(let i=1;i<65536&&p+i+1<image.b.length;i++)if(image.b[p+i]===0xcc&&image.b[p+i+1]===0xcc) {
   let end=i+2;while(image.b[p+end]===0xcc)end++;
   if((ref+end)%16===0)return i;
 }
 return 0;
}
for(const ref of [...roots].sort((a,b)=>a-b)){
 let item=sfx.get(ref);const s=old.section(ref),p=old.offset(ref);
 if(!item&&(s.flags&0x20000000))item=codeProbe(ref);
 else if(p!==null&&old.b.toString('ascii',p+8,p+12)==='.?AV'){
   const end=old.b.indexOf(0,p+8),name=old.b.subarray(p+8,end+1),pos=cur.b.indexOf(name);if(pos>=0&&cur.b.indexOf(name,pos+1)<0){const cs=cur.sections.find(s=>pos>=s.raw&&pos<s.raw+s.rawSize);item={rva:cs.rva+pos-cs.raw-8,probe:{name:name.subarray(0,-1).toString('ascii')}};}
 } else if(p!==null&&old.b.toString('ascii',p,p+4)==='.?AV'){
   const end=old.b.indexOf(0,p),name=old.b.subarray(p,end+1),pos=cur.b.indexOf(name);if(pos>=0&&cur.b.indexOf(name,pos+1)<0){const cs=cur.sections.find(s=>pos>=s.raw&&pos<s.raw+s.rawSize);item={rva:cs.rva+pos-cs.raw,probe:{name:name.subarray(0,-1).toString('ascii'),nameOffset:0}};}
 }
 if(!item)for(const at of xref.get(ref)||[]){item=codeProbe(at,true);if(item)break;}
 if(!item&&sfxProcessCalls.includes(ref)) {
   const owner=[...sfx.entries()].filter(([at,v])=>v.probe.methodTable&&at<=ref).sort((a,b)=>b[0]-a[0])[0];
   if(owner&&ref+5<=owner[0]+extent(old,owner[0])&&ref-owner[0]+5<=extent(cur,owner[1].rva)) {
     const p=pattern(ref,64),mapped=owner[1].rva+ref-owner[0],cp=cur.offset(mapped);
     if(p&&cp!==null&&p.every((v,i)=>v<0||cur.b[cp+i]===v))item={rva:mapped,probe:{within:owner[0],offset:ref-owner[0],start:ref,pattern:p.map(v=>v<0?'??':v.toString(16).padStart(2,'0')).join(' ')}};
   }
 }
 if(!item)unresolved.push('0x'+ref.toString(16));else records.push({reference:ref,current:item.rva,...item.probe});
}

if(unresolved.length)throw Error('Unresolved native references: '+unresolved.join(', '));
const indices=new Map(records.map((r,i)=>[r.reference,i]));
const specs=[],checks=[],fixups=[];
for(const r of records){
 let kind='Code',source=0,offset=r.offset||0,probe=r.pattern||r.name||'';
 if(r.name){kind='Name';offset=r.nameOffset??8;}
 else if(r.tableType){kind='Table';source=indices.get(r.tableType);offset=0;}
 else if(r.methodTable){kind='Method';source=indices.get(r.methodTable);offset=r.slot*4;probe=pattern(r.reference,128).map(v=>v<0?'??':v.toString(16).padStart(2,'0')).join(' ');
   const cp=cur.offset(r.current),tokens=probe.split(' ');if(!tokens.every((v,i)=>v==='??'||parseInt(v,16)===cur.b[cp+i]))throw Error('Changed SFX method ABI: '+r.reference.toString(16));}
 else if(r.within){kind='Offset';source=indices.get(r.within);}
 else if(r.follow)kind='Operand';
 specs.push({reference:r.reference,kind,source,offset,probe,access:(old.section(r.reference).flags&0xe0000000)>>>0});
 const start=r.pattern?r.start:r.methodTable?r.reference:null;
 const length=r.pattern?r.pattern.split(' ').length:r.methodTable?128:0;
 if(start!==null){
   for(const at of old.reloc)if(at>=start&&at+4<=start+length&&indices.has(old.value(at)-old.base))checks.push({source:specs.length-1,offset:at-start,target:indices.get(old.value(at)-old.base),relative:false});
   for(const b of branches)if(b.site+b.operand>=start&&b.site+b.operand+4<=start+length&&indices.has(b.target))checks.push({source:specs.length-1,offset:b.site+b.operand-start,target:indices.get(b.target),relative:true});
 }
}
for(const b of branches)if(b.operand===1&&old.b[old.offset(b.site)]===0xe8&&indices.has(b.target)&&records.some(r=>b.site>=r.reference&&b.site<r.reference+16))fixups.push({site:b.site,target:indices.get(b.target)});
const hex=n=>'0x'+n.toString(16),str=JSON.stringify;
const output=['// Generated by tools/generate-native-bindings.mjs from local disposable PE copies.','#pragma once','#include <cstdint>','namespace native_binding::catalog {','enum class Kind { Code, Operand, Name, Table, Method, Offset };','struct Spec { uint32_t reference;Kind kind;unsigned source,offset;const char *probe;uint32_t access; };','inline constexpr Spec specs[]={',...specs.map(r=>'    {'+hex(r.reference)+',Kind::'+r.kind+','+r.source+','+r.offset+','+str(r.probe)+','+hex(r.access)+'u},'),'};','struct Check { unsigned source,offset,target;bool relative; };','inline constexpr Check checks[]={',...checks.map(r=>'    {'+r.source+','+r.offset+','+r.target+','+r.relative+'},'),'};','struct Call { uint32_t site;unsigned target; };','inline constexpr Call calls[]={',...fixups.map(r=>'    {'+hex(r.site)+','+r.target+'},'),'};','}',''].join('\n');
fs.writeFileSync('src/generated/native_bindings.hpp',output.replace('\n}\n','\ninline constexpr uint32_t sfxProcessCalls[]={'+sfxProcessCalls.map(hex).join(',')+'};\n}\n'));
fs.writeFileSync('build/native-binding-generator/audit.json',JSON.stringify({records,specs:specs.length,checks:checks.length,calls:fixups.length},null,2));
console.log(JSON.stringify({specs:specs.length,checks:checks.length,calls:fixups.length,output:'src/generated/native_bindings.hpp'}));
