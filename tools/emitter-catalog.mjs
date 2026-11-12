// Exact first-mip identity of emissive world materials. Never classify HUD or arbitrary bright objects.
import fs from 'node:fs';import path from 'node:path';import crypto from 'node:crypto';
const root=process.argv[2];if(!root)throw Error('Usage: node tools/emitter-catalog.mjs <client directory>');
const files=['Model/Texture/Obj_MaFlRe01-light.dds','Model/Texture/Obj_MaFlHo01-light.dds','Model/Texture/Obj_MaFlCi001-light.dds',
 'Model/Texture/Obj_MaFlFo001-light.dds','SFX/Texture/sfx_genlight01-1.dds','SFX/Texture/sfx_genlodelight01-1.dds',
 'SFX/Texture/sfx_light01.dds','SFX/Texture/sfx_light02.dds','SFX/Texture/obj_magiclight.dds'];
let out='#pragma once\ninline constexpr unsigned char EmitterHashes[][32] = {\n';
const hashes=new Set(),rawHashes={};
for(const file of files){const b=fs.readFileSync(path.join(root,file));const w=b.readUInt32LE(16),h=b.readUInt32LE(12),four=b.toString('ascii',84,88);
 const size=four==='DXT1'?Math.ceil(w/4)*Math.ceil(h/4)*8:/DXT[35]/.test(four)?Math.ceil(w/4)*Math.ceil(h/4)*16:w*h*b.readUInt32LE(88)/8;
 const hash=crypto.createHash('sha256').update(b.subarray(128,128+size)).digest('hex');rawHashes[file]=hash;if(hashes.has(hash))continue;hashes.add(hash);
 out+='// '+file+'\n{'+hash.match(/../g).map(x=>'0x'+x).join(',')+'},\n';
}
for(const item of JSON.parse(fs.readFileSync(new URL('./emitter-decoded.json',import.meta.url),'utf8'))){
 if(rawHashes[item.file]!==item.rawHash||hashes.has(item.sha256))continue;
 hashes.add(item.sha256);out+='// '+item.file+' (D3DX ARGB8 upload)\n{'+item.sha256.match(/../g).map(x=>'0x'+x).join(',')+'},\n';
}
fs.writeFileSync('src/generated/emitter_catalog.hpp',out+'};\n');console.log('Generated '+hashes.size+' exact emissive-material identities');
out='#pragma once\ninline constexpr unsigned char FoliageHashes[][32] = {\n';const seen=new Set();
for(const file of fs.readdirSync(path.join(root,'Model/Texture')).sort().filter(n=>/^obj_macoprtr\d+-\d+\.dds$/i.test(n))){
 const b=fs.readFileSync(path.join(root,'Model/Texture',file));const w=b.readUInt32LE(16),h=b.readUInt32LE(12),four=b.toString('ascii',84,88);
 const size=four==='DXT1'?Math.ceil(w/4)*Math.ceil(h/4)*8:/DXT[35]/.test(four)?Math.ceil(w/4)*Math.ceil(h/4)*16:w*h*b.readUInt32LE(88)/8;
 const hash=crypto.createHash('sha256').update(b.subarray(128,128+size)).digest('hex');if(seen.has(hash))continue;seen.add(hash);
 out+='// '+file+'\n{'+hash.match(/../g).map(x=>'0x'+x).join(',')+'},\n';
}
fs.writeFileSync('src/generated/foliage_catalog.hpp',out+'};\n');console.log('Generated '+seen.size+' common-tree material identities');
out='#pragma once\ninline constexpr unsigned char OverlayHashes[][32] = {\n';
for(const file of ['Model/Texture/ARROW1.dds','Model/Texture/etc_arrow-01.dds']){
 const b=fs.readFileSync(path.join(root,file)),w=b.readUInt32LE(16),h=b.readUInt32LE(12);
 if(b.toString('ascii',84,88)!=='DXT1')throw Error('Unexpected native marker format: '+file);
 const hash=crypto.createHash('sha256').update(b.subarray(128,128+Math.ceil(w/4)*Math.ceil(h/4)*8)).digest('hex');
 out+='// '+file+' (native 3D UI marker)\n{'+hash.match(/../g).map(x=>'0x'+x).join(',')+'},\n';
}
fs.writeFileSync('src/generated/overlay_catalog.hpp',out+'};\n');
