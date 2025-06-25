# Chronomancy – Temporal Entropy Network

_A spiritual successor to Randonautica, powered by Scott Wilber's bias-amplification doctrine and a fully-auditable entropy pipeline._

---

## ✨ What is Chronomancy?
Chronomancy is a research-grade platform for exploring *temporal* anomalies – the same way Randonautica popularised spatial anomaly hunts.  Instead of dropping you at random GPS coordinates, Chronomancy drops *moments* into your future timeline that are derived from a raw physical noise source (PCQNG jitter) and braided with a public randomness beacon.  Think **quantum fortune cookies for time**.

Our north-star requirements (all carved into `canon.yaml` and enforced by tests):

1. **Bias preservation** – raw coloured noise must remain accessible for PSI analytics.
2. **Cryptographic unpredictability** – nobody can peek at a future seed.
3. **Public verifiability** – any third party can re-derive every epoch seed from public artefacts.

---

## 🛠️ Current Stack

```
chronomancy/
│
├─ bot/               # Telegram bot + PCQNG port (Python)
├─ miniapp/           # Telegram WebApp frontend
├─ services/
│   └─ mixer/         # (WIP) epoch mixer & VDF
├─ infra/
│   └─ drand-lite/    # 9-node Docker mesh, BLS threshold 6/9
└─ tools/             # verification + test harness (WIP)
```

| Layer | Component | Status |
|-------|-----------|--------|
| **Physical noise** | PCQNG jitter port (Windows cycle timer) | ✅ tested |
| **Browser sampler** | `browser-entropy.js` commit/reveal loop | ⏳ alpha |
| **Beacon** | drand-lite mesh (docker-compose) | ⏳ alpha |
| **Mixer** | seed interleave ＋ Wesolowski VDF | 🚧 todo |
| **Verifier** | one-shot `verify_epoch.py` + GitHub Action | 🚧 todo |
| **Explorer** | Grafana histogram + Twine pulse viewer | 🚧 todo |

---

## 🚦 Protocol in 60 Seconds

```
Epoch = 10 s hard-wall
 ├─ 1 s : nonce reveal  ← drand-lite mesh (BLS + VRF)
 ├─ 5 s : commit window ← browser sends SHA3(epoch‖nonce‖traceHash)
 ├─ 4 s : reveal window ← browser pins raw jitter (CID) + WebAuthn sig
 └─ <1 s : mixer → interleave bytes → VDF → Twine pulse
```

Trust model: at least **3 honest browsers** and **1 honest drand node** per epoch; missing reveals get substituted with `SHA3(commit)` so attackers can't bias the seed by silence.

---

## 🪢 Dual-Stream Architecture

| Rail | Purpose | Rate | Where bytes live | Verifiable? |
|------|---------|------|------------------|-------------|
| **Public Mesh** | Anchor every experiment in an immutable, VRF-seeded pulse chain. | 10 s epochs, ≈20 kB | IPFS (interleaved seed) + on-chain pulse header | ✅ (Twine pulse + VDF) |
| **Focus Channel** | High-bandwidth raw jitter for a single operator session. | 1 MHz (≈8 Mbit s⁻¹) | Operator's device → IPFS (CID) → **sub-epoch** Merkle root (≤1 s) | ✅ (Merkle proof to root) |

Why two rails?
1. *PSI coupling* – the operator manipulates timing noise inside the same silicon they touch.
2. *Scalability* – global mesh stays light; only paying users open heavy channels.
3. *Auditability* – a 32-byte Merkle root is enough to prove integrity; no need to jam megabits onto chain.

Focus sessions are funded by burning **ENT** tokens; the burn tx emits an on-chain event that authorises one 10-s window. Frames delayed > 200 ms are marked *stale* and don't count toward bonuses.  The Mixer treats every sub-epoch Merkle root as an **opaque payload**; it merely notarises the root in the next 10-s pulse—no latency coupling.

Honest-entropy safeguard: a pulse is **rejected** if <40 % of deltas derive from fully-revealed traces (constant lives in `canon.yaml`).

_“1 THz” is a horizon marker, not a hard pledge.  Real deployment will adopt an **adaptive sampling layer** that projects per-operator jitter onto bias vectors so shard blobs remain <10 MB per pulse._

---

## ⚛️ 1 THz Distributed Array Vision

Long-term we want an *effective* 1 THz entropy aperture – one trillion timestamp deltas per second – without any single device or server melting.

Strategy:

1. **Horizontal scale** – 1 M focus-capable devices × 1 MHz each = 1 THz.
2. **Sharded mixers** – every 10 s, devices hash-route their Merkle roots to one of 1 024 shard mixers that run in parallel, then a super-mixer folds 1 024 VDF proofs into the final pulse.
3. **Sparse pulls** – verifiers need only the shard that contains the operator of interest; overall bandwidth stays O(log N).
4. **Edge IPFS gateways** – CID pinning happens at region-local gateways to keep <200 ms SLA.
5. **Hierarchical tokens** – ENT v2 introduces *stake weight* so shard operators are reimbursed for storage & VDF cycles.

_Reaching 1 THz is a scaling game, not a physics one – the coloured noise is plentiful, we just need to pipeline it._

---

## 📈 Roadmap & Milestones

| Tag | Goal | Owner |
|-----|------|-------|
| **M-0** | Beacon heartbeat reachable at `/beacon/nonce` | infra 🅰 |
| **M-α** | Commit + reveal loop proven for ≥3 browsers | frontend 🫵 |
| **M-β1** | Mixer emits pulse & verifier replays (no adversary) | backend 🅱 |
| **M-β2** | All adversary simulations pass (early-peek, replay, stall, sybil) | backend 🅱 |
| **M-γ** | VDF integrated, 99-th % epoch <10 s | infra 🅰 + backend 🅱 |
| **M-δ** | ENT token contract + gas→fiat cost-oracle stub | solidity 🆕 |
| **M-ε** | Focus websocket & 200 ms latency tests pass | backend 🅱 |
| **M-ζ** | Honest-entropy threshold enforcement | all |
| **M-η** | Explorer dashboard live + payout UI | frontend 🫵 |
| **M-θ** | 1 024-shard mixer PoC (≈1 GHz aggregate) | infra 🅰 |
| **M-λ** | 1 THz distributed array public beta (with adaptive sampling) | all 🔄 |

After **η** the core pipeline is locked; gamified layers (badges, QC-trainer, advanced tokenomics) land on separate repos.

---

## 🔬 Testing Philosophy
All canonical constants and bias constraints live in `canon.yaml`.  Test hierarchy:

1. **Unit** – deterministic math (LFSR, LPF, state machine).
2. **Statistical** – eBits bias spectrum, σ/μ thresholds.
3. **Protocol** – commit/reveal replay, mixer substitution, VDF proof.
4. **Adversary simulations** – early-peek, replay, beacon outage, entropy-fraction.
5. **Power analysis lock-in** – every statistical test fixes its sample count; changes require paired power-justification PR.

CI must stay green across *all five* layers before merge.

---

## 🤝 Contributing

1. Fork and branch `feat/<module>` off `main`.
2. Keep PRs atomic; update `canon.yaml` when you introduce or change a constant.
3. Add/extend tests **in the same PR**.
4. Sign commits — we're an entropy project, provenance matters.

---

> "Randomness is just unmodelled causality." — Scott Wilber 