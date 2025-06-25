from services.shard_mixer.main import store_reveal, RevealPayload, Metrics, dump_proof, HONEST_ENTROPY_FRACTION_MIN
import os, time

def test_hef_gate(tmp_path, monkeypatch):
    # point DB to temp dir
    monkeypatch.setenv("SHARD_DB_DIR", str(tmp_path))
    from importlib import reload
    import services.shard_mixer.main as sm
    reload(sm)

    epoch = 9999
    # create reveals: 3 honest (q=0.8), 8 pseudo (q=0.1)
    good_metrics = Metrics(compression_ratio=0.1, spectral_slope=1.0, mutual_information=0.8, whiteness=0.1, sample_count=1024)
    bad_metrics = Metrics(compression_ratio=0.9, spectral_slope=0.5, mutual_information=0.1, whiteness=0.9, sample_count=1024)

    for i in range(3):
        p = RevealPayload(epoch=epoch, operator_id=f"good{i}", merkle_root=f"{i:064x}", metrics=good_metrics)
        sm.store_reveal(p, good_metrics.quality_score(), 100)

    for i in range(8):
        p = RevealPayload(epoch=epoch, operator_id=f"bad{i}", merkle_root=f"{i+20:064x}", metrics=bad_metrics)
        sm.store_reveal(p, bad_metrics.quality_score(), 50)

    sm.dump_proof(epoch)
    void_path = tmp_path / f"VOID_epoch_{epoch}.json"
    assert void_path.exists(), "HEF gate failed to void epoch" 