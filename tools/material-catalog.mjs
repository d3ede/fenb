// Regenerate exact material identities. No client artwork is copied or embedded.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
const root = process.argv[2]; // Optional read-only verification against the client's original files.
const hash = b => crypto.createHash('sha256').update(b).digest('hex');
const identities = new Map();
function add(digest, kind) {
  if (identities.has(digest) && identities.get(digest) !== kind)
    throw Error('Ambiguous material identity: ' + digest);
  identities.set(digest, kind);
}
for (const item of JSON.parse(fs.readFileSync(new URL('./materials.json', import.meta.url)))) {
  if (![item.decodedHash, item.sourceHash, item.uploadHash ?? item.decodedHash].every(v => /^[a-f0-9]{64}$/.test(v)) ||
      !Number.isInteger(item.kind) || item.kind < 1 || item.kind > 7)
    throw Error('Invalid material metadata: ' + item.file);
  add(item.decodedHash, item.kind);
  if (item.uploadHash) add(item.uploadHash, item.kind);
  if (!root) continue;
  const b = fs.readFileSync(path.join(root, item.file));
  if (hash(b) !== item.sourceHash) throw Error('Changed source asset: ' + item.file);
  if (b.toString('ascii', 0, 4) === 'DDS ') {
    const w = b.readUInt32LE(16), h = b.readUInt32LE(12), four = b.toString('ascii', 84, 88);
    const size = four === 'DXT1' ? Math.ceil(w/4)*Math.ceil(h/4)*8 :
      /DXT[35]/.test(four) ? Math.ceil(w/4)*Math.ceil(h/4)*16 : w*h*b.readUInt32LE(88)/8;
    if (!size || 128+size > b.length) throw Error('Incomplete DDS: ' + item.file);
    if (hash(b.subarray(128, 128+size)) !== item.uploadHash)
      throw Error('Changed upload identity: ' + item.file);
  }
}
let out = '#pragma once\nstruct MaterialIdentity { unsigned char hash[32]; unsigned kind; };\ninline constexpr MaterialIdentity MaterialIdentities[] = {\n';
for (const [digest, kind] of [...identities].sort((a,b) => a[0].localeCompare(b[0])))
  out += '{{' + digest.match(/../g).map(v => '0x'+v).join(',') + '},' + kind + '},\n';
fs.writeFileSync('src/generated/material_catalog.hpp', out + '};\n');
console.log('Generated ' + identities.size + ' exact material identities');
