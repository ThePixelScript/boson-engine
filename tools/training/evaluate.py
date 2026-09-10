"""
Boson Evaluation & Quantization Shadow Diagnostic (Phase 8-B).
Performs Gate 8-B-15 Quantization Shadow Check and Test Set Evaluation.
"""

import os
import sys
import math
import json
import argparse
from typing import Dict, Tuple

# Ensure project root is in sys.path
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)

import torch
import torch.nn.functional as F

from tools.training.model import BosonNNUE
from tools.training.dataset import BosonDataset, get_dataloader
from tools.training.train import LN10_DIV_400, compute_logistic_prob, compute_pearson_corr


def run_quantization_shadow_check(model: BosonNNUE,
                                  val_dataset: BosonDataset,
                                  batch_size: int = 128,
                                  export_vectors_path: str = "checkpoints/gate_8b_15_vectors.json") -> Dict[str, float]:
    """
    Gate 8-B-15 Quantization Shadow Check:
    Emulates exact int16/int8/int32 integer arithmetic on a frozen mini-batch.
    Reports max |E_float - E_shadow| and MAE(E_float, E_shadow).
    """
    model.eval()

    val_loader = get_dataloader(val_dataset, batch_size=batch_size, shuffle=False)
    first_batch = next(iter(val_loader))
    acc = first_batch["accumulator"]
    z = first_batch["z_stm"]
    q = first_batch["q_stm"]

    with torch.no_grad():
        E_float = model(acc)
        E_shadow = model.quantization_shadow_forward(acc)

        diff = torch.abs(E_float - E_shadow)
        max_diff = torch.max(diff).item()
        mae = torch.mean(diff).item()
        corr = compute_pearson_corr(E_float, E_shadow)

    print(f"\n================================================================================")
    print(f"===   GATE 8-B-15 QUANTIZATION SHADOW CHECK TELEMETRY                        ===")
    print(f"================================================================================")
    print(f"  Frozen Mini-Batch Sample Size: {acc.size(0)} positions")
    print(f"  Max Absolute Error |E_float - E_shadow|: {max_diff:.2f} centipawns")
    print(f"  Mean Absolute Error MAE(E_float, E_shadow): {mae:.4f} centipawns")
    print(f"  Shadow Correlation Corr(E_float, E_shadow): {corr:.6f}")
    print(f"  Sample Head Comparisons (First 5 records):")
    for i in range(min(5, acc.size(0))):
        print(f"    Pos #{i:02d} | E_float: {E_float[i].item():+8.2f} cp | "
              f"E_shadow: {E_shadow[i].item():+8.2f} cp | "
              f"|Diff|: {diff[i].item():6.2f} cp | Teacher q: {q[i].item():+6.0f} cp")
    print(f"================================================================================\n")

    result = {
        "batch_size": acc.size(0),
        "max_abs_diff": max_diff,
        "mae": mae,
        "corr": corr,
        "sample_float": [round(x.item(), 2) for x in E_float[:10]],
        "sample_shadow": [round(x.item(), 2) for x in E_shadow[:10]],
    }

    if export_vectors_path:
        os.makedirs(os.path.dirname(export_vectors_path), exist_ok=True)
        # Export top 10 vectors for C++ verification
        export_data = {
            "max_abs_diff": max_diff,
            "mae": mae,
            "corr": corr,
            "positions": []
        }
        for i in range(min(10, acc.size(0))):
            rec = val_dataset.records[i]
            export_data["positions"].append({
                "index": i,
                "position_hash": hex(rec.position_hash),
                "side_to_move": rec.side_to_move,
                "z_stm": rec.z_stm,
                "q_stm": rec.q_stm,
                "us_features": rec.us_features,
                "them_features": rec.them_features,
                "E_float": float(E_float[i].item()),
                "E_shadow": float(E_shadow[i].item()),
                "diff": float(diff[i].item())
            })
        with open(export_vectors_path, "w") as f:
            json.dump(export_data, f, indent=2)

    return result


def evaluate_on_test_set(checkpoint_path: str,
                         test_path: str = "data/dataset_phase8a/test.bin") -> Dict[str, float]:
    """
    Evaluates the winning frozen model strictly on data/dataset_phase8a/test.bin (200 records).
    Must remain untouched until model selection is finalized.
    """
    print(f"\n================================================================================")
    print(f"===   FINAL TEST SET EVALUATION (POST-SELECTION FREEZE)                      ===")
    print(f"================================================================================")
    print(f"  Checkpoint: {checkpoint_path}")
    print(f"  Test File:  {test_path}")

    if not os.path.exists(checkpoint_path):
        raise FileNotFoundError(f"Checkpoint not found: {checkpoint_path}")

    ckpt = torch.load(checkpoint_path, map_location="cpu")
    model = BosonNNUE(mode=ckpt.get("mode", "P2"))
    model.load_state_dict(ckpt["model_state_dict"])
    model.eval()

    test_dataset = BosonDataset(test_path, precompute_accumulators=True)
    test_loader = get_dataloader(test_dataset, batch_size=128, shuffle=False)

    all_E, all_z, all_q = [], [], []
    h1_zeros, h1_sats = [], []
    h2_zeros, h2_sats = [], []

    with torch.no_grad():
        for batch in test_loader:
            acc = batch["accumulator"]
            z = batch["z_stm"]
            q = batch["q_stm"]

            E, diag = model.forward_with_diagnostics(acc)
            all_E.append(E)
            all_z.append(z)
            all_q.append(q)

            h1_zeros.append(diag["h1_zero_pct"])
            h1_sats.append(diag["h1_sat_pct"])
            h2_zeros.append(diag["h2_zero_pct"])
            h2_sats.append(diag["h2_sat_pct"])

    cat_E = torch.cat(all_E)
    cat_z = torch.cat(all_z)
    cat_q = torch.cat(all_q)

    p_model = compute_logistic_prob(cat_E)
    p_teacher = compute_logistic_prob(cat_q)

    test_bce = F.binary_cross_entropy_with_logits(cat_E * LN10_DIV_400, cat_z).item()
    test_brier = torch.mean((p_model - cat_z) ** 2).item()
    test_teacher_mse = torch.mean((p_model - p_teacher) ** 2).item()
    test_corr = compute_pearson_corr(cat_E, cat_q)
    test_mae_cp = torch.mean(torch.abs(cat_E - cat_q)).item()

    metrics = {
        "checkpoint": checkpoint_path,
        "test_records": len(test_dataset),
        "test_bce": test_bce,
        "test_brier": test_brier,
        "test_teacher_mse": test_teacher_mse,
        "test_corr": test_corr,
        "test_mae_cp": test_mae_cp,
        "h1_zero_pct": sum(h1_zeros) / max(1, len(h1_zeros)),
        "h1_sat_pct": sum(h1_sats) / max(1, len(h1_sats)),
        "h2_zero_pct": sum(h2_zeros) / max(1, len(h2_zeros)),
        "h2_sat_pct": sum(h2_sats) / max(1, len(h2_sats)),
        "mean_E": cat_E.mean().item(),
        "std_E": cat_E.std().item()
    }

    print(f"  Test Records Ingested:    {metrics['test_records']}")
    print(f"  Test BCE Loss:            {metrics['test_bce']:.4f}")
    print(f"  Test Brier Score:         {metrics['test_brier']:.4f}")
    print(f"  Test Teacher Prob MSE:    {metrics['test_teacher_mse']:.6f}")
    print(f"  Test Pearson Corr(E, q):  {metrics['test_corr']:+.4f}")
    print(f"  Test Centipawn MAE(E, q): {metrics['test_mae_cp']:.2f} cp")
    print(f"  Activation Saturation H1: 0%={metrics['h1_zero_pct']:.1f}%, 127={metrics['h1_sat_pct']:.1f}%")
    print(f"  Activation Saturation H2: 0%={metrics['h2_zero_pct']:.1f}%, 127={metrics['h2_sat_pct']:.1f}%")
    print(f"  Output Score Stats:       Mean={metrics['mean_E']:+.2f} cp, Std={metrics['std_E']:.2f} cp")
    print(f"================================================================================\n")

    return metrics


def run_layerwise_quantization_diagnostics(model: BosonNNUE,
                                          val_dataset: BosonDataset,
                                          batch_size: int = 128) -> Dict[str, Dict[str, float]]:
    """
    Phase 8-C1 Preparation: Comprehensive Layer-by-Layer Quantization Error Analysis.
    Computes error distributions across validation split (N = 300) for:
      - Accumulator output (A_float vs A_int16)
      - FC1 linear output and post-CReLU activations (h1_float vs h1_int8)
      - FC2 linear output and post-CReLU activations (h2_float vs h2_int8)
      - FC3 output and final centipawns (E_float vs E_shadow)
    """
    model.eval()
    val_loader = get_dataloader(val_dataset, batch_size=batch_size, shuffle=False)

    all_d_acc = []
    all_d_u1, all_d_h1 = [], []
    all_d_u2, all_d_h2 = [], []
    all_d_E = []

    h1_zeros, h1_sats = [], []
    h2_zeros, h2_sats = [], []

    # Integer weights derived from continuous float parameters matching scaling bridge
    w1_int = torch.clamp(torch.round(model.fc1.weight.data * (64.0 / 127.0)), -128, 127).to(torch.int32)
    b1_int = torch.round(model.fc1.bias.data * 64.0).to(torch.int32)

    w2_int = torch.clamp(torch.round(model.fc2.weight.data * 64.0), -128, 127).to(torch.int32)
    b2_int = torch.round(model.fc2.bias.data * 64.0).to(torch.int32)

    w3_int = torch.clamp(torch.round(model.fc3.weight.data / 16.0), -128, 127).to(torch.int32)
    b3_int = torch.round(model.fc3.bias.data / 16.0).to(torch.int32)

    with torch.no_grad():
        for batch in val_loader:
            acc = batch["accumulator"]

            # 1. Accumulator Layer
            acc_int16 = torch.clamp(torch.round(acc), -32768, 32767).to(torch.int32)
            d_acc = torch.abs(acc - acc_int16.float())
            all_d_acc.append(d_acc.flatten())

            input_act_int = torch.clamp(acc_int16, 0, 127)
            x_norm = torch.clamp(acc, 0.0, 127.0) / 127.0

            # 2. FC1 Layer
            u1_float = model.fc1(x_norm)
            h1_float = torch.clamp(u1_float, 0.0, 127.0)

            u1_raw_int = b1_int.unsqueeze(0) + torch.matmul(input_act_int, w1_int.t())
            u1_scaled_int = torch.trunc(u1_raw_int.float() / 64.0).to(torch.int32)
            h1_int8 = torch.clamp(u1_scaled_int, 0, 127)

            d_u1 = torch.abs(u1_float - u1_scaled_int.float())
            d_h1 = torch.abs(h1_float - h1_int8.float())
            all_d_u1.append(d_u1.flatten())
            all_d_h1.append(d_h1.flatten())

            h1_zeros.append((h1_float == 0.0).float().mean().item())
            h1_sats.append((h1_float == 127.0).float().mean().item())

            # 3. FC2 Layer
            u2_float = model.fc2(h1_float)
            h2_float = torch.clamp(u2_float, 0.0, 127.0)

            u2_raw_int = b2_int.unsqueeze(0) + torch.matmul(h1_int8, w2_int.t())
            u2_scaled_int = torch.trunc(u2_raw_int.float() / 64.0).to(torch.int32)
            h2_int8 = torch.clamp(u2_scaled_int, 0, 127)

            d_u2 = torch.abs(u2_float - u2_scaled_int.float())
            d_h2 = torch.abs(h2_float - h2_int8.float())
            all_d_u2.append(d_u2.flatten())
            all_d_h2.append(d_h2.flatten())

            h2_zeros.append((h2_float == 0.0).float().mean().item())
            h2_sats.append((h2_float == 127.0).float().mean().item())

            # 4. FC3 Layer
            E_float = model.fc3(h2_float).squeeze(-1)
            u3_raw_int = b3_int.unsqueeze(0) + torch.matmul(h2_int8, w3_int.t())
            score_int = u3_raw_int.squeeze(-1) * 16
            E_shadow = torch.clamp(score_int, -30000, 30000).float()

            d_E = torch.abs(E_float - E_shadow)
            all_d_E.append(d_E.flatten())

    def compute_stats(deltas: torch.Tensor) -> Dict[str, float]:
        deltas = deltas.float()
        mae = torch.mean(deltas).item()
        rmse = torch.sqrt(torch.mean(deltas ** 2)).item()
        max_err = torch.max(deltas).item()
        p95 = torch.quantile(deltas, 0.95).item()
        p99 = torch.quantile(deltas, 0.99).item()
        return {"mae": mae, "rmse": rmse, "max": max_err, "p95": p95, "p99": p99}

    stats_acc = compute_stats(torch.cat(all_d_acc))
    stats_u1  = compute_stats(torch.cat(all_d_u1))
    stats_h1  = compute_stats(torch.cat(all_d_h1))
    stats_u2  = compute_stats(torch.cat(all_d_u2))
    stats_h2  = compute_stats(torch.cat(all_d_h2))
    stats_E   = compute_stats(torch.cat(all_d_E))

    h1_zero_pct = sum(h1_zeros) / len(h1_zeros) * 100.0
    h1_sat_pct  = sum(h1_sats) / len(h1_sats) * 100.0
    h2_zero_pct = sum(h2_zeros) / len(h2_zeros) * 100.0
    h2_sat_pct  = sum(h2_sats) / len(h2_sats) * 100.0

    print(f"\n=========================================================================================================")
    print(f"===   LAYER-WISE QUANTIZATION FIDELITY & ERROR DISTRIBUTION (N = {len(val_dataset)} Validation Records)      ===")
    print(f"=========================================================================================================")
    print(f"| {'Layer / Stage':<22} | {'MAE':<8} | {'RMSE':<8} | {'Max |Delta|':<11} | {'P95(|Delta|)':<12} | {'P99(|Delta|)':<12} | {'0% Dead':<8} | {'127 Sat%':<8} |")
    print(f"|:{'-'*22}-|-{'-'*8}:|-{'-'*8}:|-{'-'*11}:|-{'-'*12}:|-{'-'*12}:|-{'-'*8}:|-{'-'*8}:|")
    print(f"| {'Accumulator (A)':<22} | {stats_acc['mae']:8.4f} | {stats_acc['rmse']:8.4f} | {stats_acc['max']:11.4f} | {stats_acc['p95']:12.4f} | {stats_acc['p99']:12.4f} | {'N/A':^8} | {'N/A':^8} |")
    print(f"| {'FC1 Linear (u1)':<22} | {stats_u1['mae']:8.4f} | {stats_u1['rmse']:8.4f} | {stats_u1['max']:11.4f} | {stats_u1['p95']:12.4f} | {stats_u1['p99']:12.4f} | {'N/A':^8} | {'N/A':^8} |")
    print(f"| {'FC1 CReLU (h1)':<22}  | {stats_h1['mae']:8.4f} | {stats_h1['rmse']:8.4f} | {stats_h1['max']:11.4f} | {stats_h1['p95']:12.4f} | {stats_h1['p99']:12.4f} | {h1_zero_pct:7.1f}% | {h1_sat_pct:7.1f}% |")
    print(f"| {'FC2 Linear (u2)':<22} | {stats_u2['mae']:8.4f} | {stats_u2['rmse']:8.4f} | {stats_u2['max']:11.4f} | {stats_u2['p95']:12.4f} | {stats_u2['p99']:12.4f} | {'N/A':^8} | {'N/A':^8} |")
    print(f"| {'FC2 CReLU (h2)':<22}  | {stats_h2['mae']:8.4f} | {stats_h2['rmse']:8.4f} | {stats_h2['max']:11.4f} | {stats_h2['p95']:12.4f} | {stats_h2['p99']:12.4f} | {h2_zero_pct:7.1f}% | {h2_sat_pct:7.1f}% |")
    print(f"| {'FC3 Output (E, cp)':<22} | {stats_E['mae']:8.2f} | {stats_E['rmse']:8.2f} | {stats_E['max']:11.2f} | {stats_E['p95']:12.2f} | {stats_E['p99']:12.2f} | {'N/A':^8} | {'N/A':^8} |")
    print(f"=========================================================================================================\n")

    return {
        "accumulator": stats_acc,
        "fc1_linear": stats_u1,
        "fc1_crelu": stats_h1,
        "fc2_linear": stats_u2,
        "fc2_crelu": stats_h2,
        "fc3_output": stats_E,
        "h1_zero_pct": h1_zero_pct,
        "h1_sat_pct": h1_sat_pct,
        "h2_zero_pct": h2_zero_pct,
        "h2_sat_pct": h2_sat_pct,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Boson NNUE Evaluation & Shadow Diagnostics")
    parser.add_argument("--checkpoint", default="checkpoints/Exp_C3_Blended_a075_best.pt")
    parser.add_argument("--val_path", default="data/dataset_phase8a/val.bin")
    parser.add_argument("--test_path", default="data/dataset_phase8a/test.bin")
    parser.add_argument("--shadow_check", action="store_true", default=False)
    parser.add_argument("--evaluate_test", action="store_true", default=False)
    parser.add_argument("--layerwise-quant", "--layerwise_quant", action="store_true", default=False)
    args = parser.parse_args()

    # If no explicit flag passed, run standard suite
    if not (args.shadow_check or args.evaluate_test or args.layerwise_quant):
        args.shadow_check = True
        args.evaluate_test = True
        args.layerwise_quant = True

    if os.path.exists(args.checkpoint):
        ckpt = torch.load(args.checkpoint, map_location="cpu", weights_only=False)
        model = BosonNNUE(mode=ckpt.get("mode", "P2"))
        model.load_state_dict(ckpt["model_state_dict"])
        val_ds = BosonDataset(args.val_path, precompute_accumulators=True)

        if args.shadow_check:
            run_quantization_shadow_check(model, val_ds)

        if args.layerwise_quant:
            run_layerwise_quantization_diagnostics(model, val_ds)

        if args.evaluate_test:
            evaluate_on_test_set(args.checkpoint, args.test_path)
