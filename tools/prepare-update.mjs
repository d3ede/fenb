// Publish this manifest beside the versioned ZIP, under a v<version> release tag.
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
const version = fs.readFileSync(new URL('../src/version.hpp', import.meta.url), 'utf8')
  .match(/FENBVersion\[\]\s*=\s*"(\d\.\d[a-h])"/)?.[1];
if (!version) throw Error('Invalid public version');
const directory = process.argv[2] ?? 'dist';
const archive = fs.readFileSync(path.join(directory, `FENB-${version}-win32.zip`));
if (archive.length < 512 || archive.length > 33554432) throw Error('Invalid archive size');
fs.writeFileSync(path.join(directory, 'FENB-update.json'), JSON.stringify({
  schema: 1, version, bytes: archive.length, sha256: createHash('sha256').update(archive).digest('hex'),
}, null, 2) + '\n');
