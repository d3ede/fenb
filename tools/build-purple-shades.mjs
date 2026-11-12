import fs from 'node:fs';
import path from 'node:path';
import { Resvg } from '../artifacts/logo-renderer/node_modules/@resvg/resvg-js/index.js';

const root=path.resolve('assets/branding');
const out=path.join(root,'purple-shades');
fs.mkdirSync(out,{recursive:true});
const variants=[
 {key:'iris',name:'Ирис',description:'Холодный сине-фиолетовый',hue:-20},
 {key:'amethyst',name:'Аметист',description:'Чистый фиолетовый',hue:0},
 {key:'orchid',name:'Орхидея',description:'Тёплый сиреневый',hue:20}
];
function rotateHue(hex,degrees){
 const c=hex.slice(1).match(/../g).map(v=>parseInt(v,16)/255);
 const max=Math.max(...c),min=Math.min(...c),d=max-min,l=(max+min)/2;
 if(d===0) return hex;
 let h=max===c[0]?((c[1]-c[2])/d+6)%6:max===c[1]?(c[2]-c[0])/d+2:(c[0]-c[1])/d+4;
 h=((h*60+degrees)%360+360)%360;
 const s=d/(1-Math.abs(2*l-1));
 const C=(1-Math.abs(2*l-1))*s,X=C*(1-Math.abs((h/60)%2-1)),m=l-C/2;
 const rgb=h<60?[C,X,0]:h<120?[X,C,0]:h<180?[0,C,X]:h<240?[0,X,C]:h<300?[X,0,C]:[C,0,X];
 return '#'+rgb.map(v=>Math.round((v+m)*255).toString(16).padStart(2,'0')).join('').toUpperCase();
}
const assets=[];
for(const v of variants){
 for(const layout of ['square','horizontal','symbol']){
  let s=fs.readFileSync(path.join(root,`vivid/fenb-violet-vivid-${layout}.svg`),'utf8');
  s=s.replace(/#[0-9a-fA-F]{6}\b/g,h=>rotateHue(h,v.hue));
  s=s.replaceAll('violet-vivid',`violet-${v.key}`).replace(/<title>[\s\S]*?<\/title>/,`<title>FENB / ${v.name} / ${layout}</title>`);
  const name=`fenb-violet-${v.key}-${layout}`;
  fs.writeFileSync(path.join(out,name+'.svg'),s);
  const rendered=new Resvg(s,{fitTo:{mode:'zoom',value:2}}).render();
  fs.writeFileSync(path.join(out,name+'.png'),rendered.asPng());
  if(!s.includes('clip-path=') || /<(image|text|rect)\b/.test(s)) throw new Error(`Unexpected content: ${name}`);
  assets.push({name,width:rendered.width,height:rendered.height});
 }
}
function embed(name,id,x,y,w,h,viewBox){
 let s=fs.readFileSync(path.join(out,name+'.svg'),'utf8').replace(/<title>[\s\S]*?<\/title>/,'');
 s=s.replace(/id="([^"]+)"/g,(_,v)=>`id="${id}-${v}"`).replace(/url\(#([^)]+)\)/g,(_,v)=>`url(#${id}-${v})`);
 return s.replace(/<svg[^>]*>/,`<svg x="${x}" y="${y}" width="${w}" height="${h}" viewBox="${viewBox}" fill="none">`);
}
const txt=(x,y,s,size=22,color='#666371',weight=400)=>`<text x="${x}" y="${y}" font-family="Arial,sans-serif" font-size="${size}" fill="${color}" font-weight="${weight}">${s}</text>`;
let preview='<rect width="1800" height="1230" fill="#E9E8EE"/>';
preview+=txt(48,62,'FENB / три фиолетовых оттенка',34,'#282632',700);
preview+=txt(48,104,'Одинаковый уровень блеска и насыщенности — меняется оттенок');
let sheet='';
for(const [i,v] of variants.entries()){
 const x=48+i*584;
 preview+=txt(x,155,`${String(i+1).padStart(2,'0')} / ${v.name}`,26,'#353340',700);
 preview+=txt(x,187,v.description,20);
 for(let b=0;b<2;b++){
  const y=211+b*493;
  preview+=`<rect x="${x}" y="${y}" width="552" height="466" rx="20" fill="${b===0?'#11111C':'#FFFFFF'}"/>`;
  preview+=txt(x+22,y+31,b===0?'ТЁМНЫЙ ФОН':'СВЕТЛЫЙ ФОН',16,b===0?'#AAA5BD':'#777181',700);
  preview+=embed(`fenb-violet-${v.key}-square`,`preview-${i}-${b}`,x+16,y+29,520,428,'170 170 680 575');
 }
 sheet+=embed(`fenb-violet-${v.key}-square`,`import-square-${i}`,64+i*1124,64,1024,1024,'0 0 1024 1024');
 sheet+=embed(`fenb-violet-${v.key}-horizontal`,`import-horizontal-${i}`,64+i*1124,1180,1024,320,'0 0 1600 500');
 sheet+=embed(`fenb-violet-${v.key}-symbol`,`import-symbol-${i}`,394+i*1124,1620,360,312,'0 0 600 520');
}
const wrap=(w,h,s)=>`<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}" fill="none">${s}</svg>`;
const source=wrap(1800,1230,preview);
fs.writeFileSync(path.join(out,'FENB-purple-shades-preview.svg'),source);
fs.writeFileSync(path.join(out,'FENB-purple-shades-preview.png'),new Resvg(source,{font:{loadSystemFonts:true}}).render().asPng());
fs.writeFileSync(path.join(out,'FENB-purple-shades-Figma-import.svg'),wrap(3480,1996,sheet));
fs.writeFileSync(path.join(out,'README.txt'),'FENB / THREE PURPLE SHADES\n\n01 Iris: cool blue violet\n02 Amethyst: current vivid violet\n03 Orchid: warm lilac violet\n\nAll three use the same finish, saturation and HSL lightness.\n9 transparent SVGs and 9 transparent PNGs at 2x.\nEach shade has square, horizontal and symbol versions.\nOutlined lettering, editable vector paths, corrected clipped facets.\nFigma-import.svg contains all 9 transparent variants.\nPreview backgrounds are for presentation only.\n');
console.log(JSON.stringify({directory:out,variants:variants.map(v=>({name:v.name,mid:rotateHue('#B17EE4',v.hue)})),assets},null,2));
