#!/usr/bin/env node
/**
 * Local development server for KiCad DRC WASM demo
 *
 * Serves the public/ directory with COOP/COEP headers required for SharedArrayBuffer.
 *
 * Usage: node serve-local.mjs [port]
 */

import { createServer } from 'http';
import { readFile, stat } from 'fs/promises';
import { join, extname } from 'path';
import { fileURLToPath } from 'url';
import { dirname } from 'path';

const __dirname = dirname(fileURLToPath(import.meta.url));
const PUBLIC_DIR = join(__dirname, 'public');
const PORT = parseInt(process.argv[2]) || 8080;

const MIME_TYPES = {
    '.html': 'text/html',
    '.js': 'application/javascript',
    '.mjs': 'application/javascript',
    '.wasm': 'application/wasm',
    '.css': 'text/css',
    '.json': 'application/json',
    '.png': 'image/png',
    '.svg': 'image/svg+xml',
    '.ico': 'image/x-icon',
    '.kicad_pcb': 'text/plain',
};

const server = createServer(async (req, res) => {
    // Add COOP/COEP headers for SharedArrayBuffer support
    res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
    res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');

    let url = req.url.split('?')[0];
    if (url === '/') url = '/index.html';

    const filePath = join(PUBLIC_DIR, url);

    // Security: prevent directory traversal
    if (!filePath.startsWith(PUBLIC_DIR)) {
        res.writeHead(403);
        res.end('Forbidden');
        return;
    }

    try {
        const stats = await stat(filePath);
        if (!stats.isFile()) {
            throw new Error('Not a file');
        }

        const ext = extname(filePath).toLowerCase();
        const contentType = MIME_TYPES[ext] || 'application/octet-stream';

        const content = await readFile(filePath);
        res.writeHead(200, { 'Content-Type': contentType });
        res.end(content);
    } catch (err) {
        if (err.code === 'ENOENT') {
            res.writeHead(404);
            res.end(`Not found: ${url}`);
        } else {
            res.writeHead(500);
            res.end('Internal server error');
        }
    }
});

server.listen(PORT, () => {
    console.log(`\n  KiCad DRC WASM Demo Server`);
    console.log(`  ─────────────────────────`);
    console.log(`  Local:   http://localhost:${PORT}/`);
    console.log(`  Network: http://0.0.0.0:${PORT}/`);
    console.log(`\n  COOP/COEP headers enabled for SharedArrayBuffer\n`);
    console.log(`  Press Ctrl+C to stop\n`);
});
