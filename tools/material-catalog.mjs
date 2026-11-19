// Regenerate exact material identities. No client artwork is copied or embedded.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
const root = process.argv[2]; // Optional read-only verification against the client's original files.
const hash = b => crypto.createHash('sha256').update(b).digest('hex');
const identities = new Map();
function add(digest, field, value) {
  const entry = identities.get(digest) ?? {kind: 0, winter: 'None'};
  const empty = field === 'kind' ? 0 : 'None';
  if (entry[field] !== empty && entry[field] !== value)
    throw Error('Ambiguous material identity: ' + digest);
  entry[field] = value;
  identities.set(digest, entry);
}
const materials = JSON.parse(fs.readFileSync(new URL('./materials.json', import.meta.url)));
const winter = JSON.parse(fs.readFileSync(new URL('./winter-materials.json', import.meta.url)));
for (const item of [...materials, ...winter]) {
  if (![item.decodedHash, item.sourceHash, item.uploadHash ?? item.decodedHash].every(v => /^[a-f0-9]{64}$/.test(v)) ||
      (item.winter ? !['Ground','Paving','Plant','Exterior'].includes(item.winter) :
       !Number.isInteger(item.kind) || item.kind < 1 || item.kind > 7))
    throw Error('Invalid material metadata: ' + item.file);
  const field = item.winter ? 'winter' : 'kind', value = item[field];
  add(item.decodedHash, field, value);
  if (item.uploadHash) add(item.uploadHash, field, value);
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
let out = '#pragma once\nstruct MaterialIdentity { unsigned char hash[32]; unsigned kind; WinterSurface winter; };\ninline constexpr MaterialIdentity MaterialIdentities[] = {\n';
for (const [digest, {kind, winter}] of [...identities].sort((a,b) => a[0].localeCompare(b[0])))
  out += '{{' + digest.match(/../g).map(v => '0x'+v).join(',') + '},' + kind + ',WinterSurface::' + winter + '},\n';
fs.writeFileSync('src/generated/material_catalog.hpp', out + '};\n');
console.log('Generated ' + identities.size + ' exact material identities');
