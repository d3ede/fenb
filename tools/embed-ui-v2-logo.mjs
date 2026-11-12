import fs from 'node:fs';

// Existing approved horizontal lockup, already cropped with its glow margin.
const bytes = fs.readFileSync('assets/branding/header-preview/horizontal.bgra');
const width = bytes.readUInt32LE(0), height = bytes.readUInt32LE(4);
if (!width || !height || width > 512 || height > 256 || bytes.length !== 8 + width * height * 4)
  throw new Error('Invalid horizontal logo BGRA asset');
const lines = [];
for (let offset = 8; offset < bytes.length; offset += 32) {
  const words = [];
  for (let p = offset; p < Math.min(offset + 32, bytes.length); p += 4)
    words.push('0x' + bytes.readUInt32LE(p).toString(16).padStart(8, '0') + 'u');
  lines.push('    ' + words.join(', ') + ',');
}
fs.mkdirSync('build/ui-v2', { recursive: true });
fs.writeFileSync('build/ui-v2/ui_studio_logo.hpp',
  `// Generated from the approved horizontal lockup by tools/embed-ui-v2-logo.mjs.\n#pragma once\n#include <cstdint>\ninline constexpr unsigned STUDIO_LOGO_WIDTH = ${width};\ninline constexpr unsigned STUDIO_LOGO_HEIGHT = ${height};\ninline constexpr std::uint32_t STUDIO_LOGO_BGRA[] = {\n${lines.join('\n')}\n};\n`);
