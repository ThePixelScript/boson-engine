"""
Boson Dataset Diagnostics & Audit Suite.
Calculates dataset distributions, flag prevalence, P(z=1 | q) calibration,
and cryptographic integrity manifests.
"""

import math
import os
import sys
import hashlib
from typing import List, Dict, Tuple

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))

from tools.dataset.schema import (
    read_dataset_file, DatasetHeader, DatasetRecord,
    FLAG_IN_CHECK, FLAG_HAS_CAPTURE, FLAG_HAS_PROMO, FLAG_HAS_EP,
    SPLIT_NAMES
)


def compute_file_sha256(filepath: str) -> str:
    """Compute SHA-256 digest of a file."""
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()


def logistic_win_probability(q_cp: float) -> float:
    """Heuristic Teacher Mapping: σ(q/400) = 1 / (1 + 10^(-q / 400))."""
    return 1.0 / (1.0 + math.pow(10.0, -q_cp / 400.0))


class DatasetDiagnostics:
    """Analyzes DatasetRecord collections and generates verification reports."""

    def __init__(self, records: List[DatasetRecord], split_name: str = "combined"):
        self.records = records
        self.split_name = split_name
        self.total_records = len(records)

    def analyze(self) -> Dict:
        if self.total_records == 0:
            return {"total": 0}

        # 1. Outcomes
        wins = sum(1 for r in self.records if abs(r.z_stm - 1.0) < 1e-4)
        draws = sum(1 for r in self.records if abs(r.z_stm - 0.5) < 1e-4)
        losses = sum(1 for r in self.records if abs(r.z_stm - 0.0) < 1e-4)

        # 2. Teacher evaluations
        scores = [r.q_stm for r in self.records]
        scores_sorted = sorted(scores)
        mean_q = sum(scores) / self.total_records
        median_q = scores_sorted[self.total_records // 2]
        var_q = sum((s - mean_q) ** 2 for s in scores) / max(1, self.total_records - 1)
        std_q = math.sqrt(var_q)

        # 3. Flags
        in_check_cnt = sum(1 for r in self.records if r.metadata_flags & FLAG_IN_CHECK)
        has_capture_cnt = sum(1 for r in self.records if r.metadata_flags & FLAG_HAS_CAPTURE)
        has_promo_cnt = sum(1 for r in self.records if r.metadata_flags & FLAG_HAS_PROMO)
        has_ep_cnt = sum(1 for r in self.records if r.metadata_flags & FLAG_HAS_EP)

        # 4. Calibration bins for P(z=1 | q)
        # Bins: (-inf, -300), [-300, -100), [-100, 0), [0, 100], (100, 300], (300, inf)
        bins = [
            ("< -300 cp", lambda q: q < -300),
            ("[-300, -100) cp", lambda q: -300 <= q < -100),
            ("[-100, 0) cp", lambda q: -100 <= q < 0),
            ("[0, +100] cp", lambda q: 0 <= q <= 100),
            ("(+100, +300] cp", lambda q: 100 < q <= 300),
            ("> +300 cp", lambda q: q > 300)
        ]

        calibration_data = []
        for label, pred in bins:
            bin_recs = [r for r in self.records if pred(r.q_stm)]
            n = len(bin_recs)
            if n > 0:
                bin_wins = sum(1 for r in bin_recs if abs(r.z_stm - 1.0) < 1e-4)
                bin_score = sum(r.z_stm for r in bin_recs) / n
                win_pct = (bin_wins / n) * 100.0
                mean_bin_q = sum(r.q_stm for r in bin_recs) / n
                expected_score = logistic_win_probability(mean_bin_q) * 100.0
            else:
                bin_wins = 0
                win_pct = 0.0
                bin_score = 0.0
                mean_bin_q = 0.0
                expected_score = 50.0

            calibration_data.append({
                "label": label,
                "count": n,
                "pct_of_total": (n / self.total_records) * 100.0,
                "win_rate": win_pct,
                "empirical_score": bin_score * 100.0,
                "mean_q": mean_bin_q,
                "expected_logistic": expected_score
            })

        return {
            "split": self.split_name,
            "total_records": self.total_records,
            "outcomes": {
                "wins": wins, "win_pct": (wins / self.total_records) * 100.0,
                "draws": draws, "draw_pct": (draws / self.total_records) * 100.0,
                "losses": losses, "loss_pct": (losses / self.total_records) * 100.0
            },
            "eval_stats": {
                "min": scores_sorted[0],
                "max": scores_sorted[-1],
                "mean": mean_q,
                "median": median_q,
                "std": std_q
            },
            "flags": {
                "in_check": in_check_cnt, "in_check_pct": (in_check_cnt / self.total_records) * 100.0,
                "has_capture": has_capture_cnt, "has_capture_pct": (has_capture_cnt / self.total_records) * 100.0,
                "has_promo": has_promo_cnt, "has_promo_pct": (has_promo_cnt / self.total_records) * 100.0,
                "has_ep": has_ep_cnt, "has_ep_pct": (has_ep_cnt / self.total_records) * 100.0
            },
            "calibration": calibration_data
        }

    def generate_report(self) -> str:
        res = self.analyze()
        if res.get("total_records", 0) == 0:
            return f"Dataset [{self.split_name}]: 0 records\n"

        lines = [
            f"===================================================================================================",
            f"                             BOSON DATASET AUDIT REPORT: [{self.split_name.upper()}]",
            f"===================================================================================================",
            f"  Total Records:          {res['total_records']}",
            f"  Outcomes (z_stm):       Wins: {res['outcomes']['wins']} ({res['outcomes']['win_pct']:.1f}%), "
            f"Draws: {res['outcomes']['draws']} ({res['outcomes']['draw_pct']:.1f}%), "
            f"Losses: {res['outcomes']['losses']} ({res['outcomes']['loss_pct']:.1f}%)",
            f"  Teacher Scores (q_stm): Mean: {res['eval_stats']['mean']:.1f} cp | "
            f"Median: {res['eval_stats']['median']} cp | Std: {res['eval_stats']['std']:.1f} cp | "
            f"Range: [{res['eval_stats']['min']}, {res['eval_stats']['max']}] cp",
            f"  Metadata Flags:         In-Check: {res['flags']['in_check']} ({res['flags']['in_check_pct']:.1f}%) | "
            f"Capture: {res['flags']['has_capture']} ({res['flags']['has_capture_pct']:.1f}%) | "
            f"EP: {res['flags']['has_ep']} ({res['flags']['has_ep_pct']:.1f}%) | "
            f"Promo: {res['flags']['has_promo']} ({res['flags']['has_promo_pct']:.1f}%)",
            f"",
            f"---------------------------------------------------------------------------------------------------",
            f"                             TEACHER / OUTCOME CALIBRATION: P(z=1 | q)",
            f"---------------------------------------------------------------------------------------------------",
            f"Bin Range            Count    Share    WinRate%   Score%   Mean q   Heuristic Teacher Mapping",
            f"---------------------------------------------------------------------------------------------------"
        ]

        for b in res["calibration"]:
            lines.append(
                f"{b['label']:<18} {b['count']:>7}  {b['pct_of_total']:>6.1f}%  "
                f"{b['win_rate']:>8.1f}%  {b['empirical_score']:>6.1f}%  {b['mean_q']:>6.0f} cp  "
                f"{b['expected_logistic']:>25.1f}%"
            )

        lines.append(f"===================================================================================================")
        return "\n".join(lines)


def audit_dataset_directory(dir_path: str) -> str:
    """Audit train.bin, val.bin, test.bin in a directory and print report."""
    output = []
    output.append(f"================================================================================")
    output.append(f"               BOSON DATASET MANIFEST AUDIT: {dir_path}")
    output.append(f"================================================================================")

    total_all = 0
    splits_found = {}

    for split_id, name in SPLIT_NAMES.items():
        fname = f"{name}.bin"
        fpath = os.path.join(dir_path, fname)
        if os.path.exists(fpath):
            sha = compute_file_sha256(fpath)
            hdr, recs = read_dataset_file(fpath)
            splits_found[name] = (hdr, recs, sha, os.path.getsize(fpath))
            total_all += len(recs)
            output.append(f"[{name.upper()}] File: {fname} | Records: {len(recs)} | Size: {os.path.getsize(fpath)} B")
            output.append(f"  SHA-256: {sha}")
            output.append(f"  Header:  Magic={hdr.magic.decode('ascii', errors='ignore')} Ver={hdr.format_version} "
                          f"Teacher={hdr.teacher_version} Depth={hdr.teacher_depth} Split={hdr.split_id}")

    output.append(f"--------------------------------------------------------------------------------")
    output.append(f"Total Combined Records: {total_all}")

    for name, (hdr, recs, sha, sz) in splits_found.items():
        diag = DatasetDiagnostics(recs, split_name=name)
        output.append("")
        output.append(diag.generate_report())

    return "\n".join(output)


def main():
    if len(sys.argv) < 2:
        print("Usage: python diagnostics.py [--dir <dir> | --file <file>]")
        return

    if "--dir" in sys.argv:
        idx = sys.argv.index("--dir")
        if idx + 1 < len(sys.argv):
            dpath = sys.argv[idx + 1]
            print(audit_dataset_directory(dpath))
    elif "--file" in sys.argv:
        idx = sys.argv.index("--file")
        if idx + 1 < len(sys.argv):
            fpath = sys.argv[idx + 1]
            hdr, recs = read_dataset_file(fpath)
            diag = DatasetDiagnostics(recs, split_name=os.path.basename(fpath))
            print(diag.generate_report())


if __name__ == "__main__":
    main()
