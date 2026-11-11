// Only texture families positively identified in the Neuz capture. No fuzzy material matching.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
const directory=process.argv[2]||'X:/Games/Homesickness/World/Texture';
const entries=[];
for(const name of fs.readdirSync(directory).sort()){
  if(!/^water_(HarAni|riverAni)\d+\.dds$/i.test(name))continue;
  const b=fs.readFileSync(path.join(directory,name));
  if(b.toString('ascii',0,4)!=='DDS '||b.readUInt32LE(12)!==128||b.readUInt32LE(16)!==128)throw Error(`Unexpected texture: ${name}`);
  const dxt1=b.toString('ascii',84,88)==='DXT1';
  if(!dxt1&&(b.readUInt32LE(84)!==0||b.readUInt32LE(88)!==16||b.readUInt32LE(92)!==0x7c00||b.readUInt32LE(96)!==0x3e0||b.readUInt32LE(100)!==0x1f||b.readUInt32LE(104)!==0x8000))throw Error(`Unsupported format: ${name}`);
  const bytes=dxt1?8192:32768;
  if(b.length<128+bytes)throw Error(`Incomplete texture: ${name}`);
  entries.push(crypto.createHash('sha256').update(b.subarray(128,128+bytes)).digest('hex'));
}
const hashes=[...new Set(entries)];
if(!hashes.length)throw Error('No verified water texture families found');
fs.writeFileSync('src/generated/water_catalog.hpp','#pragma once\ninline constexpr unsigned char WaterHashes[][32] = {\n'+hashes.map(h=>'{'+h.match(/../g).map(x=>'0x'+x).join(',')+'}').join(',\n')+'\n};\n');
console.log(`WaterFX: ${hashes.length} exact texture fingerprints`);
