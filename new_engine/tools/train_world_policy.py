#!/usr/bin/env python3
"""Train a tiny optional Valwood economic policy on explicit synthetic rules.

This is a small MLP, not a language model. It chooses gather/consume/transfer/
patrol proposals. World rules remain authoritative. No personal data, network
service or third-party training corpus is used. Published outputs are NEW files.
"""
import os
os.environ["OPENBLAS_NUM_THREADS"] = "1"
os.environ["OMP_NUM_THREADS"] = "1"
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import numpy as np


def dataset(rng, count):
    x = np.zeros((count, 18), dtype=np.float32)
    activity = rng.integers(0, 6, count)
    x[np.arange(count), activity] = 1
    x[:, 6:10] = rng.uniform(0, 1, (count, 4))
    phase = rng.uniform(0, 2 * np.pi, count)
    x[:, 10], x[:, 11] = np.sin(phase), np.cos(phase)
    x[:, 12] = rng.integers(0, 2, count)
    x[:, 14:18] = 1
    y = np.zeros(count, dtype=np.int64)
    y[activity == 2] = 1  # Work -> Gather
    y[(activity == 2) & (x[:, 12] == 1) & (x[:, 6] > .25) &
      (x[:, 8] + .1 < x[:, 6])] = 3  # A surplus can supply the authored trade route
    y[activity == 3] = 2  # Eat -> Consume
    y[activity == 4] = 4  # Guard -> Patrol
    return x, y


def publish(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".valwood-policy-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.link(temporary, path)
    finally:
        os.unlink(temporary)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument("--iterations", type=int, default=1200)
    args = parser.parse_args()
    if args.output.exists() or args.report.exists():
        parser.error("Choose new output and report files; existing files are preserved")
    if not 100 <= args.iterations <= 5000:
        parser.error("--iterations must be100..5000")
    rng = np.random.default_rng(50912)
    x, labels = dataset(rng, 8192)
    params = [rng.normal(0, .2, (18, 24)), np.zeros(24),
              rng.normal(0, .2, (24, 5)), np.zeros(5)]
    first = [np.zeros_like(p) for p in params]
    second = [np.zeros_like(p) for p in params]
    for step in range(1, args.iterations + 1):
        selected = rng.integers(0, len(x), 384)
        batch, y = x[selected], labels[selected]
        z = batch @ params[0] + params[1]
        hidden = np.maximum(0, z)
        logits = hidden @ params[2] + params[3]
        p = np.exp(logits - logits.max(axis=1, keepdims=True))
        p /= p.sum(axis=1, keepdims=True)
        p[np.arange(len(y)), y] -= 1
        p /= len(y)
        back = (p @ params[2].T) * (z > 0)
        gradients = [batch.T @ back, back.sum(axis=0), hidden.T @ p, p.sum(axis=0)]
        for i, gradient in enumerate(gradients):
            first[i] = .9 * first[i] + .1 * gradient
            second[i] = .999 * second[i] + .001 * gradient * gradient
            params[i] -= .008 * (first[i] / (1 - .9 ** step)) / (
                np.sqrt(second[i] / (1 - .999 ** step)) + 1e-8)
    test, expected = dataset(np.random.default_rng(27091), 4096)
    actual = np.argmax(np.maximum(0, test @ params[0] + params[1]) @ params[2] + params[3], axis=1)
    accuracy = float(np.mean(actual == expected))
    if accuracy < .96 or any(not np.isfinite(p).all() or np.abs(p).max() > 32 for p in params):
        raise RuntimeError("The trained weights failed the explicit held-out accuracy/finite-weight gate")
    text = "VALWOOD_MICRO_POLICY 1 18 24 5\n"
    text += "\n".join(" ".join(format(float(v), ".9g") for v in p.flat) for p in params) + "\n"
    payload = text.encode()
    confusion = np.zeros((5, 5), dtype=np.int64)
    for a, b in zip(expected, actual):
        confusion[a, b] += 1
    report = {"schema": "valwood.micro-policy-training", "version": 1,
              "architecture": [18, 24, 5], "parameter_count": sum(p.size for p in params),
              "training": "Synthetic authored economic rules, fixed seed50912; no user data or external corpus",
              "held_out_seed": 27091, "training_samples": len(x), "held_out_samples": len(test),
              "accuracy": accuracy, "confusion_matrix": confusion.tolist(),
              "weights_sha256": hashlib.sha256(payload).hexdigest(),
              "limits": "Not language/dialogue/navigation inference; proposals require authoritative world validation"}
    publish(args.output, payload)
    publish(args.report, (json.dumps(report, indent=2) + "\n").encode())
    print(json.dumps({"ok": True, "accuracy": accuracy, "parameters": report["parameter_count"]}))


if __name__ == "__main__":
    main()
