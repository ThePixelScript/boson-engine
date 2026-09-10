"""
Boson Supervised NNUE Training Engine (Phase 8-B).
Executes Micro-Baseline Experiments A, B1, B2, C1, C2, C3, and Mode P1 validation.
"""

import os
import sys
import math
import json
import argparse
from typing import Dict, List, Tuple, Optional
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader

from tools.training.model import BosonNNUE
from tools.training.dataset import BosonDataset, get_dataloader

T_E = 400.0
T_Q = 400.0
LN10_DIV_400 = math.log(10.0) / 400.0


def compute_logistic_prob(cp: torch.Tensor, scale: float = LN10_DIV_400) -> torch.Tensor:
    """Computes win probability p = 1 / (1 + 10^(-cp/400)) = sigmoid(cp * ln(10) / 400)."""
    return torch.sigmoid(cp * scale)


def compute_pearson_corr(x: torch.Tensor, y: torch.Tensor) -> float:
    """Computes Pearson correlation coefficient Corr(x, y)."""
    if x.numel() <= 1:
        return 0.0
    vx = x - torch.mean(x)
    vy = y - torch.mean(y)
    cov = torch.sum(vx * vy)
    denom = torch.sqrt(torch.sum(vx ** 2) * torch.sum(vy ** 2))
    if denom.item() == 0.0 or torch.isnan(denom):
        return 0.0
    r = (cov / denom).item()
    return max(-1.0, min(1.0, r))


class BosonTrainer:
    def __init__(self,
                 exp_name: str,
                 alpha: float = 1.0,
                 loss_type: str = "BCE",  # "BCE", "DISTILL_MSE", "DIRECT_MSE", "BLENDED"
                 mode: str = "P2",
                 batch_size: int = 128,
                 lr: float = 1e-3,
                 weight_decay: float = 1e-4,
                 epochs: int = 30,
                 checkpoint_dir: str = "checkpoints",
                 weights_path: Optional[str] = "boson-v1.nnue"):
        self.exp_name = exp_name
        self.alpha = alpha
        self.loss_type = loss_type
        self.mode = mode
        self.batch_size = batch_size
        self.lr = lr
        self.weight_decay = weight_decay
        self.epochs = epochs
        self.checkpoint_dir = checkpoint_dir
        self.weights_path = weights_path

        os.makedirs(self.checkpoint_dir, exist_ok=True)

        self.model = BosonNNUE(mode=self.mode, weights_path=self.weights_path)
        # Trainable parameters
        trainable_params = [p for p in self.model.parameters() if p.requires_grad]
        self.optimizer = optim.AdamW(trainable_params, lr=self.lr, weight_decay=self.weight_decay)

    def compute_loss(self, E: torch.Tensor, z: torch.Tensor, q: torch.Tensor) -> Tuple[torch.Tensor, Dict[str, float]]:
        """
        Compute loss matching experiment specification:
          - Exp A: Outcome-Only BCE (alpha = 1.0)
          - Exp B1: Teacher-Probability Distillation MSE (alpha = 0.0, T_q = 400)
          - Exp B2: Direct Normalized Centipawn MSE (E/400 vs q/400)
          - Exp C: Blended objective alpha * BCE + (1 - alpha) * Distill_MSE
        """
        logit_E = E * LN10_DIV_400
        p_model = torch.sigmoid(logit_E)
        p_teacher = compute_logistic_prob(q)

        loss_bce = nn.functional.binary_cross_entropy_with_logits(logit_E, z)
        loss_distill = nn.functional.mse_loss(p_model, p_teacher)
        loss_direct = nn.functional.mse_loss(E / 400.0, q / 400.0)

        if self.loss_type == "BCE":
            total_loss = loss_bce
        elif self.loss_type == "DISTILL_MSE":
            total_loss = loss_distill
        elif self.loss_type == "DIRECT_MSE":
            total_loss = loss_direct
        elif self.loss_type == "BLENDED":
            total_loss = self.alpha * loss_bce + (1.0 - self.alpha) * loss_distill
        else:
            raise ValueError(f"Unknown loss type: {self.loss_type}")

        telemetry = {
            "bce": loss_bce.item(),
            "distill_mse": loss_distill.item(),
            "direct_mse": loss_direct.item(),
            "total_loss": total_loss.item()
        }
        return total_loss, telemetry

    def train_epoch(self, train_loader: DataLoader) -> Dict[str, float]:
        self.model.train()
        total_loss = 0.0
        total_bce = 0.0
        total_distill = 0.0
        n_batches = 0

        for batch in train_loader:
            acc = batch["accumulator"]
            z = batch["z_stm"]
            q = batch["q_stm"]

            self.optimizer.zero_grad()
            E = self.model(acc)
            loss, telemetry = self.compute_loss(E, z, q)
            loss.backward()
            self.optimizer.step()

            total_loss += telemetry["total_loss"]
            total_bce += telemetry["bce"]
            total_distill += telemetry["distill_mse"]
            n_batches += 1

        return {
            "train_loss": total_loss / max(1, n_batches),
            "train_bce": total_bce / max(1, n_batches),
            "train_distill": total_distill / max(1, n_batches)
        }

    @torch.no_grad()
    def evaluate(self, val_loader: DataLoader) -> Dict[str, float]:
        self.model.eval()
        all_E = []
        all_z = []
        all_q = []
        total_loss = 0.0
        n_batches = 0

        h1_zeros, h1_sats = [], []
        h2_zeros, h2_sats = [], []

        for batch in val_loader:
            acc = batch["accumulator"]
            z = batch["z_stm"]
            q = batch["q_stm"]

            E, diag = self.model.forward_with_diagnostics(acc)
            loss, _ = self.compute_loss(E, z, q)

            total_loss += loss.item()
            n_batches += 1

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

        val_bce = nn.functional.binary_cross_entropy_with_logits(cat_E * LN10_DIV_400, cat_z).item()
        val_brier = torch.mean((p_model - cat_z) ** 2).item()
        val_teacher_mse = torch.mean((p_model - p_teacher) ** 2).item()
        val_corr = compute_pearson_corr(cat_E, cat_q)

        return {
            "val_loss": total_loss / max(1, n_batches),
            "val_bce": val_bce,
            "val_brier": val_brier,
            "val_teacher_mse": val_teacher_mse,
            "val_corr": val_corr,
            "h1_zero_pct": sum(h1_zeros) / max(1, len(h1_zeros)),
            "h1_sat_pct": sum(h1_sats) / max(1, len(h1_sats)),
            "h2_zero_pct": sum(h2_zeros) / max(1, len(h2_zeros)),
            "h2_sat_pct": sum(h2_sats) / max(1, len(h2_sats)),
            "mean_E": cat_E.mean().item(),
            "std_E": cat_E.std().item()
        }

    def run(self, train_loader: DataLoader, val_loader: DataLoader) -> Dict:
        best_val_loss = float("inf")
        best_epoch = -1
        best_metrics = {}
        history = []

        print(f"\n================================================================================")
        print(f"  STARTING RUN: {self.exp_name} | Mode: {self.mode} | Loss: {self.loss_type} (alpha={self.alpha})")
        print(f"================================================================================")

        for epoch in range(1, self.epochs + 1):
            train_m = self.train_epoch(train_loader)
            val_m = self.evaluate(val_loader)

            epoch_record = {
                "epoch": epoch,
                **train_m,
                **val_m
            }
            history.append(epoch_record)

            is_best = val_m["val_loss"] < best_val_loss
            if is_best:
                best_val_loss = val_m["val_loss"]
                best_epoch = epoch
                best_metrics = val_m.copy()
                best_metrics["epoch"] = best_epoch

                ckpt_path = os.path.join(self.checkpoint_dir, f"{self.exp_name}_best.pt")
                torch.save({
                    "exp_name": self.exp_name,
                    "mode": self.mode,
                    "loss_type": self.loss_type,
                    "alpha": self.alpha,
                    "epoch": epoch,
                    "model_state_dict": self.model.state_dict(),
                    "val_metrics": best_metrics
                }, ckpt_path)

            if epoch % 5 == 0 or epoch == self.epochs or is_best:
                print(f"  Epoch {epoch:02d}/{self.epochs:02d} | TrainLoss: {train_m['train_loss']:.4f} | "
                      f"ValLoss: {val_m['val_loss']:.4f} | ValBCE: {val_m['val_bce']:.4f} | "
                      f"ValBrier: {val_m['val_brier']:.4f} | ValCorr: {val_m['val_corr']:+.4f} | "
                      f"H1(0%/127%): {val_m['h1_zero_pct']:.1f}%/{val_m['h1_sat_pct']:.1f}% | "
                      f"{'[*BEST*]' if is_best else ''}")

        print(f"  --> Run {self.exp_name} Complete. Best Epoch: {best_epoch} (Val Loss: {best_val_loss:.4f})")
        return {
            "exp_name": self.exp_name,
            "best_epoch": best_epoch,
            "best_metrics": best_metrics,
            "history": history
        }


def run_all_experiments(train_path: str = "data/dataset_phase8a/train.bin",
                        val_path: str = "data/dataset_phase8a/val.bin",
                        checkpoint_dir: str = "checkpoints") -> Dict[str, Dict]:
    """Execute Exp A, B1, B2, C1, C2, C3, plus Mode P1 validation."""
    torch.manual_seed(42)

    print(f"Loading Datasets: Train from {train_path}, Val from {val_path}...")
    train_dataset = BosonDataset(train_path, precompute_accumulators=True)
    val_dataset = BosonDataset(val_path, precompute_accumulators=True)

    train_loader = get_dataloader(train_dataset, batch_size=128, shuffle=True)
    val_loader = get_dataloader(val_dataset, batch_size=128, shuffle=False)

    print(f"Datasets Loaded: Train={len(train_dataset)} records, Val={len(val_dataset)} records.")

    experiments = [
        # Exp A: Outcome-Only BCE (alpha = 1.0)
        {"exp_name": "Exp_A_OutcomeBCE", "alpha": 1.0, "loss_type": "BCE", "mode": "P2"},
        # Exp B1: Teacher-Probability Distillation MSE (alpha = 0.0, T_q = 400)
        {"exp_name": "Exp_B1_TeacherDistillMSE", "alpha": 0.0, "loss_type": "DISTILL_MSE", "mode": "P2"},
        # Exp B2: Direct Normalized Centipawn MSE (E/400 vs q/400)
        {"exp_name": "Exp_B2_DirectCpMSE", "alpha": 0.0, "loss_type": "DIRECT_MSE", "mode": "P2"},
        # Exp C1: Blended objective alpha = 0.25
        {"exp_name": "Exp_C1_Blended_a025", "alpha": 0.25, "loss_type": "BLENDED", "mode": "P2"},
        # Exp C2: Blended objective alpha = 0.50
        {"exp_name": "Exp_C2_Blended_a050", "alpha": 0.50, "loss_type": "BLENDED", "mode": "P2"},
        # Exp C3: Blended objective alpha = 0.75
        {"exp_name": "Exp_C3_Blended_a075", "alpha": 0.75, "loss_type": "BLENDED", "mode": "P2"},
        # Mode P1: Pipeline validation run
        {"exp_name": "Exp_P1_FullNetworkValidation", "alpha": 0.50, "loss_type": "BLENDED", "mode": "P1"},
    ]

    results = {}
    for exp_cfg in experiments:
        torch.manual_seed(42)  # Strict reproducible initialization across baselines
        trainer = BosonTrainer(
            exp_name=exp_cfg["exp_name"],
            alpha=exp_cfg["alpha"],
            loss_type=exp_cfg["loss_type"],
            mode=exp_cfg["mode"],
            batch_size=128,
            lr=1e-3,
            weight_decay=1e-4,
            epochs=25,
            checkpoint_dir=checkpoint_dir
        )
        res = trainer.run(train_loader, val_loader)
        results[exp_cfg["exp_name"]] = res

    # Write summary results to JSON
    summary_path = os.path.join(checkpoint_dir, "phase8b_experiments_summary.json")
    with open(summary_path, "w") as f:
        json.dump(results, f, indent=2)

    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Boson Phase 8-B Training Pipeline")
    parser.add_argument("--train_path", default="data/dataset_phase8a/train.bin")
    parser.add_argument("--val_path", default="data/dataset_phase8a/val.bin")
    parser.add_argument("--checkpoint_dir", default="checkpoints")
    args = parser.parse_args()

    run_all_experiments(args.train_path, args.val_path, args.checkpoint_dir)
