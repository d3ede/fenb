// Exact native sun-disc and lens-flare identities, decoded offline with texture-fingerprint.cpp.
import fs from 'node:fs';
import crypto from 'node:crypto';
import path from 'node:path';
const root=process.argv[2];
if(!root)throw Error('Usage: node tools/sun-catalog.mjs <client directory>');
let out='#pragma once\nstruct SunIdentity { unsigned char hash[32]; bool disc; };\ninline constexpr SunIdentity SunIdentities[] = {\n';
for(const item of JSON.parse(fs.readFileSync(new URL('./sun-decoded.json',import.meta.url),'utf8'))){
 const actual=crypto.createHash('sha256').update(fs.readFileSync(path.join(root,item.file))).digest('hex');
 if(actual!==item.rawHash)throw Error('Source asset changed: '+item.file);
 out+='// '+item.file+' (ARGB8 upload)\n{{'+item.sha256.match(/../g).map(x=>'0x'+x).join(',')+'},'+(item.kind==='disc'?'true':'false')+'},\n';
}
fs.writeFileSync('src/generated/sun_catalog.hpp',out+'};\n');
