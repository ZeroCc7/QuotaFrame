const fs = require('node:fs');
const path = require('node:path');
const { Resvg } = require('@resvg/resvg-js');
const root = __dirname;
let svg = fs.readFileSync(path.join(root, 'layout.svg'), 'utf8');
svg = svg.replaceAll('href="plate.png"', `href="data:image/png;base64,${fs.readFileSync(path.join(root, 'plate.png')).toString('base64')}"`);
svg = svg.replace('<defs>', '<style>text{font-family:Oxanium;font-weight:800}</style><defs>');
const result = new Resvg(svg, {background:'#000000',font:{fontFiles:[path.join(root,'Oxanium-ExtraBold.ttf')],loadSystemFonts:false}}).render();
fs.writeFileSync(path.join(root,'background.png'),result.asPng());
const rgba = result.pixels;
const rgb565 = Buffer.alloc(result.width * result.height * 2);
for (let i = 0, p = 0; i < rgba.length; i += 4, p += 2) {
  const value = ((rgba[i] >> 3) << 11) | ((rgba[i + 1] >> 2) << 5) | (rgba[i + 2] >> 3);
  rgb565.writeUInt16LE(value, p);
}
fs.writeFileSync(path.join(root, 'background.rgb565'), rgb565);
