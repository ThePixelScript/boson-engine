import json

with open("checkpoints/phase8b_experiments_summary.json", "r") as f:
    data = json.load(f)

header = f"{'Experiment':<30} | {'Mode':<4} | {'BestEp':<6} | {'ValLoss':<8} | {'ValBCE':<8} | {'ValBrier':<8} | {'TeacherMSE':<10} | {'ValCorr':<8} | {'H1(0%/127%)':<12} | {'H2(0%/127%)':<12}"
print("=" * len(header))
print(header)
print("-" * len(header))

for exp_name, info in data.items():
    m = info["best_metrics"]
    mode = "P1" if "P1" in exp_name else "P2"
    h1_s = f"{m['h1_zero_pct']:.1f}%/{m['h1_sat_pct']:.1f}%"
    h2_s = f"{m['h2_zero_pct']:.1f}%/{m['h2_sat_pct']:.1f}%"
    print(f"{exp_name:<30} | {mode:<4} | {m['epoch']:<6} | {m['val_loss']:<8.4f} | {m['val_bce']:<8.4f} | {m['val_brier']:<8.4f} | {m['val_teacher_mse']:<10.6f} | {m['val_corr']:<+8.4f} | {h1_s:<12} | {h2_s:<12}")

print("=" * len(header))
