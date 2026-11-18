// Offline import of reviewed replacement art. Neither input directory is modified.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {execFileSync} from 'node:child_process';
const [art,client]=process.argv.slice(2);
if(!art||!client)throw Error('Usage: node tools/prepare-tree-assets.mjs <reviewed-preview-directory> <read-only-client-directory>');
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const mapping=JSON.parse(fs.readFileSync(path.join(art,'replacement-map.json'),'utf8').replace(/^\uFEFF/,''));
const report=JSON.parse(fs.readFileSync(path.join(art,'atlas-packing-report.json'),'utf8').replace(/^\uFEFF/,''));
const files=Object.keys(mapping).sort();
const decoded=JSON.parse(execFileSync('build/texture-fingerprint.exe',files.map(f=>path.join(client,'Model/Texture',f)),{encoding:'utf8',windowsHide:true}));
fs.mkdirSync('assets/trees',{recursive:true});
const unique=new Map(),sources=[];
for(const file of files){
 const png=path.join(art,'textures-new',file+'.png'),pngHash=hash(fs.readFileSync(png));
 const info=report.find(x=>x.texture===file);if(!info)throw Error('Missing reviewed atlas '+file);
 let asset=unique.get(pngHash);
 if(!asset){asset=`assets/trees/bark-${String(unique.size+1).padStart(2,'0')}.dds`;unique.set(pngHash,asset);
  execFileSync('build/tree-texturec.exe',[png,asset,info.alphaPixelsPreserved?'alpha':'opaque'],{stdio:'inherit',windowsHide:true});}
 const source=fs.readFileSync(path.join(client,'Model/Texture',file));
 if(source.toString('ascii',0,4)!=='DDS ')throw Error('Expected source DDS '+file);
 const width=source.readUInt32LE(16),height=source.readUInt32LE(12),four=source.toString('ascii',84,88);
 const size=/DXT[1-5]/.test(four)?Math.ceil(width/4)*Math.ceil(height/4)*(four==='DXT1'?8:16):width*height*source.readUInt32LE(88)/8;
 const raw=decoded.find(x=>path.basename(x.file).toLowerCase()===file);
 if(!raw||raw.width!==width||raw.height!==height)throw Error('Missing decoded identity '+file);
 sources.push({file,width,height,sourceFormat:four.replace(/\0/g,''),sourceSha256:hash(source),
               uploadSha256:hash(source.subarray(128,128+size)),decodedSha256:raw.sha256,asset});
}
fs.writeFileSync('tools/trees.json',JSON.stringify({schema:1,sources},null,2)+'\n');
console.log(`Prepared ${unique.size} DDS assets for ${sources.length} exact source identities.`);
