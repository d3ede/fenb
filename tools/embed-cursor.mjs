import fs from 'node:fs';

// The editable SVG is the single geometry source for the standalone preview.
const svg = fs.readFileSync('assets/cursors/fenb-pointer.svg', 'utf8');
const path = svg.match(/d="([^"]+)"/)?.[1];
if (!path || !/^M [\d.]+ [\d.]+(?: L [\d.]+ [\d.]+){3} Z$/.test(path))
  throw new Error('Expected one closed four-point cursor silhouette');
const points = path.match(/[\d.]+/g).map(Number);
const fill = svg.match(/fill="#([A-Fa-f0-9]{6})"/)?.[1];
const stroke = svg.match(/stroke="#([A-Fa-f0-9]{6})"/)?.[1];
const weight = Number(svg.match(/stroke-width="([\d.]+)"/)?.[1]);
if (!fill || !stroke || !weight) throw new Error('Missing cursor paints');
const rgb = hex => [0, 2, 4].map(i => parseInt(hex.slice(i, i + 2), 16)).join(', ');
const vertices = [];
for (let i = 0; i < points.length; i += 2) vertices.push(`{${points[i].toFixed(2)}f, ${points[i + 1].toFixed(2)}f}`);
fs.mkdirSync('build/ui-v2', { recursive: true });
fs.writeFileSync('build/ui-v2/ui_cursor.hpp', `// Generated from assets/cursors/fenb-pointer.svg.\n#pragma once\ninline constexpr ImVec2 CursorPoints[] = {${vertices.join(', ')}};\ninline constexpr ImU32 CursorFill = IM_COL32(${rgb(fill)}, 255);\ninline constexpr ImU32 CursorEdge = IM_COL32(${rgb(stroke)}, 255);\ninline constexpr float CursorWeight = ${weight.toFixed(2)}f;\n`);
