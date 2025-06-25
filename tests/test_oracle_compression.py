from services.shard_mixer.main import Metrics, HONEST_ENTROPY_FRACTION_MIN
import os, random

def test_oracle_quality_scores():
    # PRNG data – expect low quality (< slash threshold 0.3)
    prng = os.urandom(2_000_000)
    m1 = Metrics.from_trace(prng)
    assert m1.quality_score() < 0.3

    # Add structure: repeating pattern should increase compression ratio but MI high
    patterned = (b'AB' * 1_000_000)
    m2 = Metrics.from_trace(patterned)
    assert m2.quality_score() > 0.6 