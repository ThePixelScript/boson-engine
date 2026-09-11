"""
Boson NNUE Model Exporter (Phase 8-C2).
Serializes calibrated integer checkpoint into canonical little-endian binary boson-v2.nnue.
"""

import os
import sys
import struct
import hashlib
import json
import torch

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)

NNUE_MAGIC = 0x4E4E5545  # 'N','N','U','E'
NNUE_VERSION = 1
HALFKP_FEATURES = 40960
ACCUMULATOR_SIZE = 512
FC1_INPUT_SIZE = 1024
FC1_OUTPUT_SIZE = 32
FC2_OUTPUT_SIZE = 32


def export_model(checkpoint_path: str = "checkpoints/C3_QAT_calibrated.pt",
                 output_bin_path: str = "models/boson-v2.nnue",
                 output_manifest_path: str = "models/boson-v2.manifest.json") -> dict:
    ckpt_full = os.path.join(PROJECT_ROOT, checkpoint_path)
    out_bin_full = os.path.join(PROJECT_ROOT, output_bin_path)
    out_manifest_full = os.path.join(PROJECT_ROOT, output_manifest_path)

    print(f"\n=================================================================")
    print(f"===   PHASE 8-C2: BOSON NNUE BINARY MODEL EXPORTER (V2)       ===")
    print(f"=================================================================")
    print(f"  Source Checkpoint:  {checkpoint_path}")
    print(f"  Target Binary Path: {output_bin_path}")
    print(f"  Target Manifest:    {output_manifest_path}")

    ckpt = torch.load(ckpt_full, map_location="cpu", weights_only=False)
    sd = ckpt["model_state_dict"]

    # 1. Feature transformer biases and weights
    ft_b_flt = sd["ft_biases"].float()
    ft_w_flt = sd["ft_weights"].float()
    ft_b_int = torch.round(ft_b_flt).to(torch.int16).numpy()
    ft_w_int = torch.round(ft_w_flt).to(torch.int16).numpy()

    # 2. FC1: weights (fc1.weight * 64/127), biases (fc1.bias * 64)
    w1_flt = sd["fc1.weight"].float() * (64.0 / 127.0)
    b1_flt = sd["fc1.bias"].float() * 64.0
    w1_int = torch.clamp(torch.round(w1_flt), -128, 127).to(torch.int8).numpy()
    b1_int = torch.round(b1_flt).to(torch.int32).numpy()

    # 3. FC2: weights (fc2.weight * 64), biases (fc2.bias * 64)
    w2_flt = sd["fc2.weight"].float() * 64.0
    b2_flt = sd["fc2.bias"].float() * 64.0
    w2_int = torch.clamp(torch.round(w2_flt), -128, 127).to(torch.int8).numpy()
    b2_int = torch.round(b2_flt).to(torch.int32).numpy()

    # 4. FC3: weights (fc3.weight / 16), bias (fc3.bias / 16)
    w3_flt = sd["fc3.weight"].float() / 16.0
    b3_flt = sd["fc3.bias"].float() / 16.0
    w3_int = torch.clamp(torch.round(w3_flt), -128, 127).to(torch.int8).numpy().reshape(32)
    b3_int = int(torch.round(b3_flt).to(torch.int32).item())

    # Build binary byte buffer
    # Header: uint32[7]
    header = struct.pack("<7I",
                         NNUE_MAGIC,
                         NNUE_VERSION,
                         HALFKP_FEATURES,
                         ACCUMULATOR_SIZE,
                         FC1_INPUT_SIZE,
                         FC1_OUTPUT_SIZE,
                         FC2_OUTPUT_SIZE)

    payload = bytearray()
    payload.extend(header)
    payload.extend(ft_b_int.tobytes())
    payload.extend(ft_w_int.tobytes())
    payload.extend(b1_int.tobytes())
    payload.extend(w1_int.tobytes())
    payload.extend(b2_int.tobytes())
    payload.extend(w2_int.tobytes())
    payload.extend(struct.pack("<i", b3_int))
    payload.extend(w3_int.tobytes())

    total_bytes = len(payload)
    sha256_hash = hashlib.sha256(payload).hexdigest()

    os.makedirs(os.path.dirname(out_bin_full), exist_ok=True)
    with open(out_bin_full, "wb") as f:
        f.write(payload)

    # Compute tensor checksums
    manifest = {
        "format": "BOSN_NNUE_V1",
        "model_file": output_bin_path,
        "size_bytes": total_bytes,
        "sha256": sha256_hash,
        "architecture": {
            "magic": hex(NNUE_MAGIC),
            "version": NNUE_VERSION,
            "halfkp_features": HALFKP_FEATURES,
            "accumulator_size": ACCUMULATOR_SIZE,
            "fc1_input": FC1_INPUT_SIZE,
            "fc1_output": FC1_OUTPUT_SIZE,
            "fc2_output": FC2_OUTPUT_SIZE,
            "fc3_output": 1,
            "output_scale": 16
        },
        "tensors": {
            "ft_biases": {
                "dtype": "int16",
                "shape": list(ft_b_int.shape),
                "min": int(ft_b_int.min()),
                "max": int(ft_b_int.max()),
                "sha256": hashlib.sha256(ft_b_int.tobytes()).hexdigest()
            },
            "ft_weights": {
                "dtype": "int16",
                "shape": list(ft_w_int.shape),
                "min": int(ft_w_int.min()),
                "max": int(ft_w_int.max()),
                "sha256": hashlib.sha256(ft_w_int.tobytes()).hexdigest()
            },
            "fc1_biases": {
                "dtype": "int32",
                "shape": list(b1_int.shape),
                "min": int(b1_int.min()),
                "max": int(b1_int.max()),
                "sha256": hashlib.sha256(b1_int.tobytes()).hexdigest()
            },
            "fc1_weights": {
                "dtype": "int8",
                "shape": list(w1_int.shape),
                "min": int(w1_int.min()),
                "max": int(w1_int.max()),
                "zero_count": int((w1_int == 0).sum()),
                "sha256": hashlib.sha256(w1_int.tobytes()).hexdigest()
            },
            "fc2_biases": {
                "dtype": "int32",
                "shape": list(b2_int.shape),
                "min": int(b2_int.min()),
                "max": int(b2_int.max()),
                "sha256": hashlib.sha256(b2_int.tobytes()).hexdigest()
            },
            "fc2_weights": {
                "dtype": "int8",
                "shape": list(w2_int.shape),
                "min": int(w2_int.min()),
                "max": int(w2_int.max()),
                "zero_count": int((w2_int == 0).sum()),
                "sha256": hashlib.sha256(w2_int.tobytes()).hexdigest()
            },
            "fc3_bias": {
                "dtype": "int32",
                "val": b3_int
            },
            "fc3_weights": {
                "dtype": "int8",
                "shape": list(w3_int.shape),
                "min": int(w3_int.min()),
                "max": int(w3_int.max()),
                "zero_count": int((w3_int == 0).sum()),
                "sha256": hashlib.sha256(w3_int.tobytes()).hexdigest()
            }
        }
    }

    os.makedirs(os.path.dirname(out_manifest_full), exist_ok=True)
    with open(out_manifest_full, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)

    print(f"  Successfully exported {total_bytes:,} bytes.")
    print(f"  SHA-256 Digest: {sha256_hash}")
    print(f"  Manifest written to: {output_manifest_path}\n")

    return manifest


if __name__ == "__main__":
    export_model()
