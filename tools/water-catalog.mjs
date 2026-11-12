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
const weather=process.argv[3]||'X:/Games/Homesickness/Weather';
function weatherHashes(pattern){
  const result=[];
  for(const name of fs.readdirSync(weather).sort()){
    if(!pattern.test(name))continue;
    const b=fs.readFileSync(path.join(weather,name));
    const format=b.toString('ascii',84,88);
    if(b.toString('ascii',0,4)!=='DDS '||!/^DXT[135]$/.test(format))throw Error(`Unsupported sky texture: ${name}`);
    const size=Math.max(1,Math.ceil(b.readUInt32LE(12)/4))*Math.max(1,Math.ceil(b.readUInt32LE(16)/4))*(format==='DXT1'?8:16);
    if(b.length<128+size)throw Error(`Incomplete sky texture: ${name}`);
    result.push(crypto.createHash('sha256').update(b.subarray(128,128+size)).digest('hex'));
  }
  if(!result.length)throw Error('Verified sky/cloud texture family missing');
  return [...new Set(result)];
}
const arrays={WaterHashes:hashes,SkyHashes:weatherHashes(/^skybox\d+(?:_low)?\.dds$/i),CloudHashes:weatherHashes(/^(?:cloud\d+|rartesia_cloud)(?:_low)?\.dds$/i)};
fs.writeFileSync('src/generated/water_catalog.hpp','#pragma once\n'+Object.entries(arrays).map(([name,values])=>'inline constexpr unsigned char '+name+'[][32] = {\n'+values.map(h=>'{'+h.match(/../g).map(x=>'0x'+x).join(',')+'}').join(',\n')+'\n};\n').join(''));
console.log(`WaterFX: ${hashes.length} exact texture fingerprints`);
