import fs from 'node:fs';
import path from 'node:path';
import { Resvg } from '../artifacts/logo-renderer/node_modules/@resvg/resvg-js/index.js';

const bloom=process.argv.includes('--bloom');
const edition=bloom?'refined-bloom':'refined';
const out=path.resolve(bloom?'artifacts/FENB-Refined-Bloom':'artifacts/FENB-Refined');
fs.mkdirSync(out,{recursive:true});
const previous=fs.readFileSync('artifacts/FENB-Premium/fenb-premium-stacked-transparent.svg','utf8');
const palettes={
 dark:{background:'#0A0A14',colors:{'#FAF1FF':'#DDD0FC','#D5B9F9':'#B6A0ED','#B78BE8':'#9A7BDD','#F8EDFF':'#E4DAFF','#61409D':'#4C3C96','#A879F0':'#9875E2','#FFF2FF':'#F1E8FF','#FFFFFF':'#F6F0FF'},metal:['#E5DAFF','#D5C5F7','#B7A1EB','#9075D1','#BA9CEB','#C4A6F2'],word:['#E4DBFA','#CFC1EF','#A991D1','#CBB9E9','#A18ACA'],glow:{wide:'.16',mid:'.24',near:'.18',word:'.08',top:'.18',edge:'.35'}},
 light:{background:'#FFFFFF',colors:{'#FAF1FF':'#9C83D2','#D5B9F9':'#8266BF','#B78BE8':'#66509F','#F8EDFF':'#A994DB','#61409D':'#40316E','#A879F0':'#7457B6','#FFF2FF':'#C6B3E9','#FFFFFF':'#D8CCEF'},metal:['#AA93DC','#9B80D3','#8266BF','#624A9D','#9472C6','#7454AE'],word:['#70559D','#61488C','#8F75B8','#765A9E','#5A4186'],glow:{wide:'0',mid:'.035',near:'.045',word:'0',top:'0',edge:'.06'}}
};
if(bloom){
 palettes.dark={background:'#0A0A14',colors:{'#FAF1FF':'#EDDEFC','#D5B9F9':'#CCB5F4','#B78BE8':'#AD84E0','#F8EDFF':'#F3E8FF','#61409D':'#594095','#A879F0':'#A17BEA','#FFF2FF':'#F9EDFF','#FFFFFF':'#FBF6FF'},metal:['#F3E9FF','#E6D5FB','#CBB2F1','#A183D9','#D2AAF0','#DDB6F7'],word:['#F2EAFD','#DED1F2','#B29ED6','#DFD2F4','#B49DDB'],glow:{wide:'.34',mid:'.44',near:'.28',word:'.14',top:'.55',edge:'.7'}};
 palettes.light.glow={wide:'.07',mid:'.10',near:'.08',word:'0',top:'.13',edge:'.22'};
}
function gradient(stops,offsets){return stops.map((c,i)=>`<stop offset="${offsets[i]}" stop-color="${c}"/>`).join('');}
function version(theme){
 const p=palettes[theme];
 let s=previous.replace(/#[0-9a-fA-F]{6}\b/g,h=>p.colors[h.toUpperCase()]??h);
 s=s.replace(/(<linearGradient id="premium-metal"[^>]*>)[\s\S]*?(<\/linearGradient>)/,`$1${gradient(p.metal,[0,.23,.47,.69,.88,1])}$2`);
 s=s.replace(/(<linearGradient id="premium-word"[^>]*>)[\s\S]*?(<\/linearGradient>)/,`$1${gradient(p.word,[0,.25,.53,.7,1])}$2`);
 // Refine rim highlights together with the main palette; keep exact geometry.
 const highlight=theme==='dark'?(bloom?'#F0E4FF':'#DFD5FA'):'#B8A4DC';
 s=s.replaceAll('#FBF3FF',highlight).replaceAll('#F6E7FF',highlight).replaceAll('#F2E9FF',highlight).replaceAll('#F5DEFF',highlight).replaceAll('#FFF4FF',highlight);
 if(theme==='light'){
  s=s.replaceAll('#DDBEFF','#9B7AC5').replaceAll('#D1ADFF','#9C7BC5').replaceAll('#B58AFF','#9671C7').replaceAll('#AE7BEF','#7B58B1').replaceAll('#D3A3FF','#9C7AC5');
 }
 const weights={'glow-wide':p.glow.wide,'glow-mid':p.glow.mid,'glow-near':p.glow.near,'glow-word':p.glow.word};
 for(const [id,opacity] of Object.entries(weights)) s=s.replace(new RegExp(`(<use[^>]*filter="url\\(#${id}\\)"[^>]*opacity=")[^"]+`, 'g'),`$1${opacity}`);
 // Explicit filter bounds keep the top-edge bloom smooth and prevent clipping.
 if(bloom){
  s=s.replace('</defs>',`<filter id="upper-halo" filterUnits="userSpaceOnUse" x="150" y="0" width="470" height="190" color-interpolation-filters="sRGB"><feGaussianBlur stdDeviation="13"/></filter></defs>`);
  s=s.replace(/<ellipse cx="350"[^>]*\/>/,`<ellipse cx="350" cy="78" rx="82" ry="6" fill="#8647FF" filter="url(#upper-halo)" opacity="${p.glow.top}"/>`);
  s=s.replace(/<filter id="glow-edge"[\s\S]*?<\/filter>/,'<filter id="glow-edge" filterUnits="userSpaceOnUse" x="150" y="0" width="470" height="190" color-interpolation-filters="sRGB"><feGaussianBlur stdDeviation="4"/></filter>');
 }else s=s.replace(/<ellipse cx="350"[^>]*\/>/,'');
 s=s.replace(/(<path[^>]*filter="url\(#glow-edge\)"[^>]*opacity=")[^"]+/g,`$1${p.glow.edge}`);
 s=s.replace(/<title>[\s\S]*?<\/title>/,`<title>FENB / ${edition} Iris + Premium / ${theme} theme</title>`);
 s=s.replace('</defs>',`</defs><rect width="1024" height="1024" fill="${p.background}"/>`);
 return s;
}
const outputs={};
for(const theme of ['dark','light']){
 const source=version(theme);
 const name=`fenb-${edition}-${theme}`;
 fs.writeFileSync(path.join(out,name+'.svg'),source);
 const result=new Resvg(source,{fitTo:{mode:'zoom',value:2}}).render();
 fs.writeFileSync(path.join(out,name+'.png'),result.asPng());
 let wordPixels=0;
 const px=result.pixels;
 for(let y=1256;y<1402;y++) for(let x=510;x<1540;x++) {
  const i=(y*result.width+x)*4;
  if(theme==='light'?Math.min(px[i],px[i+1],px[i+2])<180:Math.max(px[i],px[i+1],px[i+2])>100) wordPixels++;
 }
 if(wordPixels<30000) throw new Error(`Missing or faint wordmark: ${theme}`);
 outputs[theme]=source;
}
function embed(source,id,x,y){
 const s=source.replace(/<title>[\s\S]*?<\/title>/,'')
  .replace(/id="([^"]+)"/g,(_,v)=>`id="${id}-${v}"`)
  .replace(/url\(#([^)]+)\)/g,(_,v)=>`url(#${id}-${v})`)
  .replace(/href="#([^"]+)"/g,(_,v)=>`href="#${id}-${v}"`);
 return s.replace(/<svg[^>]*>/,`<svg x="${x}" y="${y}" width="680" height="500" viewBox="200 230 624 520" fill="none">`);
}
const text=(x,y,s,size=24,color='#34333F',weight=400)=>`<text x="${x}" y="${y}" font-family="Arial,sans-serif" font-size="${size}" fill="${color}" font-weight="${weight}">${s}</text>`;
const width=bloom?2376:1600;
let board=`<rect width="${width}" height="1430" fill="#E9E8EE"/>`;
board+=text(48,62,bloom?'FENB / возвращённый фиолетовый bloom':'FENB / Premium → средний вариант с Ирисом',32,'#282632',700);
board+=text(48,104,bloom?'Справа: сияние верхней грани из Premium, светлее металл на тёмном фоне, отдельная контрастная версия для белого.':'Одна композиция: знак + надпись. Для светлой темы — отдельная более контрастная палитра.',21,'#666371');
const headings=bloom?['Исходный / Premium','Предыдущий средний','Обновлённый / bloom']:['Было / Premium','Обновлённый'];
headings.forEach((label,c)=>{board+=text(48+c*776,163,label,26,'#34333F',700);});
for(const [r,theme] of ['dark','light'].entries()){
 const y=194+r*596,p=palettes[theme];
 const sources=bloom?[previous,fs.readFileSync(`artifacts/FENB-Refined/fenb-refined-${theme}.svg`,'utf8'),outputs[theme]]:[previous,outputs[theme]];
 for(let c=0;c<sources.length;c++){
  const x=48+c*776;
  board+=`<rect x="${x}" y="${y}" width="728" height="562" rx="20" fill="${p.background}"/>`;
  board+=text(x+24,y+36,theme==='dark'?'ТЁМНЫЙ ФОН':'СВЕТЛЫЙ ФОН',17,theme==='dark'?'#A8A2BA':'#777181',700);
  board+=embed(sources[c],`compare-${r}-${c}`,x+24,y+46);
 }
}
board+=text(48,1410,bloom?'Одна композиция: знак + FENB. Белая тема сохраняет тёмную надпись и более глубокий фиолетовый.':'Справа: мягче bloom на тёмном фоне; глубже фиолетовый и темнее надпись на белом.',21,'#666371');
const comparison=`<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="1430" viewBox="0 0 ${width} 1430">${board}</svg>`;
fs.writeFileSync(path.join(out,`FENB-${edition}-comparison.svg`),comparison);
fs.writeFileSync(path.join(out,`FENB-${edition}-comparison.png`),new Resvg(comparison,{font:{loadSystemFonts:true}}).render().asPng());
fs.writeFileSync(path.join(out,'README.txt'),'FENB / REFINED\n\nOnly the stacked logo + wordmark composition.\nDark: finish between Premium and Iris, cooler violet, softer bloom.\nLight: separate deeper purple palette and dark metallic wordmark for white backgrounds.\n\nfenb-refined-dark.svg / .png\nfenb-refined-light.svg / .png\nFENB-refined-comparison.png — previous Premium vs refined on both backgrounds.\n\nIndividual PNGs: 2048 x 2048. SVGs: 1024 x 1024, editable paths and SVG filter effects, no external fonts or embedded raster images.\nPrevious Premium assets are preserved separately.\n');
if(bloom)fs.writeFileSync(path.join(out,'README.txt'),'FENB / REFINED BLOOM\n\nOnly stacked logo + wordmark.\nDark: pale violet metal closer to Premium, original broad purple bloom and stronger upper-edge glow.\nLight: dedicated deeper-purple palette and dark wordmark, with a restrained purple halo.\n\nfenb-refined-bloom-dark.svg / .png\nfenb-refined-bloom-light.svg / .png\nFENB-refined-bloom-comparison.png — Premium, previous Refined, updated Bloom on both backgrounds.\n\nIndividual PNGs: 2048 x 2048. SVGs: editable vector paths and filter effects. No embedded raster or external fonts.\n');
console.log(JSON.stringify({folder:out,deliverables:[`fenb-${edition}-dark.svg`,`fenb-${edition}-dark.png`,`fenb-${edition}-light.svg`,`fenb-${edition}-light.png`,`FENB-${edition}-comparison.png`]}));
