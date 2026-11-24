#!/usr/bin/env node
import {readFile,writeFile} from 'node:fs/promises';
import {parseProject,compile,PROJECT_BYTE_LIMIT} from './dist/story.mjs';
const [source,destination]=process.argv.slice(2);
if(!source||!destination){console.error('Usage: node compile.mjs PROJECT.vwstory.json NEW_OUTPUT.gameplay');process.exit(2);}
try{const sourceBytes=await readFile(source);if(sourceBytes.length>PROJECT_BYTE_LIMIT)throw new Error('Story project exceeds 16 MiB.');const doc=parseProject(sourceBytes.toString('utf8'));await writeFile(destination,compile(doc),{flag:'wx'});console.log('Validated engine gameplay export written: '+destination);}catch(error){console.error(error.message);process.exitCode=1;}
