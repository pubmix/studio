import test from 'node:test';import assert from 'node:assert/strict';import {ROLES,restore,validateBuffers,defaults} from './model.js';
test('canonical four-lane order',()=>assert.deepEqual(ROLES,['VOCALS','MELODY','BASS','RHYTHM']));
test('corrupt saved state recovers',()=>assert.deepEqual(restore('{'),defaults()));
test('saved controls cannot exceed safe ranges',()=>{const s=restore(JSON.stringify({levels:[-1,2,'bad',.5],bpm:999,theme:'unknown'}));assert.deepEqual(s.levels,[0,1,0,.5]);assert.equal(s.bpm,300);assert.equal(s.theme,'Dark');});
test('aligned stems accepted',()=>assert.equal(validateBuffers(Array.from({length:4},()=>({length:44100,sampleRate:44100,duration:1}))),1));
test('missing or unequal stems rejected',()=>{assert.throws(()=>validateBuffers([]));assert.throws(()=>validateBuffers([{length:4,sampleRate:1},{length:3,sampleRate:1},{length:4,sampleRate:1},{length:4,sampleRate:1}]));});
