import fs from 'node:fs';
const exports = JSON.parse(fs.readFileSync('tools/system-exports.json','utf8').replace(/^\uFEFF/,''));
const hooked = new Set(['Direct3DCreate9','Direct3DCreate9Ex','Direct3DCreate9On12','Direct3DCreate9On12Ex']);
let def = 'LIBRARY d3d9\nEXPORTS\n';
let msvcDef = 'LIBRARY d3d9\nEXPORTS\n';
let cpp = '// Generated x86 tail-jump forwarders. Preserve argument/return ABI, including private exports.\n#include "exports.hpp"\n';
for(const e of exports) {
  const symbol = e.name || `Ordinal${e.ordinal}`;
  def += `    ${symbol} = FENB_${symbol} @${e.ordinal}${e.name ? '' : ' NONAME'}\n`;
  const stackBytes = {Direct3DCreate9:4,Direct3DCreate9Ex:8,Direct3DCreate9On12:12,Direct3DCreate9On12Ex:16};
  msvcDef += `    ${symbol} = _FENB_${symbol}${hooked.has(symbol)?'@'+stackBytes[symbol]:''} @${e.ordinal}${e.name ? '' : ' NONAME'}\n`;
  if(hooked.has(symbol)) continue;
  cpp += `extern "C" void Resolve_${symbol}();\nextern "C" { void* target_${symbol} = reinterpret_cast<void*>(&Resolve_${symbol}); }\n`;
  cpp += `extern "C" __declspec(naked) void Resolve_${symbol}() {\n#if defined(_MSC_VER) && !defined(__clang__)\n    __asm { pushfd }\n    __asm { pushad }\n    __asm { mov ebx, esp }\n    __asm { and esp, -16 }\n    __asm { call EnsureSystem }\n    __asm { mov esp, ebx }\n    __asm { popad }\n    __asm { popfd }\n    __asm { jmp dword ptr [target_${symbol}] }\n#else\n    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_${symbol}");\n#endif\n}\n`;
  cpp += `extern "C" __declspec(naked) void FENB_${symbol}() {\n#if defined(_MSC_VER) && !defined(__clang__)\n    __asm { jmp dword ptr [target_${symbol}] }\n#else\n    __asm__ volatile("jmp *_target_${symbol}");\n#endif\n}\n`;
}
cpp += 'void BindForwarders(HMODULE module) {\n';
for(const e of exports) {
  const symbol=e.name||`Ordinal${e.ordinal}`;
  if(!hooked.has(symbol)) cpp += `    target_${symbol} = reinterpret_cast<void*>(GetProcAddress(module, ${e.name ? JSON.stringify(e.name) : `MAKEINTRESOURCEA(${e.ordinal})`}));\n`;
}
cpp += '}\n';
fs.writeFileSync('src/generated/exports.cpp',cpp);
fs.writeFileSync('src/d3d9.def',def);
fs.writeFileSync('src/d3d9-msvc.def',msvcDef);

