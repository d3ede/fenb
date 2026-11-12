// Keep distribution notices in sync with the original, separately tracked licenses.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('../', import.meta.url));
const output = path.resolve(root, process.argv[2] ?? 'dist');
const notices = [
  ['FENB', 'LICENSE'],
  ['Dear ImGui', 'vendor/imgui-1.92.9b/LICENSE.txt'],
  ['Inter font', 'assets/fonts/OFL.txt'],
  ['SMAA', 'vendor/smaa/LICENSE.txt'],
];
const sections = notices.map(([name, source]) =>
  `${'='.repeat(72)}\n${name}\n${'='.repeat(72)}\n\n${fs.readFileSync(path.join(root, source), 'utf8').trimEnd()}\n`);
fs.mkdirSync(output, { recursive: true });
fs.writeFileSync(path.join(output, 'LICENSES.txt'), sections.join('\n'), 'utf8');
fs.copyFileSync(path.join(root, 'docs/INSTALL.txt'), path.join(output, 'INSTALL.txt'));
