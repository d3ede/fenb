import fs from 'node:fs';
import path from 'node:path';
import { Resvg } from '../artifacts/logo-renderer/node_modules/@resvg/resvg-js/index.js';

const out=path.resolve('artifacts/FENB-Approved-Logo-Kit');
fs.mkdirSync(out,{recursive:true});
const themes=[
 {key:'dark',folder:'Dark',name:'Для тёмной темы',bg:'#0A0A14',backgroundName:'on-dark'},
 {key:'light',folder:'Light',name:'Для светлой темы',bg:'#FFFFFF',backgroundName:'on-white'}
];
const layouts=[
 {key:'stacked',name:'Знак + надпись снизу',w:1024,h:1024,mark:'translate(280 214) scale(.82)',word:'translate(254.825 628) scale(.81)'},
 {key:'symbol',name:'Квадратный знак без надписи',w:1024,h:1024,mark:null,word:null},
 {key:'horizontal',name:'Горизонтальный логотип',w:1600,h:500,mark:'translate(28 20) scale(.82)',word:'translate(575 195) scale(1.38)'}
];
function svg(w,h,defs,body,title){return `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}" fill="none"><title>${title}</title><defs>${defs}</defs>${body}</svg>`;}
function bounds(render){
 const p=render.pixels;let left=render.width,top=render.height,right=-1,bottom=-1;
 for(let y=0;y<render.height;y++)for(let x=0;x<render.width;x++)if(p[(y*render.width+x)*4+3]>2){left=Math.min(left,x);top=Math.min(top,y);right=Math.max(right,x);bottom=Math.max(bottom,y);}
 if(right<left)throw new Error('Empty symbol');
 return {x:left,y:top,w:right-left+1,h:bottom-top+1};
}
const assets=[];
for(const theme of themes){
 const source=fs.readFileSync(`artifacts/FENB-Refined-Bloom/fenb-refined-bloom-${theme.key}.svg`,'utf8');
 const defs=source.match(/<defs>([\s\S]*?)<\/defs>/)?.[1];
 const mark=source.match(/<g transform="translate\(280 214\) scale\(\.82\)">([\s\S]*?)<\/g>/)?.[1];
 const word=source.match(/<g transform="translate\(254\.825 628\) scale\(\.81\)">([\s\S]*?)<\/g>/)?.[1];
 if(!defs||!mark||!word)throw new Error(`Cannot extract approved geometry: ${theme.key}`);
 const measurement=new Resvg(svg(1024,1024,defs,`<g transform="translate(200 150)">${mark}</g>`,'measure')).render();
 const b=bounds(measurement),scale=780/Math.max(b.w,b.h);
 const cx=b.x-200+b.w/2,cy=b.y-150+b.h/2;
 const symbolTransform=`translate(${(512-cx*scale).toFixed(5)} ${(512-cy*scale).toFixed(5)}) scale(${scale.toFixed(6)})`;
 for(const layout of layouts){
  const art=`<g id="logo-mark" transform="${layout.mark??symbolTransform}">${mark}</g>`+(layout.word?`<g id="logo-wordmark" transform="${layout.word}">${word}</g>`:'');
  for(const transparent of [false,true]){
   const suffix=transparent?'transparent':theme.backgroundName;
   const name=`fenb-${theme.key}-${layout.key}-${suffix}`;
   const bg=transparent?'':`<rect id="background" width="${layout.w}" height="${layout.h}" fill="${theme.bg}"/>`;
   const result=svg(layout.w,layout.h,defs,bg+art,`FENB / approved purple bloom / ${theme.name} / ${layout.name} / ${transparent?'transparent':theme.bg}`);
   const svgDir=path.join(out,'SVG',theme.folder),pngDir=path.join(out,'PNG',theme.folder);
   fs.mkdirSync(svgDir,{recursive:true});fs.mkdirSync(pngDir,{recursive:true});
   const svgFile=path.join(svgDir,name+'.svg'),pngFile=path.join(pngDir,name+'.png');
   fs.writeFileSync(svgFile,result);
   const render=new Resvg(result,{fitTo:{mode:'zoom',value:2}}).render();
   fs.writeFileSync(pngFile,render.asPng());
   const pixel=Array.from(render.pixels.subarray(0,4));
   const expected=transparent?[0,0,0,0]:theme.key==='dark'?[10,10,20,255]:[255,255,255,255];
   if(pixel.some((v,i)=>v!==expected[i]))throw new Error(`Invalid background: ${name}`);
   if(layout.key==='symbol' && render.width!==render.height)throw new Error(`Non-square symbol: ${name}`);
   if(/<(image|text)\b/.test(result)||!result.includes('clip-path='))throw new Error(`Invalid vector source: ${name}`);
   assets.push({theme:theme.key,layout:layout.key,transparent,svgFile,pngFile,w:layout.w,h:layout.h});
  }
 }
 console.log(`Exported ${theme.key}: 6 SVG + 6 PNG`);
}
function embed(asset,id,x,y,w,h,vb){
 let s=fs.readFileSync(asset.svgFile,'utf8').replace(/<title>[\s\S]*?<\/title>/,'');
 s=s.replace(/id="([^"]+)"/g,(_,v)=>`id="${id}-${v}"`).replace(/url\(#([^)]+)\)/g,(_,v)=>`url(#${id}-${v})`).replace(/href="#([^"]+)"/g,(_,v)=>`href="#${id}-${v}"`);
 return s.replace(/<svg[^>]*>/,`<svg x="${x}" y="${y}" width="${w}" height="${h}" viewBox="${vb}" fill="none">`);
}
const txt=(x,y,s,size=24,color='#34333F',weight=400)=>`<text x="${x}" y="${y}" font-family="Arial,sans-serif" font-size="${size}" fill="${color}" font-weight="${weight}">${s}</text>`;
const columns=[
 {theme:'dark',transparent:false,label:'Тёмная тема',fill:'#0A0A14'},
 {theme:'light',transparent:false,label:'Светлая тема',fill:'#FFFFFF'},
 {theme:'dark',transparent:true,label:'Прозрачный / для тёмной темы',fill:'url(#checker-dark)'},
 {theme:'light',transparent:true,label:'Прозрачный / для светлой темы',fill:'url(#checker-light)'}
];
const W=2880,H=1510,left=48,gap=24,cw=678,ch=392;
let board=`<rect width="${W}" height="${H}" fill="#E9E8EE"/>`;
board+=txt(48,62,'FENB / утверждённый фиолетовый bloom',36,'#282632',700);
board+=txt(48,104,'12 вариантов: три композиции · две палитры · фон или прозрачность. Каждый сохранён в SVG и PNG.',23,'#666371');
columns.forEach((c,i)=>{board+=txt(left+i*(cw+gap),162,c.label,22,'#34333F',700);});
for(const [r,layout] of layouts.entries()){
 const y=193+r*(ch+24);
 for(const [c,column] of columns.entries()){
  const x=left+c*(cw+gap);
  board+=`<rect x="${x}" y="${y}" width="${cw}" height="${ch}" rx="18" fill="${column.fill}"/>`;
  board+=txt(x+22,y+33,layout.name,19,column.theme==='dark'?'#AAA5BD':'#777181',700);
  const a=assets.find(a=>a.theme===column.theme&&a.layout===layout.key&&a.transparent===column.transparent);
  const vb=layout.key==='stacked'?'170 190 684 590':layout.key==='symbol'?'0 0 1024 1024':'0 0 1600 500';
  const [pw,ph]=layout.key==='horizontal'?[626,196]:layout.key==='symbol'?[368,320]:[540,320];
  board+=embed(a,`overview-${r}-${c}`,x+(cw-pw)/2,y+53+(ch-64-ph)/2,pw,ph,vb);
 }
}
board+=txt(48,1480,'Шахматная подложка — только в обзоре. Прозрачные SVG и PNG сохраняют настоящее свечение и альфа-канал.',22,'#666371');
const checker=(id,a,b)=>`<pattern id="${id}" width="32" height="32" patternUnits="userSpaceOnUse"><rect width="32" height="32" fill="${a}"/><path d="M0 0H16V16H0ZM16 16H32V32H16Z" fill="${b}"/></pattern>`;
const overview=svg(W,H,checker('checker-dark','#20202E','#2B2A3D')+checker('checker-light','#F7F6FA','#E6E3EE'),board,'FENB / all approved variants');
fs.writeFileSync(path.join(out,'00-FENB-all-variants.png'),new Resvg(overview,{font:{loadSystemFonts:true}}).render().asPng());
fs.writeFileSync(path.join(out,'README.txt'),`FENB — УТВЕРЖДЁННЫЙ ВАРИАНТ СО СВЕЧЕНИЕМ\n\nФинальная палитра и bloom взяты из одобренной версии Refined Bloom.\n\nSVG/Dark и PNG/Dark — светлый фиолетовый металл и яркое свечение для тёмной темы.\nSVG/Light и PNG/Light — более глубокий фиолетовый, контрастная надпись и мягкое свечение для светлой темы.\n\nВ каждой теме:\nstacked — знак + надпись снизу, SVG 1024 × 1024, PNG 2048 × 2048.\nsymbol — квадратный знак без надписи, SVG 1024 × 1024, PNG 2048 × 2048.\nhorizontal — знак + надпись справа, SVG 1600 × 500, PNG 3200 × 1000.\n\nКаждая композиция: с тематическим фоном и на прозрачном фоне.\non-dark — тёмный фон #0A0A14.\non-white — белый фон #FFFFFF.\ntransparent — настоящий альфа-канал, включая мягкий bloom.\n\n00-FENB-all-variants.png — общий обзор всех 12 вариантов.\n\nВсего: 12 SVG и 12 PNG. В SVG нет встроенных растров или внешних шрифтов; буквы в контурах, эффекты заданы SVG-фильтрами. PNG точно сохраняют отрендеренный bloom; поддержка SVG-фильтров может различаться между редакторами. Исправленный внутренний контур F сохранён.\n`);
fs.writeFileSync(path.join(out,'manifest.json'),JSON.stringify({version:'Approved Refined Bloom',assets:assets.map(a=>({theme:a.theme,composition:a.layout,transparent:a.transparent,svg:path.relative(out,a.svgFile),png:path.relative(out,a.pngFile),svgSize:[a.w,a.h],pngSize:[a.w*2,a.h*2]}))},null,2));
console.log(JSON.stringify({folder:out,svgCount:assets.length,pngCount:assets.length,overview:`${W}x${H}`,squareSymbols:assets.filter(a=>a.layout==='symbol').length}));
