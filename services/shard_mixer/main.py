"""Shard Mixer Service

Part of the Chronomancy entropy pipeline.  Runs independently from the
Mini-App backend and is meant to be deployed one per shard (N≈1 024).

Responsibilities
• Accept focus-channel reveal payloads → Merkle root + quality metrics.
• Score quality via open-source oracle; persist in SQLite.
• Return new sampling cap (kHz) so the client can throttle itself.
• Produce shard-level proof file (JSON) every 10-s mesh epoch for the global
  mesh mixer.  Proof = list of root objects + aggregated stats.

Scott Wilber justification: quality gating and quadratic reward maintain
bias-preserving entropy without swamping the mesh rail.
"""

from __future__ import annotations

import json
import math
import os
import sqlite3
import time
from pathlib import Path
from typing import Dict

import zstandard as zstd  # lightweight, pure-C back-end wheel on all OSes
import numpy as np
from fastapi import FastAPI, HTTPException, Body
from pydantic import BaseModel, Field

# ---------------------------------------------------------------------------
# Canon-pinned constants (mirrored; authoritative copy in canon.yaml)
# ---------------------------------------------------------------------------

BASE_CAP_KHZ = 250
HONEST_ENTROPY_FRACTION_MIN = 0.40

# ---------------------------------------------------------------------------
# Storage path – mount a docker volume at /data for persistence
# ---------------------------------------------------------------------------

DB_DIR = Path(os.environ.get("SHARD_DB_DIR", "/data"))
DB_DIR.mkdir(parents=True, exist_ok=True)
DB_PATH = DB_DIR / "shard_mixer.db"

app = FastAPI(title="Chronomancy Shard Mixer", version="0.1.0")


class Metrics(BaseModel):
    compression_ratio: float = Field(..., ge=0, le=1)
    spectral_slope: float = Field(..., ge=0, le=2)
    mutual_information: float = Field(..., ge=0, le=1)
    whiteness: float = Field(..., ge=0, le=1)
    sample_count: int = Field(..., ge=1)

    def quality_score(self) -> float:
        """Return simplified quality score: just structural compressibility."""
        return max(0.0, min(1.0, 1 - self.compression_ratio))

    # ---------------- Class helpers used by tests ----------------
    @classmethod
    def from_trace(cls, data: bytes):
        """Compute oracle metrics from raw jitter bytes (≤ a few MB).

        Compression ratio uses Zstd level 3.  Mutual information uses a 16-bin
        histogram of successive byte pairs.  Spectral slope is approximated via
        log–log PSD fit but simplified to variance ratio between low/high bands
        for speed (12.5 kHz binning when sample_rate=1 MHz).
        """
        if not data:
            raise ValueError("empty data")

        # Compression ratio
        cctx = zstd.ZstdCompressor(level=3)
        compressed = cctx.compress(data)
        cr = len(compressed) / len(data)

        # Mutual information (16-bin)
        arr = np.frombuffer(data, dtype=np.uint8)
        if len(arr) < 2:
            mi = 0.0
        else:
            x = arr[:-1] >> 4  # high nibble
            y = arr[1:] >> 4
            joint_hist = np.zeros((16, 16), dtype=np.int32)
            for xi, yi in zip(x, y):
                joint_hist[xi, yi] += 1
            px = joint_hist.sum(axis=1, keepdims=True)
            py = joint_hist.sum(axis=0, keepdims=True)
            pxy = joint_hist
            with np.errstate(divide="ignore", invalid="ignore"):
                mi_mat = pxy * np.log2((pxy * joint_hist.sum()) / (px * py))
            mi = np.nansum(mi_mat) / (len(arr) - 1)
            mi = min(max(mi / 4.0, 0.0), 1.0)  # normalise roughly

        # Spectral slope proxy
        # Fast variance ratio between first and last quartile frequencies.
        fft = np.fft.rfft(arr.astype(np.float32) - 128.0)
        power = np.abs(fft) ** 2
        q = len(power) // 4
        low = power[:q].mean()
        high = power[-q:].mean()
        slope = low / high if high else 0
        slope_norm = min(max((math.log10(slope + 1e-6) + 6) / 12, 0.0), 2.0)

        return cls(
            compression_ratio=float(cr),
            mutual_information=float(mi),
            spectral_slope=float(slope_norm),
            whiteness=float(high / (low + high + 1e-6)),
            sample_count=len(arr),
        )


class RevealPayload(BaseModel):
    epoch: int = Field(..., description="Mesh epoch index (10-s cadence)")
    operator_id: str = Field(..., description="Wallet or device ID")
    merkle_root: str = Field(..., pattern=r"^[0-9a-fA-F]{64}$")  # hex-encoded 32-B hash
    src: str = Field("js_timer", pattern=r"^[a-z0-9_]+$", description="Entropy source tag – POC only supports 'js_timer'")
    metrics: Metrics


# ---------------------------------------------------------------------------
# DB helpers
# ---------------------------------------------------------------------------

SCHEMA_SQL = """
CREATE TABLE IF NOT EXISTS reveals (
    epoch INTEGER,
    operator_id TEXT,
    merkle_root TEXT,
    src TEXT,
    quality REAL,
    cap_khz INTEGER,
    received_ts REAL,
    PRIMARY KEY (epoch, operator_id, src)
);
"""

def get_conn() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH)
    conn.execute("PRAGMA journal_mode=WAL;")
    conn.execute(SCHEMA_SQL)
    return conn


def store_reveal(p: RevealPayload, quality: float, cap_khz: int) -> None:
    with get_conn() as conn:
        conn.execute(
            "INSERT OR REPLACE INTO reveals VALUES (?,?,?,?,?,?,?)",
            (
                p.epoch,
                p.operator_id,
                p.merkle_root.lower(),
                p.src,
                quality,
                cap_khz,
                time.time(),
            ),
        )
        conn.commit()


# ---------------------------------------------------------------------------
# Routes
# ---------------------------------------------------------------------------

@app.post("/focus/reveal")
async def focus_reveal(payload: RevealPayload = Body(...)) -> Dict[str, int]:
    """Receive Merkle root + metrics; return new cap_khz."""

    # duplicate reveal check
    with get_conn() as conn:
        existing = conn.execute(
            "SELECT merkle_root FROM reveals WHERE epoch=? AND operator_id=? AND src=?",
            (payload.epoch, payload.operator_id, payload.src),
        ).fetchone()
    if existing is not None:
        raise HTTPException(status_code=409, detail="duplicate reveal for epoch")

    # Basic sanity: reject traces smaller than 256 bytes (spam guard)
    if payload.metrics.sample_count < 256:
        raise HTTPException(status_code=400, detail="trace too small")

    q = payload.metrics.quality_score()
    cap_khz = int(round(BASE_CAP_KHZ * (q ** 2)))
    store_reveal(payload, q, cap_khz)

    # append to epoch log
    log_path = DB_DIR / f"epoch_{payload.epoch}.log"
    log_path.write_text(
        f"{time.time():.0f}\t{payload.operator_id}\t{q:.3f}\t{cap_khz}\n",
        append=True if log_path.exists() else False,
    )

    if q < HONEST_ENTROPY_FRACTION_MIN:
        # Emit VOID pulse marker
        out_path = Path(f"VOID_epoch_{payload.epoch}.json")
        out_path.write_text(json.dumps({"hef": q, "reason": "low_entropy"}, indent=2))
        print(f"VOID pulse: HEF {q:.2f} below threshold, wrote {out_path}")
        return

    out = [
        {"operator": op, "root": root, "quality": q} for op, root, q in rows
    ]
    Path(f"proof_epoch_{payload.epoch}.json").write_text(json.dumps(out, indent=2))
    print(f"wrote proof_epoch_{payload.epoch}.json with {len(out)} roots, HEF={q:.2f}")

    return {"cap_khz": cap_khz, "quality": round(q, 3)}


@app.get("/healthz")
async def health() -> Dict[str, str]:
    return {"status": "ok"}


# ---------------------------------------------------------------------------
# CLI utility to dump current epoch proofs (simplified)
# ---------------------------------------------------------------------------

def dump_proof(epoch: int) -> None:
    """Write shard proof JSON for given epoch."""
    with get_conn() as conn:
        rows = conn.execute(
            "SELECT operator_id, merkle_root, quality FROM reveals WHERE epoch=?",
            (epoch,),
        ).fetchall()
    honest = [r for r in rows if r[2] >= HONEST_ENTROPY_FRACTION_MIN]
    hef = len(honest) / len(rows) if rows else 0.0

    if hef < HONEST_ENTROPY_FRACTION_MIN:
        # Emit VOID pulse marker
        out_path = Path(f"VOID_epoch_{epoch}.json")
        out_path.write_text(json.dumps({"hef": hef, "reason": "low_entropy"}))
        print(f"VOID pulse: HEF {hef:.2f} below threshold, wrote {out_path}")
        return

    out = [
        {"operator": op, "root": root, "quality": q} for op, root, q in rows
    ]
    Path(f"proof_epoch_{epoch}.json").write_text(json.dumps(out, indent=2))
    print(f"wrote proof_epoch_{epoch}.json with {len(out)} roots, HEF={hef:.2f}")


if __name__ == "__main__":
    import argparse, uvicorn

    parser = argparse.ArgumentParser("shard-mixer service")
    parser.add_argument("--dump", type=int, help="Dump proof for epoch and exit")
    parser.add_argument("--serve", action="store_true", help="Run API server")
    args = parser.parse_args()

    if args.dump is not None:
        dump_proof(args.dump)
    elif args.serve:
        uvicorn.run("services.shard_mixer.main:app", host="0.0.0.0", port=8002) 