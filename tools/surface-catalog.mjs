// Exact material identities only. No game artwork is embedded.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
const root=process.argv[2];
if(!root)throw Error('Usage: node tools/surface-catalog.mjs <client directory>');
const identities=new Map();
for(const item of JSON.parse(fs.readFileSync(new URL('./surface-materials.json',import.meta.url),'utf8'))){
 const data=fs.readFileSync(path.join(root,item.file));
 if(crypto.createHash('sha256').update(data).digest('hex')!==item.sourceHash)
  throw Error('Changed surface asset: '+item.file);
 const w=data.readUInt32LE(16),h=data.readUInt32LE(12),four=data.toString('ascii',84,88);
 const size=four==='DXT1'?Math.ceil(w/4)*Math.ceil(h/4)*8:/DXT[35]/.test(four)?Math.ceil(w/4)*Math.ceil(h/4)*16:w*h*data.readUInt32LE(88)/8;
 identities.set(crypto.createHash('sha256').update(data.subarray(128,128+size)).digest('hex'),item.kind);
 identities.set(item.sha256,item.kind);
}
let out='#pragma once\nstruct SurfaceIdentity { unsigned char hash[32]; unsigned kind; };\ninline constexpr SurfaceIdentity SurfaceIdentities[] = {\n';
for(const [hash,kind] of identities)
 out+='{ {'+hash.match(/../g).map(v=>'0x'+v).join(',')+'}, '+kind+' },\n';
fs.writeFileSync('src/generated/surface_catalog.hpp',out+'};\n');
console.log('Generated '+identities.size+' surface identities');
