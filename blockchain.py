"""Chronomancy header chain anchored in drand randomness.

Scott Wilber justification: A minimal, witness-verifiable chain is all that's
needed to prove entropy freshness (canon.yaml › autonomy).  Blocks are cheap –
we build one whenever a shard-quality Merkle root is ready.
"""

from __future__ import annotations

import json
import threading
import time
import urllib.request
from hashlib import sha256
from pathlib import Path
from typing import List

# ------------------------------------------------------------
# Persistence
# ------------------------------------------------------------

ROOT_DIR = Path(__file__).resolve().parent
CHAIN_PATH = ROOT_DIR / "chain.json"
CHAIN_PATH.parent.mkdir(parents=True, exist_ok=True)

# ------------------------------------------------------------
# Drand public beacon – 30-s cadence
# ------------------------------------------------------------

DRAND_URL = "https://api.drand.sh/public/latest"

def _fetch_drand() -> tuple[int, str]:
    """Return (round, randomness_hex).  Fallback to zeros on error."""
    try:
        with urllib.request.urlopen(DRAND_URL, timeout=5) as r:
            d = json.loads(r.read())
            return int(d["round"]), d["randomness"]
    except Exception as exc:  # noqa: BLE001
        print("drand fetch failed:", exc)
        return 0, "0" * 64

# ------------------------------------------------------------
# Merkle helpers
# ------------------------------------------------------------

def merkle_root(leaves: List[str]) -> str:
    """Return the hex Merkle root of *leaves* (hex strings)."""
    if not leaves:
        return "0" * 64
    cur = [bytes.fromhex(h) for h in leaves]
    while len(cur) > 1:
        nxt = []
        for i in range(0, len(cur), 2):
            a = cur[i]
            b = cur[i + 1] if i + 1 < len(cur) else a  # duplicate last if odd
            nxt.append(sha256(a + b).digest())
        cur = nxt
    return cur[0].hex()

# ------------------------------------------------------------
# Block & chain classes
# ------------------------------------------------------------

class Block(dict):
    @property
    def hash(self) -> str:  # type: ignore[override]
        return self["hash"]


class Chain:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        if CHAIN_PATH.exists():
            self._chain: List[Block] = [Block(b) for b in json.loads(CHAIN_PATH.read_text())]
        else:
            self._chain = [self._genesis()]
            self._persist()

    # ----------------- public -----------------
    def latest(self) -> Block:
        return self._chain[-1]

    def append(self, merkle_root_hex: str) -> Block:
        rnd, randomness = _fetch_drand()
        prev = self.latest()
        hgt = prev["height"] + 1
        ts = time.time()
        raw = f"{hgt}|{rnd}|{randomness}|{merkle_root_hex}|{prev['hash']}"
        h = sha256(raw.encode()).hexdigest()

        # Random-walk step: last nibble ≥ 8 ⇒ +1 else ‑1
        step = 1 if int(h[-1], 16) >= 8 else -1
        walk_val = prev.get("walk", 0) + step

        blk: Block = Block(
            height=hgt,
            ts=ts,
            drand_round=rnd,
            randomness=randomness,
            merkle_root=merkle_root_hex,
            prev_hash=prev["hash"],
            hash=h,
            step=step,
            walk=walk_val,
        )
        with self._lock:
            self._chain.append(blk)
            self._persist()
        return blk

    # ----------------- helpers -----------------
    def _persist(self) -> None:
        CHAIN_PATH.write_text(json.dumps(self._chain, indent=2))

    def _genesis(self) -> Block:
        g_hash = sha256(b"chronomancy-genesis").hexdigest()
        return Block(
            height=0,
            ts=time.time(),
            drand_round=0,
            randomness="0" * 64,
            merkle_root="0" * 64,
            prev_hash="0" * 64,
            hash=g_hash,
            step=0,
            walk=0,
        )

# ------------------------------------------------------------
# Singleton helpers
# ------------------------------------------------------------

_chain = Chain()

def latest_block() -> Block:
    return _chain.latest()

def commit_block(root_hex: str) -> Block:
    return _chain.append(root_hex)

# ------------------------------
# Walk helpers
# ------------------------------

def walk_value(height: int | None = None) -> int:
    """Return cumulative random-walk value at *height* (latest if None)."""
    if height is None:
        return _chain.latest()["walk"]
    if height < 0 or height > _chain.latest()["height"]:
        raise IndexError("height out of range")
    return _chain._chain[height]["walk"]  # type: ignore[attr-defined]

def step_at(height: int) -> int:
    """Return +1/-1 step for given height (0 is genesis)."""
    if height <= 0 or height > _chain.latest()["height"]:
        raise IndexError("invalid height")
    return _chain._chain[height]["step"]  # type: ignore[attr-defined] 