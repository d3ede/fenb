import fs from 'node:fs';
import path from 'node:path';
import { defs,emblem,wordmark,svg } from './build-logo-kit.mjs';
import { Resvg } from '../artifacts/logo-renderer/node_modules/@resvg/resvg-js/index.js';

const out=path.resolve('artifacts/FENB-Premium');
fs.mkdirSync(out,{recursive:true});
const p={top:'#FAF1FF',mid:'#D5B9F9',bottom:'#B78BE8',edge:'#F8EDFF',deep:'#61409D',ring:'#A879F0',shine:'#FFF2FF',word:'#D3C9EB'};
const k='premium';
let base=defs(p,k).slice(6,-7);
base=base.replace(/(<linearGradient id="premium-metal"[^>]*>)[\s\S]*?(<\/linearGradient>)/,`$1<stop stop-color="#FCF6FF"/><stop offset=".23" stop-color="#EEDFFF"/><stop offset=".47" stop-color="#D8BEFC"/><stop offset=".69" stop-color="#B28DE5"/><stop offset=".88" stop-color="#DCADF3"/><stop offset="1" stop-color="#E7BFFF"/>$2`);
base=base.replace(/(<linearGradient id="premium-word"[^>]*>)[\s\S]*?(<\/linearGradient>)/,`$1<stop stop-color="#F9F6FF"/><stop offset=".25" stop-color="#E5DEF6"/><stop offset=".53" stop-color="#B5ACD4"/><stop offset=".7" stop-color="#E8DFFA"/><stop offset="1" stop-color="#B7A8DD"/>$2`);
const filter=(id,blur,color)=>`<filter id="${id}" x="-70%" y="-70%" width="240%" height="240%" color-interpolation-filters="sRGB"><feFlood flood-color="${color}"/><feComposite in2="SourceAlpha" operator="in"/><feGaussianBlur stdDeviation="${blur}"/></filter>`;
base+=filter('glow-wide',22,'#6430E8')+filter('glow-mid',8,'#8647FF')+filter('glow-near',2.4,'#D0A1FF')+filter('glow-word',2.3,'#9A7ADE')+filter('glow-edge',3.2,'#B67AFF');
base+='<radialGradient id="background-navy" cx="82%" cy="16%" r="83%"><stop stop-color="#17172D"/><stop offset="1" stop-color="#020306"/></radialGradient><radialGradient id="background-halo"><stop stop-color="#6234B1" stop-opacity=".16"/><stop offset=".55" stop-color="#341B75" stop-opacity=".06"/><stop offset="1" stop-color="#190D39" stop-opacity="0"/></radialGradient><linearGradient id="rim-light"><stop stop-color="#DDBEFF"/><stop offset=".55" stop-color="#FFFFFF"/><stop offset="1" stop-color="#D1ADFF" stop-opacity=".15"/></linearGradient><linearGradient id="orbit-rim" x1="398" y1="152" x2="562" y2="200" gradientUnits="userSpaceOnUse"><stop stop-color="#7E4BC4" stop-opacity="0"/><stop offset=".66" stop-color="#B58AFF" stop-opacity=".5"/><stop offset=".88" stop-color="#F5DEFF"/><stop offset="1" stop-color="#AE7BEF" stop-opacity=".35"/></linearGradient>';
// Delicate metal grain stays inside the F contour and uses actual vector segments.
let seed=78491;const rand=()=>{seed=(seed*1664525+1013904223)>>>0;return seed/4294967296;};
let grain='';
for(let i=0;i<1000;i++){
 const x=160+rand()*318,y=78+rand()*377,len=.3+rand()*2.2;
 grain+=`<path d="M${x.toFixed(2)} ${y.toFixed(2)}l${len.toFixed(2)} ${(-len*.3).toFixed(2)}" stroke="${rand()>.55?'#FFFFFF':'#7954B4'}" stroke-width=".35" opacity=".075"/>`;
}
const metalGrain=`<g clip-path="url(#premium-F-clip)">${grain}</g>`;
const rim='<g fill="none" stroke-linecap="round"><path d="M160 455V180C160 112 191 78 259 78H478" stroke="url(#rim-light)" stroke-width="1.25"/><path d="M282 211H407" stroke="#F6E7FF" stroke-width="1.1" opacity=".9"/></g>';
const mark=emblem(p,k).replace(/<\/g>\s*$/,`${metalGrain}${rim}</g>`);
const word=wordmark(p,k).replace('fill-rule="evenodd"','fill-rule="evenodd" stroke="#F2E9FF" stroke-opacity=".65" stroke-width=".6" stroke-linejoin="round"');
base+=mark+word;
function litMark(transform){return `<g transform="${transform}"><ellipse cx="350" cy="78" rx="85" ry="5" fill="#8E52FF" filter="url(#glow-wide)" opacity=".55"/><use href="#premium-emblem" filter="url(#glow-wide)" opacity=".34"/><use href="#premium-emblem" filter="url(#glow-mid)" opacity=".44"/><use href="#premium-emblem" filter="url(#glow-near)" opacity=".28"/><use href="#premium-emblem"/><path d="M398 152C503 121 571 134 561 173C555 196 527 224 484 252" fill="none" stroke="url(#orbit-rim)" stroke-width="1.25"/><path d="M259 78H478" fill="none" stroke="#D3A3FF" stroke-width="3" filter="url(#glow-edge)" opacity=".7"/><path d="M259 78H478" fill="none" stroke="#FBF3FF" stroke-width=".7" opacity=".95"/><ellipse cx="458" cy="269" rx="3" ry="3" fill="#FFF4FF" filter="url(#glow-near)" opacity=".75"/></g>`;}
function litWord(transform){return `<g transform="${transform}"><use href="#premium-wordmark" filter="url(#glow-word)" opacity=".18"/><use href="#premium-wordmark"/></g>`;}
const compositions=[
 {name:'stacked',w:1024,h:1024,mark:'translate(280 214) scale(.82)',word:'translate(254.825 628) scale(.81)',halo:[520,426,340,280]},
 {name:'symbol',w:1024,h:1024,mark:'translate(70 126) scale(1.45)',word:null,halo:[512,512,440,400]},
 {name:'horizontal',w:1600,h:500,mark:'translate(28 20) scale(.82)',word:'translate(575 195) scale(1.38)',halo:[500,240,700,250]}
];
const results=[];
for(const c of compositions){
 for(const transparent of [false,true]){
  const name=`fenb-premium-${c.name}-${transparent?'transparent':'dark'}`;
  let content='';
  if(!transparent){
   content+=`<rect width="${c.w}" height="${c.h}" fill="#020306"/><rect width="${c.w}" height="${c.h}" fill="url(#background-navy)" opacity=".63"/>`;
   content+=`<ellipse cx="${c.halo[0]}" cy="${c.halo[1]}" rx="${c.halo[2]}" ry="${c.halo[3]}" fill="url(#background-halo)"/>`;
  }
  content+=litMark(c.mark)+(c.word?litWord(c.word):'');
  const source=svg(c.w,c.h,content,`<defs>${base}</defs>`,`FENB / premium violet bloom / ${c.name} / ${transparent?'transparent':'dark'}`);
  fs.writeFileSync(path.join(out,name+'.svg'),source);
  const render=new Resvg(source,{fitTo:{mode:'zoom',value:2}}).render();
  fs.writeFileSync(path.join(out,name+'.png'),render.asPng());
  const a=render.pixels[3];
  if(transparent?a!==0:a!==255) throw new Error(`Invalid alpha: ${name}`);
  results.push({name,width:render.width,height:render.height,cornerAlpha:a});
 }
}
fs.writeFileSync(path.join(out,'README.txt'),'FENB / PREMIUM VIOLET BLOOM\n\nReference: the original pale-violet metallic F, orbit, four-point star, FENB wordmark and near-black navy background.\n\nThree compositions: stacked, square symbol, horizontal.\nEach includes dark-background and genuinely transparent SVG + PNG.\nPNG files are rendered at 2x; stacked and symbol are 2048 x 2048; horizontal is 3200 x 1000.\n\nMaster artwork consists of vector paths, gradients, clipped vector metal grain and SVG Gaussian-blur bloom. No embedded raster images or external fonts.\nFor the exact finished bloom appearance, use PNG. SVG filter rendering may vary between editors.\nThe internal F contour correction is preserved.\n');
console.log(JSON.stringify({folder:out,assets:results},null,2));
