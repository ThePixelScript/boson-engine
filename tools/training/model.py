"""
Boson NNUE PyTorch Model Architecture (Phase 8-B).
Float32 HalfKP Network with explicit /64 Hidden-Layer Scaling Bridge
and Quantization Shadow Check integer emulation.
"""

import os
import struct
from typing import Optional, Tuple, List
import torch
import torch.nn as nn
import torch.nn.functional as F

HALFKP_FEATURES = 40960
ACCUMULATOR_SIZE = 512
FC1_INPUT_SIZE = 1024
FC1_OUTPUT_SIZE = 32
FC2_OUTPUT_SIZE = 32


def xorshift64_deterministic_weights(seed: int = 42) -> Tuple[torch.Tensor, torch.Tensor]:
    """
    Generate deterministic int16 FeatureWeights matching Boson's
    FeatureWeights::createDeterministic(seed) in AccumulatorStack.cpp.
    """
    state = 42 if seed == 0 else seed

    def next_rand() -> int:
        nonlocal state
        state ^= (state >> 12) & 0xFFFFFFFFFFFFFFFF
        state ^= (state << 25) & 0xFFFFFFFFFFFFFFFF
        state ^= (state >> 27) & 0xFFFFFFFFFFFFFFFF
        state &= 0xFFFFFFFFFFFFFFFF
        val = (state * 0x2545F4914F6CDD1D) & 0xFFFFFFFFFFFFFFFF
        return val

    biases = torch.zeros(ACCUMULATOR_SIZE, dtype=torch.float32)
    for i in range(ACCUMULATOR_SIZE):
        r = next_rand()
        biases[i] = float(-500 + (r % 1001))

    weights = torch.zeros((HALFKP_FEATURES, ACCUMULATOR_SIZE), dtype=torch.float32)
    for f in range(HALFKP_FEATURES):
        for i in range(ACCUMULATOR_SIZE):
            r = next_rand()
            weights[f, i] = float(-200 + (r % 401))

    return biases, weights


class BosonNNUE(nn.Module):
    """
    Boson NNUE Architecture matching Phase 7 & Phase 8 specifications.
    Layer Topology: (40960 x 2) -> 1024 -> 32 -> 32 -> 1
    Scaling Bridge: Explicit /64 division with [0, 127] CReLU clamping.
    """

    def __init__(self, mode: str = "P2", weights_path: Optional[str] = "boson-v1.nnue"):
        super().__init__()
        self.mode = mode.upper()
        if self.mode not in ("P1", "P2"):
            raise ValueError(f"Invalid mode: {self.mode}. Must be 'P1' (full training) or 'P2' (frozen feature transformer).")

        # Feature Transformer (HalfKP Embeddings)
        self.ft_biases = nn.Parameter(torch.zeros(ACCUMULATOR_SIZE, dtype=torch.float32))
        self.ft_weights = nn.Parameter(torch.zeros(HALFKP_FEATURES, ACCUMULATOR_SIZE, dtype=torch.float32))

        self.init_feature_transformer(weights_path)

        if self.mode == "P2":
            self.ft_biases.requires_grad = False
            self.ft_weights.requires_grad = False
        else:
            self.ft_biases.requires_grad = True
            self.ft_weights.requires_grad = True

        # Hidden Layers matching the /64 Scaling Bridge
        self.fc1 = nn.Linear(FC1_INPUT_SIZE, FC1_OUTPUT_SIZE, bias=True)
        self.fc2 = nn.Linear(FC1_OUTPUT_SIZE, FC2_OUTPUT_SIZE, bias=True)
        self.fc3 = nn.Linear(FC2_OUTPUT_SIZE, 1, bias=True)

        self.reset_fc_parameters()

    def reset_fc_parameters(self):
        """Initialize linear layers with variance-appropriate scaling."""
        nn.init.kaiming_normal_(self.fc1.weight, mode='fan_in', nonlinearity='relu')
        nn.init.zeros_(self.fc1.bias)
        nn.init.kaiming_normal_(self.fc2.weight, mode='fan_in', nonlinearity='relu')
        nn.init.zeros_(self.fc2.bias)
        nn.init.normal_(self.fc3.weight, mean=0.0, std=0.05)
        nn.init.zeros_(self.fc3.bias)

    def init_feature_transformer(self, weights_path: Optional[str]):
        """Load feature weights from existing model file or generate deterministic seed."""
        if weights_path and os.path.exists(weights_path):
            with open(weights_path, "rb") as f:
                header = f.read(28)
                bias_bytes = f.read(ACCUMULATOR_SIZE * 2)
                weights_bytes = f.read(HALFKP_FEATURES * ACCUMULATOR_SIZE * 2)
                b = torch.frombuffer(bytearray(bias_bytes), dtype=torch.int16).float()
                w = torch.frombuffer(bytearray(weights_bytes), dtype=torch.int16).float().reshape(HALFKP_FEATURES, ACCUMULATOR_SIZE)
                self.ft_biases.data.copy_(b)
                self.ft_weights.data.copy_(w)
        else:
            b, w = xorshift64_deterministic_weights(42)
            self.ft_biases.data.copy_(b)
            self.ft_weights.data.copy_(w)

    def compute_accumulator(self, active_features: List[int]) -> torch.Tensor:
        """Compute single accumulator half [512] from active feature indices."""
        acc = self.ft_biases.clone()
        if len(active_features) > 0:
            indices = torch.tensor(active_features, dtype=torch.long, device=self.ft_weights.device)
            acc = acc + self.ft_weights[indices].sum(dim=0)
        return acc

    def forward_from_features(self, us_features_list: List[List[int]], them_features_list: List[List[int]]) -> torch.Tensor:
        """Forward pass starting from active feature indices."""
        batch_size = len(us_features_list)
        acc_batch = torch.zeros((batch_size, FC1_INPUT_SIZE), dtype=torch.float32, device=self.fc1.weight.device)
        for i in range(batch_size):
            us_acc = self.compute_accumulator(us_features_list[i])
            them_acc = self.compute_accumulator(them_features_list[i])
            acc_batch[i, :ACCUMULATOR_SIZE] = us_acc
            acc_batch[i, ACCUMULATOR_SIZE:] = them_acc
        return self.forward(acc_batch)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        """
        Model forward pass with explicit /64 scaling bridge:
          x_norm = clamp(A_float, 0.0, 127.0) / 127.0
          u1 = FC1(x_norm)  --> h1 = clamp(u1, 0.0, 127.0)
          u2 = FC2(h1)      --> h2 = clamp(u2, 0.0, 127.0)
          E = FC3(h2) (linear centipawn output)
        """
        # 1. Feature normalization bridge
        x_norm = torch.clamp(x, 0.0, 127.0) / 127.0

        # 2. Layer 1: FC1 (1024 -> 32)
        u1 = self.fc1(x_norm)
        h1 = torch.clamp(u1, 0.0, 127.0)

        # 3. Layer 2: FC2 (32 -> 32)
        u2 = self.fc2(h1)
        h2 = torch.clamp(u2, 0.0, 127.0)

        # 4. Layer 3: FC3 (32 -> 1) linear centipawn output
        E = self.fc3(h2).squeeze(-1)
        return E

    def forward_with_diagnostics(self, x: torch.Tensor) -> Tuple[torch.Tensor, dict]:
        """Forward pass recording intermediate activation telemetry."""
        x_norm = torch.clamp(x, 0.0, 127.0) / 127.0
        u1 = self.fc1(x_norm)
        h1 = torch.clamp(u1, 0.0, 127.0)
        u2 = self.fc2(h1)
        h2 = torch.clamp(u2, 0.0, 127.0)
        E = self.fc3(h2).squeeze(-1)

        diag = {
            "h1_zero_pct": (h1 == 0.0).float().mean().item() * 100.0,
            "h1_sat_pct": (h1 == 127.0).float().mean().item() * 100.0,
            "h2_zero_pct": (h2 == 0.0).float().mean().item() * 100.0,
            "h2_sat_pct": (h2 == 127.0).float().mean().item() * 100.0,
            "mean_E": E.mean().item(),
            "std_E": E.std().item() if E.numel() > 1 else 0.0
        }
        return E, diag

    @torch.no_grad()
    def quantization_shadow_forward(self, x: torch.Tensor) -> torch.Tensor:
        """
        Emulate exact int16/int8/int32 integer arithmetic on input accumulator batch.
        Matches ScalarInference::evaluateDetailed bit-for-bit.
        """
        # 1. Round accumulator to int16, then clamp to [0, 127]
        acc_int = torch.round(x).to(torch.int32)
        input_act = torch.clamp(acc_int, 0, 127)

        # 2. Quantize float weights into int8/int32 matching bridge scaling
        # FC1:
        w1_int = torch.clamp(torch.round(self.fc1.weight.data * (64.0 / 127.0)), -128, 127).to(torch.int32)
        b1_int = torch.round(self.fc1.bias.data * 64.0).to(torch.int32)

        # FC2:
        w2_int = torch.clamp(torch.round(self.fc2.weight.data * 64.0), -128, 127).to(torch.int32)
        b2_int = torch.round(self.fc2.bias.data * 64.0).to(torch.int32)

        # FC3:
        w3_int = torch.clamp(torch.round(self.fc3.weight.data / 16.0), -128, 127).to(torch.int32)
        b3_int = torch.round(self.fc3.bias.data / 16.0).to(torch.int32)

        # 3. Layer 1 Integer Simulation:
        # u1 = bias + sum(input * w1)
        # scaled = u1 / 64 (truncated toward zero)
        # h1 = crelu(scaled)
        u1_raw = b1_int.unsqueeze(0) + torch.matmul(input_act, w1_int.t())
        u1_scaled = torch.trunc(u1_raw.float() / 64.0).to(torch.int32)
        h1 = torch.clamp(u1_scaled, 0, 127)

        # 4. Layer 2 Integer Simulation:
        u2_raw = b2_int.unsqueeze(0) + torch.matmul(h1, w2_int.t())
        u2_scaled = torch.trunc(u2_raw.float() / 64.0).to(torch.int32)
        h2 = torch.clamp(u2_scaled, 0, 127)

        # 5. Layer 3 Integer Simulation:
        # sum = bias + sum(h2 * w3)
        # score = sum * 16
        u3_raw = b3_int.unsqueeze(0) + torch.matmul(h2, w3_int.t())
        score = u3_raw.squeeze(-1) * 16
        final_score = torch.clamp(score, -30000, 30000).float()

        return final_score
