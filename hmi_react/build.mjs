// build.mjs — Compila el HMI a UN solo HTML (JS y CSS adentro) y lo deja comprimido
// en ../firmware_esp32/main/hmi.html.gz para que la ESP32 lo sirva en "/".
//
//   npm run build   -> dist/index.html + ../firmware_esp32/main/hmi.html.gz
//   npm run dev     -> http://localhost:5173 (recompila al guardar). Con el PC conectado
//                      al WiFi del carro, habla con 192.168.4.1. Sin carro: abre
//                      http://localhost:5173/?demo para ver el HMI con datos simulados.
import * as esbuild from 'esbuild';
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const dev = process.argv.includes('--dev');
const raiz = path.dirname(fileURLToPath(import.meta.url));   // fileURLToPath: en Windows pathname da "/C:/..."
const dist = path.join(raiz, 'dist');
const destinoFirmware = path.join(raiz, '..', 'firmware_esp32', 'main', 'hmi.html.gz');

const opciones = {
  entryPoints: [path.join(raiz, 'src', 'main.jsx')],
  bundle: true,
  minify: !dev,
  write: false,
  outdir: dist,
  jsx: 'automatic',
  target: ['es2020', 'safari15'],
  loader: { '.css': 'css' },
  define: { 'process.env.NODE_ENV': dev ? '"development"' : '"production"' },
  legalComments: 'none',
};

function armarHtml(resultado) {
  let js = '', css = '';
  for (const f of resultado.outputFiles) {
    if (f.path.endsWith('.js')) js = f.text;
    if (f.path.endsWith('.css')) css = f.text;
  }
  const plantilla = fs.readFileSync(path.join(raiz, 'src', 'index.html'), 'utf8');
  // Funciones de reemplazo: así un "$" dentro del código no se interpreta como patrón
  const html = plantilla
    .replace('/*CSS*/', () => css)
    .replace('/*JS*/', () => js.replace(/<\/script/g, '<\\/script'));
  fs.mkdirSync(dist, { recursive: true });
  fs.writeFileSync(path.join(dist, 'index.html'), html);
  return html;
}

if (dev) {
  const ctx = await esbuild.context({
    ...opciones,
    plugins: [{
      name: 'html',
      setup(b) {
        b.onEnd((r) => {
          if (r.errors.length) return;
          armarHtml(r);
          console.log('HMI recompilado', new Date().toLocaleTimeString());
        });
      },
    }],
  });
  await ctx.watch();
  const { port } = await ctx.serve({ servedir: dist, port: 5173 });
  console.log(`HMI en http://localhost:${port}  (datos simulados: http://localhost:${port}/?demo)`);
} else {
  const r = await esbuild.build(opciones);
  const html = armarHtml(r);
  const gz = zlib.gzipSync(html, { level: 9 });
  fs.writeFileSync(destinoFirmware, gz);
  console.log(`dist/index.html: ${(html.length / 1024).toFixed(1)} KB`);
  console.log(`hmi.html.gz -> firmware: ${(gz.length / 1024).toFixed(1)} KB`);
}
