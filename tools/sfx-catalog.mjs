// Exact material identities only; no client textures or geometry are embedded.
import fs from 'node:fs';import crypto from 'node:crypto';import path from 'node:path';
const root=process.argv[2];if(!root)throw Error('Usage: node tools/sfx-catalog.mjs <client directory>');
const identities=new Map();
for(const item of JSON.parse(fs.readFileSync(new URL('./sfx-materials.json',import.meta.url),'utf8'))) {
 const weapon=/^SFX\/Texture\/(?:fly_weaponfx_|sfx_item)/i.test(item.file);
 const kind=item.kind===6&&weapon?8:item.kind;
 const b=fs.readFileSync(path.join(root,item.file));
 if(crypto.createHash('sha256').update(b).digest('hex')!==item.rawHash)throw Error('Changed source asset: '+item.file);
 identities.set(item.sha256,kind);
 if(b.toString('ascii',0,4)==='DDS '){
  const w=b.readUInt32LE(16),h=b.readUInt32LE(12),four=b.toString('ascii',84,88);
  const count=four==='DXT1'?Math.ceil(w/4)*Math.ceil(h/4)*8:/DXT[35]/.test(four)?Math.ceil(w/4)*Math.ceil(h/4)*16:w*h*b.readUInt32LE(88)/8;
  if(count>0&&128+count<=b.length)identities.set(crypto.createHash('sha256').update(b.subarray(128,128+count)).digest('hex'),kind);
 }
}
let out='#pragma once\nstruct SfxIdentity { unsigned char hash[32]; unsigned kind; };\ninline constexpr SfxIdentity SfxIdentities[] = {\n';
for(const [hash,kind] of [...identities].sort((a,b)=>a[0].localeCompare(b[0])))
 out+='{ {'+hash.match(/../g).map(x=>'0x'+x).join(',')+'}, '+kind+' },\n';
fs.writeFileSync('src/generated/sfx_catalog.hpp',out+'};\n');
console.log('Generated '+identities.size+' SFX upload identities');
