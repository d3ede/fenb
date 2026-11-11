import fs from 'node:fs';
import path from 'node:path';
import { defs, body, svg } from './build-logo-kit.mjs';

const out = path.resolve('assets/branding/daylight');
fs.mkdirSync(out, { recursive:true });
const palettes = {
 'violet-daylight': {label:'01 / VIOLET — DAYLIGHT', top:'#926ACB',mid:'#7651B4',bottom:'#50377F',edge:'#8765B0',deep:'#3B2464',ring:'#7750AD',shine:'#AF93CE',wordTop:'#594074',word:'#3E2C52'},
 'aurora-daylight': {label:'03 / AURORA — DAYLIGHT', top:'#2E98A8',mid:'#287D98',bottom:'#3467A1',edge:'#397F96',deep:'#203E66',ring:'#2C8298',shine:'#70A7B5',wordTop:'#31586D',word:'#263E55'}
};
let definitions='', presentation='', transparent='';
for(const [i,[key,p]] of Object.entries(palettes).entries()) {
 const x=64+i*1124;
 presentation+=`<text x="${x}" y="74" fill="#4D4659" font-family="Arial,sans-serif" font-size="28" letter-spacing="3">${p.label}</text>`;
 for(const layout of ['square','horizontal','symbol']) {
  const [w,h]=layout==='square'?[1024,1024]:layout==='horizontal'?[1600,500]:[600,520];
  const k=`${key}-${layout}`;
  fs.writeFileSync(path.join(out,`fenb-${k}.svg`),svg(w,h,body(p,k,layout),defs(p,k),`FENB / ${key} / ${layout}`));
  const y={square:120,horizontal:1244,symbol:1700}[layout];
  const scale={square:1,horizontal:.64,symbol:.6}[layout];
  const dx=layout==='symbol'?330:0;
  const hk=`sheet-${k}`;
  definitions+=defs(p,hk);
  const item=`<g id="FENB-${key}-${layout}" transform="translate(${x+dx} ${y}) scale(${scale})">${body(p,hk,layout)}</g>`;
  transparent+=item;
  presentation+=`<rect id="Preview-only-${key}-${layout}" x="${x}" y="${y}" width="1024" height="${layout==='square'?1024:layout==='horizontal'?320:312}" rx="20" fill="${layout==='horizontal'?'#F3F1F6':'#FFFFFF'}"/>${item}`;
 }
}
fs.writeFileSync(path.join(out,'FENB-daylight-Figma-import.svg'),svg(2352,2080,transparent,definitions,'FENB / daylight / transparent editable vectors'));
fs.writeFileSync(path.join(out,'FENB-daylight-preview.svg'),svg(2352,2080,`<rect width="2352" height="2080" fill="#E8E5EC"/>${presentation}`,definitions,'FENB / daylight variants on white and light gray'));
fs.writeFileSync(path.join(out,'README.txt'),`FENB / DAYLIGHT\n\nViolet and Aurora adapted for white and light gray backgrounds.\n6 standalone transparent SVGs: square / horizontal / symbol in each palette.\nAll lettering is outlined; vector paths and gradients only.\n\nChanges: deeper colors, muted colored highlights, dark wordmarks.\nNo white glare, external fonts, embedded raster images, or background fills in export assets.\nPreview contains separate white and gray panels for comparison only.\n\nSquare: 1024 x 1024\nHorizontal: 1600 x 500\nSymbol: 600 x 520\n`);
const luminance=h=>{const c=h.match(/[0-9a-f]{2}/gi).map(v=>parseInt(v,16)/255).map(v=>v<=.04045?v/12.92:((v+.055)/1.055)**2.4); return .2126*c[0]+.7152*c[1]+.0722*c[2];};
console.log(JSON.stringify({directory:out,files:fs.readdirSync(out),wordmarkContrastOnWhite:Object.fromEntries(Object.entries(palettes).map(([k,p])=>[k,[p.wordTop,p.word].map(c=>Number((1.05/(luminance(c)+.05)).toFixed(2)))]))},null,2));
