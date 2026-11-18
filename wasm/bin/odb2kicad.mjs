#!/usr/bin/env node

import { readFileSync, writeFileSync } from 'fs';
import { basename } from 'path';
import { convertOdbArchive } from '../dist/index.mjs';

const args = process.argv.slice(2);

if (args.length === 0 || args.includes('--help') || args.includes('-h')) {
  console.error(`Usage: odb2kicad <input.tgz|.zip|.tar> [-o output.kicad_pcb] [--step <name>]

Convert an ODB++ archive to KiCad .kicad_pcb format.

Options:
  -o, --output <file>  Write output to file (default: stdout)
  --step <name>        Convert this step of a multi-step job (default: the first)
  -h, --help           Show this help message`);
  process.exit(args.includes('--help') || args.includes('-h') ? 0 : 1);
}

let inputPath = null;
let outputPath = null;
let step = '';

for (let i = 0; i < args.length; i++) {
  if (args[i] === '-o' || args[i] === '--output') {
    outputPath = args[++i];
    if (!outputPath) {
      console.error('Error: -o requires a filename');
      process.exit(1);
    }
  } else if (args[i] === '--step') {
    step = args[++i] || '';
  } else if (!inputPath) {
    inputPath = args[i];
  } else {
    console.error(`Error: unexpected argument "${args[i]}"`);
    process.exit(1);
  }
}

if (!inputPath) {
  console.error('Error: no input file specified');
  process.exit(1);
}

const data = readFileSync(inputPath);
const result = await convertOdbArchive(data, step ? { step } : {});

if (!result.success) {
  console.error(`Error: ${result.error}`);
  process.exit(1);
}

for (const line of result.report || []) {
  if (line.startsWith('warning:')) console.error('Warning:' + line.slice(8));
}
if (!step && result.steps && result.steps.length > 1) {
  console.error(`Note: the job has ${result.steps.length} steps (${result.steps.join(', ')}); this is ${result.step}. Use --step <name> for another.`);
}

if (outputPath) {
  writeFileSync(outputPath, result.kicadPcb);
  console.error(`Wrote ${outputPath}`);
} else {
  process.stdout.write(result.kicadPcb);
}
