import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const out = path.resolve('assets/branding');
fs.mkdirSync(out, { recursive: true });
const palettes = {
  violet: { label: 'Violet / light', top:'#F7EDFF', mid:'#D6B9FF', bottom:'#A077E9', edge:'#FFFFFF', deep:'#6941C6', ring:'#A977FF', shine:'#FFF7FF', word:'#E6D9FF' },
  reverse: { label: 'Reverse / dark', top:'#35234F', mid:'#624397', bottom:'#251638', edge:'#4C326E', deep:'#291342', ring:'#7144AC', shine:'#B28BDF', word:'#30203F' },
  aurora: { label: 'Aurora / cyan blue', top:'#E5FFFC', mid:'#73E8ED', bottom:'#5088EB', edge:'#DCFFFF', deep:'#255CAD', ring:'#32C7D7', shine:'#F2FFFF', word:'#A3E9F4' }
};
const F='M160 455V180C160 112 191 78 259 78H478L402 155H256C234 155 222 168 222 190V250C236 224 253 211 282 211H407L335 282H248C231 282 222 294 222 312V394Z';
const star='M458 183C458 252 453 269 386 269C453 269 458 285 458 357C458 285 464 269 532 269C464 269 458 252 458 183Z';
const letters=[
  ['F',0,'M0 0H104V18H21V37H87V55H21V90H0Z'],
  ['E',164,'M0 0H105V18H21V36H90V54H21V72H105V90H0Z'],
  ['N',331,'M0 90V0H16C23 0 28 3 34 9L97 69V0H118V90H101C95 90 91 87 87 83L21 24V90Z'],
  ['B',511,'M0 0H88C109 0 120 9 120 24C120 35 114 41 105 45C118 48 124 56 124 68C124 83 113 90 93 90H0ZM21 18V36H85C94 36 99 33 99 27C99 21 94 18 85 18ZM21 54V72H89C98 72 103 69 103 63C103 57 98 54 89 54Z']
];
export function defs(p,k) { return `<defs>
<clipPath id="${k}-F-clip"><path d="${F}"/></clipPath>
<linearGradient id="${k}-metal" x1="340" y1="75" x2="153" y2="448" gradientUnits="userSpaceOnUse"><stop stop-color="${p.top}"/><stop offset=".48" stop-color="${p.mid}"/><stop offset="1" stop-color="${p.bottom}"/></linearGradient>
<linearGradient id="${k}-orbit" x1="83" y1="398" x2="548" y2="155" gradientUnits="userSpaceOnUse"><stop stop-color="${p.ring}"/><stop offset=".22" stop-color="${p.shine}"/><stop offset=".49" stop-color="${p.ring}"/><stop offset=".8" stop-color="${p.deep}"/><stop offset="1" stop-color="${p.ring}"/></linearGradient>
<linearGradient id="${k}-star" x1="430" y1="230" x2="489" y2="312" gradientUnits="userSpaceOnUse"><stop stop-color="${p.shine}"/><stop offset=".5" stop-color="${p.mid}"/><stop offset="1" stop-color="${p.ring}"/></linearGradient>
<linearGradient id="${k}-word" x1="310" y1="0" x2="330" y2="90" gradientUnits="userSpaceOnUse"><stop stop-color="${p.wordTop ?? p.top}"/><stop offset="1" stop-color="${p.word}"/></linearGradient>
</defs>`; }
export function emblem(p,k) { return `<g id="${k}-emblem">
<path id="${k}-orbit-back" d="M398 152C503 121 571 134 561 173C555 196 527 224 484 252L477 244C515 216 540 190 535 173C529 151 470 145 398 152Z" fill="url(#${k}-orbit)"/>
<path id="${k}-F" d="${F}" fill="url(#${k}-metal)" stroke="${p.edge}" stroke-opacity=".65" stroke-width="1.6" stroke-linejoin="round"/>
<g id="${k}-facets" clip-path="url(#${k}-F-clip)">
<path id="${k}-top-facet" d="M160 248L238 169C229 175 222 181 222 190V250C207 269 185 300 160 336Z" fill="${p.deep}" opacity=".45"/>
<path id="${k}-lower-facet" d="M160 455V387L335 211H407L335 282H248C231 282 222 294 222 312V394Z" fill="${p.top}" opacity=".23"/>
<path id="${k}-top-sheen" d="M259 78H398L322 155H256C234 155 222 168 222 190L160 252V180C160 112 191 78 259 78Z" fill="${p.shine}" opacity=".13"/>
</g>
<path id="${k}-orbit-front" d="M160 265C107 302 57 343 58 369C59 413 235 383 429 289C279 371 104 419 64 394C24 369 85 309 160 265Z" fill="url(#${k}-orbit)"/>
<path id="${k}-orbit-glint" d="M65 386C87 403 154 394 220 376C149 398 85 407 65 386Z" fill="${p.shine}" opacity=".85"/>
<path id="${k}-spark" d="${star}" fill="url(#${k}-star)"/>
<path id="${k}-spark-core" d="M458 230C458 261 453 269 421 269C453 269 458 277 458 307C458 277 464 269 496 269C464 269 458 261 458 230Z" fill="${p.shine}" opacity=".65"/>
</g>`; }
export function wordmark(p,k) {return `<g id="${k}-wordmark" fill="url(#${k}-word)" fill-rule="evenodd">${letters.map(([n,x,d])=>`<path id="${k}-letter-${n}" transform="translate(${x} 0)" d="${d}"/>`).join('')}</g>`;}
export function body(p,k,layout){
  if(layout==='square') return `<g transform="translate(212 110)">${emblem(p,k)}</g><g transform="translate(194.5 625)">${wordmark(p,k)}</g>`;
  if(layout==='horizontal') return `<g transform="translate(34 10) scale(.83)">${emblem(p,k)}</g><g transform="translate(580 194) scale(1.38)">${wordmark(p,k)}</g>`;
  return `<g transform="translate(-9 -8)">${emblem(p,k)}</g>`;
}
export function svg(w,h,contents,definitions='',title='FENB') {return `<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}" fill="none"><title>${title}</title>${definitions}${contents}</svg>`;}
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
for(const [key,p] of Object.entries(palettes)) {
 for(const layout of ['square','horizontal','symbol']) {
  const [w,h]=layout==='square'?[1024,1024]:layout==='horizontal'?[1600,500]:[600,520];
  const k=`${key}-${layout}`;
  fs.writeFileSync(path.join(out,`fenb-${k}.svg`),svg(w,h,body(p,k,layout),defs(p,k),`FENB — ${p.label} — ${layout}`));
 }
}
// Figma import: independent transparent logo groups, arranged as a compact kit.
let allDefs='', transparent='', preview='';
for(const [i,[key,p]] of Object.entries(palettes).entries()) {
 const x=60+i*1124, bg=key==='reverse'?'#F6F3FA':'#101019';
 for(const [layout,y] of [['square',120],['horizontal',1244],['symbol',1700]]) {
  const k=`board-${key}-${layout}`; allDefs+=defs(p,k);
  const scale=layout==='horizontal'?.64:layout==='symbol'?.6:1;
  const offset=layout==='symbol'?330:0;
  const item=`<g id="FENB-${key}-${layout}" transform="translate(${x+offset} ${y}) scale(${scale})">${body(p,k,layout)}</g>`;
  transparent+=item;
  const height=layout==='square'?1024:layout==='horizontal'?320:312;
  preview+=`<rect x="${x}" y="${y}" width="1024" height="${height}" rx="20" fill="${bg}"/>${item}`;
 }
}
fs.writeFileSync(path.join(out,'FENB-Figma-import.svg'),svg(3432,2080,transparent,allDefs,'FENB / editable vector logo kit'));
const headings=['01 / VIOLET','02 / REVERSE','03 / AURORA'];
let labels=headings.map((t,i)=>`<text x="${60+i*1124}" y="75" fill="#DDD8E8" font-family="Arial,sans-serif" font-size="30" letter-spacing="5">${t}</text>`).join('');
fs.writeFileSync(path.join(out,'FENB-preview.svg'),svg(3432,2080,`<rect width="3432" height="2080" fill="#202029"/>${labels}${preview}`,allDefs,'FENB / logo presentation'));
fs.writeFileSync(path.join(out,'README.txt'),`FENB VECTOR LOGO KIT\n\n9 transparent SVG assets: 3 palettes x square, horizontal, symbol.\nAll lettering is outlined. No external fonts or raster images.\n\nViolet: light mark for dark backgrounds.\nReverse: dark violet mark for light backgrounds.\nAurora: cyan-blue alternative for dark backgrounds.\n\nFENB-Figma-import.svg: transparent vector sheet; import into Figma.\nFENB-preview.svg: presentation only; contains background panels.\n\nSquare 1024 x 1024 | Horizontal 1600 x 500 | Symbol 600 x 520\n`);
console.log(JSON.stringify({directory:out,files:fs.readdirSync(out)},null,2));
}
