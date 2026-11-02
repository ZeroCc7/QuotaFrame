const fs=require('node:fs'),path=require('node:path');
const {Resvg}=require('@resvg/resvg-js');
const svg=fs.readFileSync(path.join(__dirname,'eva-clock.svg'),'utf8');
const result=new Resvg(svg,{background:'#000',font:{fontFiles:[path.join(__dirname,'../Oxanium-ExtraBold.ttf')],loadSystemFonts:false}}).render();
fs.writeFileSync(path.join(__dirname,'eva-clock.png'),result.asPng());
const rgba=result.pixels,b=Buffer.alloc(result.width*result.height*2);
for(let i=0,p=0;i<rgba.length;i+=4,p+=2)b.writeUInt16LE(((rgba[i]>>3)<<11)|((rgba[i+1]>>2)<<5)|(rgba[i+2]>>3),p);
fs.writeFileSync(path.join(__dirname,'eva_clock.rgb565'),b);
