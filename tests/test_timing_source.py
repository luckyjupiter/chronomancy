import statistics, time
from bot.pcqng import PcqngCore


def test_cycle_timing_quality():
    """σ/μ of timestamp deltas should be reasonably low (<0.35) over short run.

    We use a 1-s sample for CI friendliness instead of full 30-s.  This
    threshold is empirical and may be relaxed on very slow VMs – tweak in
    canon.yaml if needed.
    """
    core = PcqngCore()

    samples = []
    t_end = time.time() + 1.0  # one-second window
    while time.time() < t_end:
        ts = core._read_timestamp()
        samples.append(ts)
        time.sleep(0.0001)  # 0.1 ms spacing

    deltas = [b - a for a, b in zip(samples[:-1], samples[1:])]
    if not deltas:
        raise AssertionError("No deltas captured")
    mu = statistics.mean(deltas)
    sigma = statistics.stdev(deltas)
    ratio = sigma / mu if mu else 999
    assert ratio <= 1.0, f"σ/μ too high: {ratio:.3f}" 