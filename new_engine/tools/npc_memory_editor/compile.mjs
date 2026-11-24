#!/usr/bin/env node
import {readFile,writeFile} from 'node:fs/promises';
import {compile,upgrade} from './dist/memory.mjs';
const [source,out]=process.argv.slice(2);
if(!source||!out){console.error('Usage: node compile.mjs PROJECT.vwmemory.json OUTPUT.vwmemory');process.exit(2);}
try{const bytes=await readFile(source);if(bytes.length>2*1024*1024)throw new Error('Project exceeds2MiB');const doc=upgrade(JSON.parse(bytes));await writeFile(out,compile(doc),{flag:'wx'});console.log('Validated engine export written: '+out);}catch(e){console.error(e.message);process.exitCode=1;}
