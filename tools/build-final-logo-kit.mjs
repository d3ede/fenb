import fs from 'node:fs';
import path from 'node:path';
import { Resvg } from '../artifacts/logo-renderer/node_modules/@resvg/resvg-js/index.js';

const root=path.resolve('assets/branding');
const out=path.resolve('artifacts/FENB-Logo-Kit');
fs.mkdirSync(out,{recursive:true});
const brands=[
 {key:'iris',name:'Ирис',folder:'Iris',source:layout=>`purple-shades/fenb-violet-iris-${layout}.svg`},
 {key:'aurora',name:'Аврора',folder:'Aurora',source:layout=>`vivid/fenb-aurora-vivid-${layout}.svg`}
];
const backgrounds=[{key:'white',color:'#FFFFFF',name:'Белый фон'},{key:'dark',color:'#11111C',name:'Тёмный фон'},{key:'transparent',color:null,name:'Прозрачный фон'}];
const layouts=[{key:'stacked',source:'square',name:'С надписью',w:1024,h:1024},{key:'symbol',source:'symbol',name:'Знак без надписи',w:1024,h:1024},{key:'horizontal',source:'horizontal',name:'Горизонтальный',w:1600,h:500}];
function parse(s){
 const [,head,defs,body]=s.match(/^<svg\b([^>]*)>[\s\S]*?<defs>([\s\S]*?)<\/defs>([\s\S]*)<\/svg>\s*$/);
 return {head,defs,body};
}
function svg(w,h,defs,body,title){return `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}" fill="none"><title>${title}</title><defs>${defs}</defs>${body}</svg>`;}
function alphaBounds(rendered){
 const pixels=rendered.pixels;let l=rendered.width,t=rendered.height,r=-1,b=-1;
 for(let y=0;y<rendered.height;y++) for(let x=0;x<rendered.width;x++) if(pixels[(y*rendered.width+x)*4+3]>4){l=Math.min(l,x);r=Math.max(r,x);t=Math.min(t,y);b=Math.max(b,y);}
 if(r<l) throw new Error('Empty artwork');
 return {x:l,y:t,w:r-l+1,h:b-t+1};
}
const assets=[];
for(const brand of brands){
 const dir=path.join(out,'SVG',brand.folder);fs.mkdirSync(dir,{recursive:true});
 for(const layout of layouts){
  const source=fs.readFileSync(path.join(root,brand.source(layout.source)),'utf8');
  const parsed=parse(source);let art=parsed.body;
  if(layout.key==='symbol'){
   const bounds=alphaBounds(new Resvg(source).render());
   const scale=800/Math.max(bounds.w,bounds.h);
   const tx=(1024-bounds.w*scale)/2-bounds.x*scale,ty=(1024-bounds.h*scale)/2-bounds.y*scale;
   art=`<g transform="translate(${tx.toFixed(6)} ${ty.toFixed(6)}) scale(${scale.toFixed(6)})">${art}</g>`;
  }
  for(const bg of backgrounds){
   const background=bg.color?`<rect id="background-${bg.key}" width="${layout.w}" height="${layout.h}" fill="${bg.color}"/>`:'';
   const result=svg(layout.w,layout.h,parsed.defs,background+art,`FENB / ${brand.name} / ${layout.name} / ${bg.name}`);
   const file=path.join(dir,`fenb-${brand.key}-${layout.key}-${bg.key}.svg`);
   fs.writeFileSync(file,result);
   const render=new Resvg(result).render();
   const pixel=Array.from(render.pixels.subarray(0,4));
   const expected=bg.color?(bg.key==='white'?[255,255,255,255]:[17,17,28,255]):[0,0,0,0];
   if(pixel.some((v,i)=>v!==expected[i])) throw new Error(`Unexpected background: ${file}`);
   if(/<(image|text)\b/.test(result) || !result.includes('clip-path=')) throw new Error(`Invalid vector export: ${file}`);
   if(render.width!==layout.w || render.height!==layout.h) throw new Error(`Invalid dimensions: ${file}`);
   assets.push({brand:brand.key,layout:layout.key,background:bg.key,file,width:render.width,height:render.height});
  }
 }
}
function embed(asset,id,x,y,w,h,viewBox){
 let source=fs.readFileSync(asset.file,'utf8').replace(/<title>[\s\S]*?<\/title>/,'');
 // Each asset already contains the exact requested background, except transparent assets.
 source=source.replace(/id="([^"]+)"/g,(_,v)=>`id="${id}-${v}"`).replace(/url\(#([^)]+)\)/g,(_,v)=>`url(#${id}-${v})`);
 return source.replace(/<svg[^>]*>/,`<svg x="${x}" y="${y}" width="${w}" height="${h}" viewBox="${viewBox}" fill="none">`);
}
const txt=(x,y,s,size=24,color='#34333E',weight=400)=>`<text x="${x}" y="${y}" font-family="Arial,sans-serif" font-size="${size}" fill="${color}" font-weight="${weight}">${s}</text>`;
const W=2200,H=2200,cardW=680,cardH=290,gap=32,left=48;
let board=`<rect width="${W}" height="${H}" fill="#E9E8EE"/>`;
board+=txt(48,62,'FENB / Ирис + Аврора',36,'#282632',700);
board+=txt(48,104,'18 SVG: два цвета × три композиции × три фона',23,'#666371');
for(let c=0;c<3;c++) board+=txt(left+c*(cardW+gap),158,backgrounds[c].name,25,'#34333E',700);
for(const [bi,brand] of brands.entries()){
 const start=204+bi*978;
 board+=txt(left,start,`${String(bi+1).padStart(2,'0')} / ${brand.name}`,29,'#34333E',700);
 for(const [li,layout] of layouts.entries()){
  const y=start+24+li*(cardH+14);
  for(const [ci,bg] of backgrounds.entries()){
   const x=left+ci*(cardW+gap),caption=bg.key==='dark'?'#AAA5BD':'#777181';
   board+=`<rect x="${x}" y="${y}" width="${cardW}" height="${cardH}" rx="16" fill="${bg.color??'url(#checker)'}"/>`;
   board+=txt(x+20,y+31,layout.name,17,caption,700);
   const asset=assets.find(a=>a.brand===brand.key && a.layout===layout.key && a.background===bg.key);
   // Tight preview crops make all three compositions readable. Export canvases retain full padding.
   const vb=layout.key==='stacked'?'170 170 680 575':layout.key==='symbol'?'90 170 844 680':'0 0 1600 500';
   const [pw,ph]=layout.key==='horizontal'?[624,195]:[340,234];
   const px=x+(cardW-pw)/2,py=y+45+(cardH-55-ph)/2;
   board+=embed(asset,`preview-${bi}-${li}-${ci}`,px,py,pw,ph,vb);
  }
 }
}
board+=txt(48,2180,'Шахматная подложка показана только в обзоре; SVG transparent не содержат фона.',21,'#666371');
const checker='<pattern id="checker" width="32" height="32" patternUnits="userSpaceOnUse"><rect width="32" height="32" fill="#F8F7FB"/><path d="M0 0H16V16H0ZM16 16H32V32H16Z" fill="#E7E4EE"/></pattern>';
const overview=svg(W,H,checker,board,'FENB / all 18 final logo variants');
fs.writeFileSync(path.join(out,'FENB-all-variants.png'),new Resvg(overview,{font:{loadSystemFonts:true}}).render().asPng());
fs.writeFileSync(path.join(out,'README.txt'),`FENB — ИРИС + АВРОРА\n\nВыбраны: Ирис (первый из трёх фиолетовых оттенков) и Аврора из последней яркой серии Vivid.\n\nSVG/Iris — 9 SVG фиолетового логотипа.\nSVG/Aurora — 9 SVG бирюзового логотипа.\nFENB-all-variants.png — общий обзор всех 18 вариантов.\n\nКомпозиции:\nstacked — знак + надпись снизу, 1024 × 1024.\nsymbol — только знак, квадрат 1024 × 1024.\nhorizontal — знак + надпись справа, 1600 × 500.\n\nФоны:\nwhite — белый #FFFFFF.\ndark — тёмный #11111C.\ntransparent — прозрачный, без фонового прямоугольника.\n\nВсе SVG содержат редактируемые векторные пути и градиенты. Надпись переведена в контуры. Внешние шрифты и растровые изображения не используются.\nИсправление внутреннего контура F сохранено.\nШахматная подложка существует только на общем PNG, а не в прозрачных SVG.\n`);
console.log(JSON.stringify({folder:out,svgCount:assets.length,squareSymbolCount:assets.filter(a=>a.layout==='symbol' && a.width===a.height).length,backgroundChecks:assets.length,overview:`${W}x${H}`,assets:assets.map(a=>path.relative(out,a.file))},null,2));
