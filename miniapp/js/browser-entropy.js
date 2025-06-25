// browser-entropy.js – PCQNG-compatible browser sampler
// Scott Wilber doctrine: preserve raw jitter structure; zero whitening.
//
// Collect 1 024 consecutive performance.now() deltas (~20 ms at 60 Hz)
// per epoch and participate in commit/reveal protocol.
// Epoch timeline: 10 s → 1 s beacon nonce, 5 s commit, 4 s reveal.
//
// Dependencies: Web Crypto, WebAuthn, web3.storage.
// Canon constants: SHA3-256 hashing, byte-wise interleave handled server-side.

import { Web3Storage } from 'https://cdn.jsdelivr.net/npm/web3.storage@5.1.0/dist/bundle.esm.min.js';

// -------------------------- Config --------------------------
const EPOCH_LENGTH_MS = 10_000;
const COMMIT_WINDOW_MS = 5_000;   // after first-second nonce
const DELTA_BATCH = 1024;
const STORAGE_TOKEN = window.env?.WEB3_STORAGE_TOKEN || '';

// The backend API base (served by FastAPI miniapp)
const API = window.chromancyAPI ?? new window.ChromomancyAPI('');

// ------------------------ Utilities -------------------------
async function sha3(bytes) {
  const digest = await crypto.subtle.digest('SHA-3-256', bytes);
  return Array.from(new Uint8Array(digest)).map(b => b.toString(16).padStart(2, '0')).join('');
}

function nowMs() { return performance.now(); }

function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

function currentEpoch() {
  return Math.floor(Date.now() / EPOCH_LENGTH_MS);
}

// ------------------ Jitter Sampling Logic -------------------
async function sampleDeltas(count = DELTA_BATCH) {
  const deltas = new Uint32Array(count);
  let prev = nowMs();
  for (let i = 0; i < count; i++) {
    const cur = nowMs();
    deltas[i] = Math.floor((cur - prev) * 1e6); // µs diff for high dynamic range
    prev = cur;
    // Yield to event loop minimally
    await new Promise(r => requestAnimationFrame(r));
  }
  return deltas;
}

// ------------------ Commit / Reveal Cycle -------------------
export async function startEntropySession({ userId, enabled = true }) {
  if (!enabled) return;
  if (!STORAGE_TOKEN) {
    console.warn('No web3.storage token; falling back to server entropy');
    return;
  }
  const w3 = new Web3Storage({ token: STORAGE_TOKEN });

  while (true) {
    const epoch = currentEpoch();

    // Wait for beacon nonce (assume server exposes latest nonce)
    const nonceObj = await API.request('/beacon/nonce');
    const { nonce } = nonceObj; // hex string

    // ---- Commit phase ----
    const deltas = await sampleDeltas();
    const traceBytes = new Uint8Array(deltas.buffer);
    const traceHashHex = await sha3(traceBytes);
    const commitMaterial = new TextEncoder().encode(`${epoch}|${nonce}|${traceHashHex}`);
    const commitHash = await sha3(commitMaterial);

    await API.request('/commit', {
      method: 'POST',
      body: JSON.stringify({ epoch, user_id: userId, nonce, commit_hash: commitHash })
    });

    // Wait until reveal window (epoch start + 6 s)
    const revealTime = (epoch * EPOCH_LENGTH_MS) + 6_000;
    const waitMs = revealTime - Date.now();
    if (waitMs > 0) await sleep(waitMs);

    // ---- Reveal phase ----
    // Pin raw deltas to IPFS
    const cid = await w3.put([new File([traceBytes], 'deltas.bin')], {
      name: `epoch-${epoch}-uid-${userId}`,
      wrapWithDirectory: false,
    });

    // Sign CID via WebAuthn (RS256)
    const signature = await signWithWebAuthn(cid);

    await API.request('/reveal', {
      method: 'POST',
      body: JSON.stringify({ epoch, user_id: userId, cid, signature })
    });

    // Sleep remainder of epoch
    const nextEpochStart = (epoch + 1) * EPOCH_LENGTH_MS;
    const rest = nextEpochStart - Date.now();
    if (rest > 0) await sleep(rest);
  }
}

// WebAuthn helper – returns base64 signature of CID
async function signWithWebAuthn(message) {
  const enc = new TextEncoder();
  const challenge = crypto.getRandomValues(new Uint8Array(32));
  const cred = await navigator.credentials.get({
    publicKey: {
      challenge,
      allowCredentials: [],
      timeout: 30000,
      userVerification: 'preferred',
    }
  });
  if (!cred || !cred.response || !cred.rawId) throw new Error('WebAuthn failed');
  // For demo, we concatenate rawId + signature + message hash
  const sigBuf = new Uint8Array(cred.response.signature);
  const rawId = new Uint8Array(cred.rawId);
  const msgHash = await sha3(enc.encode(message));
  const combined = new Uint8Array(rawId.length + sigBuf.length + msgHash.length / 2);
  combined.set(rawId, 0);
  combined.set(sigBuf, rawId.length);
  combined.set(new Uint8Array(msgHash.match(/.{2}/g).map(h => parseInt(h, 16))), rawId.length + sigBuf.length);
  return btoa(String.fromCharCode(...combined));
} 