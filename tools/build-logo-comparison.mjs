import fs from 'node:fs';
import path from 'node:path';
import { Resvg } from '../artifacts/logo-renderer/node_modules/@resvg/resvg-js/index.js';

const root=path.resolve('assets/branding');
const W=2400,H=1260;
const text=(x,y,s,size=24,color='#30323C',weight=400)=>`<text x="${x}" y="${y}" font-family="Arial,sans-serif" font-size="${size}" fill="${color}" font-weight="${weight}">${s}</text>`;
function logo(file,prefix,x,y){
 let s=fs.readFileSync(path.join(root,file),'utf8');
 s=s.replace(/<title>[\s\S]*?<\/title>/,'').replace(/id="([^"]+)"/g,(_,id)=>`id="${prefix}-${id}"`).replace(/url\(#([^)]+)\)/g,(_,id)=>`url(#${prefix}-${id})`);
 s=s.replace(/<svg[^>]*>/,`<svg x="${x}" y="${y}" width="520" height="438" viewBox="170 170 680 575" fill="none">`);
 return s;
}
let body=`<rect width="${W}" height="${H}" fill="#E9E8EE"/>`;
body+=text(48,66,'FENB / сравнение вариантов 1 и 3',34,'#242632',700);
body+=text(48,107,'Исходные и новые версии на одинаковых тёмном и белом фонах.',22,'#676773');
const columns=[['Исходный · тёмная тема','#11111C'],['Исходный · светлая тема','#FFFFFF'],['Новый · светлая тема','#FFFFFF'],['Новый · тёмная тема','#11111C']];
for(let c=0;c<columns.length;c++) body+=text(48+c*584,163,columns[c][0],23,'#41424E',700);
for(const [r,key] of ['violet','aurora'].entries()) {
 const y=194+r*516;
 for(let c=0;c<columns.length;c++) {
  const x=48+c*584;
  body+=`<rect x="${x}" y="${y}" width="552" height="484" rx="20" fill="${columns[c][1]}"/>`;
  body+=text(x+24,y+38,`${r===0?'01 / VIOLET':'03 / AURORA'}${c>=2?' / DAYLIGHT':''}`,17,columns[c][1]==='#11111C'?'#A6A3B8':'#777484',700);
  const file=c>=2?`daylight/fenb-${key}-daylight-square.svg`:`fenb-${key}-square.svg`;
  body+=logo(file,`r${r}c${c}`,x+16,y+42);
 }
}
body+=text(48,1240,'DAYLIGHT: приглушённые блики · более глубокие цвета · тёмная надпись · прозрачные исходники',21,'#676773');
const source=`<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}" viewBox="0 0 ${W} ${H}">${body}</svg>`;
const svgFile=path.join(root,'FENB-original-vs-daylight.svg');
const pngFile=path.join(root,'FENB-original-vs-daylight.png');
fs.writeFileSync(svgFile,source);
const rendered=new Resvg(source,{font:{loadSystemFonts:true}}).render();
fs.writeFileSync(pngFile,rendered.asPng());
console.log(JSON.stringify({svgFile,pngFile,width:rendered.width,height:rendered.height}));
