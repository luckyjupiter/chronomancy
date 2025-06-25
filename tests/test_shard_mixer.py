import importlib
from services.shard_mixer.main import Metrics, BASE_CAP_KHZ

def test_quality_score_and_cap():
    metrics = Metrics(
        compression_ratio=0.2,
        spectral_slope=1.0,
        mutual_information=0.8,
        whiteness=0.1,
        sample_count=1024,
    )
    q = metrics.quality_score()
    assert 0.0 <= q <= 1.0
    cap_khz = int(round(BASE_CAP_KHZ * (q ** 2)))
    # Expect cap between 0 and BASE_CAP_KHZ
    assert 0 <= cap_khz <= BASE_CAP_KHZ 