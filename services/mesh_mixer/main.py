from __future__ import annotations

"""Mesh-Mixer service – FastAPI stub.

Scott Wilber's canonical reference stresses that the mesh mixer must act as an
unbiased *root-of-roots* aggregator as well as a VDF time-lock.  This stub only
persists incoming pulses so that the integration pipeline can be wired and
tested.  Constants remain delegated to `canon.yaml` for authoritative values.
"""

import json
import os
from datetime import datetime
from pathlib import Path
from typing import Any, Dict

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

DATA_DIR = Path(os.getenv("DATA_DIR", "/data"))
DATA_DIR.mkdir(parents=True, exist_ok=True)
PULSES_DIR = DATA_DIR / "pulses"
PULSES_DIR.mkdir(exist_ok=True)

app = FastAPI(title="Chronomancy Mesh Mixer", version="0.1.0-stub")


class PulsePayload(BaseModel):
    epoch: int = Field(..., ge=0, description="Global epoch index")
    merkle_root: str = Field(..., min_length=8)
    proof_cid: str | None = Field(None, description="IPFS CID carrying proof")
    entropy_estimate: float = Field(..., ge=0.0, le=1.0)
    metadata: Dict[str, Any] | None = None


@app.post("/pulse", status_code=202)
async def accept_pulse(pulse: PulsePayload):
    """Persist the pulse JSON under /data/pulses/epoch_<n>.json.

    A 409 *Conflict* is returned if a pulse for the same epoch already exists.
    """
    out_path = PULSES_DIR / f"epoch_{pulse.epoch}.json"
    if out_path.exists():
        raise HTTPException(status_code=409, detail="pulse for epoch already present")

    with out_path.open("w", encoding="utf-8") as fh:
        json.dump({
            "received_at": datetime.utcnow().isoformat() + "Z",
            **pulse.model_dump(mode="json")
        }, fh, indent=2)

    return {"status": "queued", "path": str(out_path)} 