import os, tempfile
from fastapi.testclient import TestClient

# Ensure DB goes to temp dir so tests are isolated
os.environ["SHARD_DB_DIR"] = tempfile.mkdtemp()

from services.shard_mixer.main import app, COMPRESSION_CUTOFF

client = TestClient(app)


def _payload(cr: float):
    return {
        "epoch": 1,
        "operator_id": "sentinel",
        "merkle_root": "deadbeef" * 8,
        "src": "js_timer",
        "metrics": {
            "compression_ratio": cr,
            "spectral_slope": 1.0,
            "mutual_information": 0.0,
            "whiteness": 0.5,
            "sample_count": 1024,
        },
    }


def test_low_entropy_rejected():
    # compression_ratio above cutoff must 400
    resp = client.post("/focus/reveal", json=_payload(COMPRESSION_CUTOFF + 0.05))
    assert resp.status_code == 400


def test_good_entropy_accepted():
    resp = client.post("/focus/reveal", json=_payload(COMPRESSION_CUTOFF - 0.10))
    assert resp.status_code == 200 