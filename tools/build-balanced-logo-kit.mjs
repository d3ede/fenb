import fs from 'node:fs';
import path from 'node:path';
import { defs,body,svg } from './build-logo-kit.mjs';
import { Resvg } from '../artifacts/logo-renderer/node_modules/@resvg/resvg-js/index.js';

const root=path.resolve('assets/branding');
const vivid=process.argv.includes('--vivid');
const edition=vivid?'vivid':'balanced';
const out=path.join(root,edition);
fs.mkdirSync(out,{recursive:true});
const palettes=vivid?{
 violet:{top:'#D7B4F8',mid:'#B17EE4',bottom:'#8D58C9',edge:'#D7B5F1',deep:'#592D9C',ring:'#A36BDE',shine:'#ECD7FF',wordTop:'#BE99E3',word:'#9C67C5'},
 aurora:{top:'#9FE9EF',mid:'#4DC6D9',bottom:'#478FCA',edge:'#A5E3EA',deep:'#235892',ring:'#30B2CF',shine:'#D2F5F7',wordTop:'#6EC9D8',word:'#3997B6'}
}:{
 violet:{top:'#D0B4EC',mid:'#AC86D2',bottom:'#815DB2',edge:'#CEB4E5',deep:'#503585',ring:'#9870C9',shine:'#E0CDF1',wordTop:'#B49ACF',word:'#805EA6'},
 aurora:{top:'#AADEE2',mid:'#65B3C5',bottom:'#4E84B9',edge:'#ACD9DE',deep:'#2B507D',ring:'#4B9EB6',shine:'#C8E9EA',wordTop:'#81B7C2',word:'#4D8199'}
};
function glossyDefs(p,k) {
 return defs(p,k).replace(/(<linearGradient id="[^"]+-metal"[^>]*>)[\s\S]*?(<\/linearGradient>)/,`$1<stop stop-color="${p.top}"/><stop offset=".37" stop-color="${p.mid}"/><stop offset=".48" stop-color="${p.top}"/><stop offset=".62" stop-color="${p.mid}"/><stop offset="1" stop-color="${p.bottom}"/>$2`);
}
let definitions='',sheet='';
for(const [i,[key,p]] of Object.entries(palettes).entries()) {
 for(const layout of ['square','horizontal','symbol']) {
  const k=`${key}-${edition}-${layout}`;
  const [w,h]=layout==='square'?[1024,1024]:layout==='horizontal'?[1600,500]:[600,520];
  fs.writeFileSync(path.join(out,`fenb-${k}.svg`),svg(w,h,body(p,k,layout),glossyDefs(p,k),`FENB / ${key} / balanced gloss / ${layout}`));
  const y={square:80,horizontal:1170,symbol:1600}[layout],scale={square:1,horizontal:.64,symbol:.6}[layout];
  const dx=layout==='symbol'?330:0;
  const sk=`import-${k}`;
  definitions+=glossyDefs(p,sk);
  sheet+=`<g id="FENB-${k}" transform="translate(${64+i*1124+dx} ${y}) scale(${scale})">${body(p,sk,layout)}</g>`;
 }
}
fs.writeFileSync(path.join(out,`FENB-${edition}-Figma-import.svg`),svg(2352,1990,sheet,definitions,`FENB / ${edition} gloss / editable transparent kit`));
fs.writeFileSync(path.join(out,'README.txt'),`FENB / ${edition.toUpperCase()} GLOSS\n\n${vivid?'Brighter and more saturated revision of Balanced.':'Intermediate finish between Original and Daylight.'}\nColored highlights, soft metallic band, medium-tone wordmarks.\nAll decorative facets are clipped to the F silhouette to remove protruding fragments.\n\n6 transparent SVG files: Violet and Aurora in square, horizontal, symbol layouts.\nPNG exports at 2x. Figma-import SVG contains all 6 editable vector variants.\n`);

const text=(x,y,s,size=24,color='#353340',weight=400)=>`<text x="${x}" y="${y}" font-family="Arial,sans-serif" font-size="${size}" fill="${color}" font-weight="${weight}">${s}</text>`;
function embed(file,id,x,y,w,h,viewBox){
 let s=fs.readFileSync(path.join(root,file),'utf8').replace(/<title>[\s\S]*?<\/title>/,'');
 s=s.replace(/id="([^"]+)"/g,(_,v)=>`id="${id}-${v}"`).replace(/url\(#([^)]+)\)/g,(_,v)=>`url(#${id}-${v})`);
 return s.replace(/<svg[^>]*>/,`<svg x="${x}" y="${y}" width="${w}" height="${h}" viewBox="${viewBox}" fill="none">`);
}
function save(name,w,h,contents){
 const s=svg(w,h,contents);
 fs.writeFileSync(path.join(out,name+'.svg'),s);
 fs.writeFileSync(path.join(out,name+'.png'),new Resvg(s,{font:{loadSystemFonts:true}}).render().asPng());
}
// Main preview: both colors on the same white and dark backgrounds.
let preview='<rect width="1440" height="1220" fill="#E9E8EE"/>';
preview+=text(48,62,vivid?'FENB / ярче и насыщеннее':'FENB / промежуточный блеск',32,'#282632',700);
preview+=text(48,103,vivid?'Более сочные цвета · мягкий металлический блик · исправленный контур':'Мягкий металлический блик · умеренное сияние · исправленный контур',21,'#666371');
for(const [r,key] of ['violet','aurora'].entries()) {
 for(let c=0;c<2;c++) {
  const x=48+c*696,y=142+r*518,bg=c===0?'#11111C':'#FFFFFF';
  preview+=`<rect x="${x}" y="${y}" width="648" height="486" rx="20" fill="${bg}"/>`;
  preview+=text(x+24,y+35,`${r===0?'01 / VIOLET':'03 / AURORA'} · ${c===0?'ТЁМНЫЙ ФОН':'СВЕТЛЫЙ ФОН'}`,17,c===0?'#AAA5BD':'#777181',700);
  preview+=embed(`${edition}/fenb-${key}-${edition}-square.svg`,`preview-${key}-${c}`,x+44,y+42,560,430,'170 170 680 575');
 }
}
save(`FENB-${edition}-preview`,1440,1220,preview);
// Three finishes, each on dark and white, for direct comparison.
let compare='<rect width="2040" height="1190" fill="#E9E8EE"/>';
compare+=text(48,62,vivid?'FENB / промежуточный → ярче и насыщеннее → исходный':'FENB / старый → промежуточный → приглушённый',32,'#282632',700);
compare+=text(48,102,'В каждом столбце одна версия на тёмном и белом фоне. Геометрия исправлена во всех версиях.',21,'#666371');
const columns=vivid?[['Предыдущий','balanced'],['Ярче и насыщеннее','vivid'],['Исходный','original']]:[['Старый','original'],['Промежуточный','balanced'],['Приглушённый','daylight']];
for(let c=0;c<3;c++) {
 const x=48+c*664;
 compare+=text(x,155,columns[c][0],26,'#353340',700);
 for(const [r,key] of ['violet','aurora'].entries()) {
  const y=184+r*492;
  const version=columns[c][1];
  const file=version==='original'?`fenb-${key}-horizontal.svg`:`${version}/fenb-${key}-${version}-horizontal.svg`;
  compare+=text(x,y+21,r===0?'01 / VIOLET':'03 / AURORA',17,'#676371',700);
  for(let b=0;b<2;b++) {
   const by=y+40+b*208;
   compare+=`<rect x="${x}" y="${by}" width="632" height="196" rx="16" fill="${b===0?'#11111C':'#FFFFFF'}"/>`;
   compare+=embed(file,`compare-${r}-${c}-${b}`,x+12,by+5,608,186,'0 0 1600 500');
  }
 }
}
save(`FENB-${edition}-comparison`,2040,1190,compare);
// Enlarged proof of the internal curves, which previously leaked facet fragments.
let detail='<rect width="1200" height="760" fill="#FFFFFF"/>';
detail+=text(36,48,'Исправленный контур / увеличенный фрагмент',28,'#353340',700);
detail+=embed(`${edition}/fenb-violet-${edition}-symbol.svg`,'proof',24,82,1152,650,'182 110 120 210');
save(`FENB-${edition}-contour-detail`,1200,760,detail);
console.log(JSON.stringify({directory:out,assets:6,preview:`FENB-${edition}-preview.png`,comparison:`FENB-${edition}-comparison.png`}));
