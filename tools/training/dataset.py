"""
Boson NNUE PyTorch Dataset & DataLoader (Phase 8-B).
Ingests BOSN_DS1 binary dataset files with feature caching and perspective alignment.
"""

import os
import struct
from typing import List, Tuple, Optional, Dict
import torch
from torch.utils.data import Dataset, DataLoader

MAGIC = b"BOSN_DS1"
HEADER_SIZE = 64
ACCUMULATOR_SIZE = 512
FC1_INPUT_SIZE = 1024


class BosonDatasetRecord:
    __slots__ = ("position_hash", "side_to_move", "metadata_flags", "z_stm", "q_stm",
                 "us_features", "them_features", "accumulator")

    def __init__(self,
                 position_hash: int,
                 side_to_move: int,
                 metadata_flags: int,
                 z_stm: float,
                 q_stm: int,
                 us_features: List[int],
                 them_features: List[int],
                 accumulator: Optional[torch.Tensor] = None):
        self.position_hash = position_hash
        self.side_to_move = side_to_move
        self.metadata_flags = metadata_flags
        self.z_stm = z_stm
        self.q_stm = q_stm
        self.us_features = us_features
        self.them_features = them_features
        self.accumulator = accumulator


class BosonDataset(Dataset):
    """
    Direct binary reader for BOSN_DS1 datasets with optional
    pre-computed frozen accumulator caching for ultra-fast training.
    """

    def __init__(self,
                 filepath: str,
                 precompute_accumulators: bool = True,
                 weights_path: Optional[str] = "boson-v1.nnue"):
        self.filepath = filepath
        self.records: List[BosonDatasetRecord] = []
        self.header_info: Dict = {}

        if not os.path.exists(filepath):
            raise FileNotFoundError(f"Dataset file not found: {filepath}")

        self._read_binary(filepath)

        if precompute_accumulators:
            self._precompute_accumulators(weights_path)

    def _read_binary(self, filepath: str):
        with open(filepath, "rb") as f:
            hdr_bytes = f.read(HEADER_SIZE)
            if len(hdr_bytes) < HEADER_SIZE:
                raise ValueError(f"Invalid dataset header in {filepath}: too short")
            magic, fmt_v, feat_v, teach_v, teach_d, rec_cnt, split_id, _ = struct.unpack("<8sIIIIQB31s", hdr_bytes)
            if magic != MAGIC:
                raise ValueError(f"Invalid magic: {magic} (expected {MAGIC})")

            self.header_info = {
                "magic": magic.decode("ascii", errors="replace"),
                "format_version": fmt_v,
                "feature_version": feat_v,
                "teacher_version": teach_v,
                "teacher_depth": teach_d,
                "record_count": rec_cnt,
                "split_id": split_id
            }

            for _ in range(rec_cnt):
                pfx = f.read(16)
                if not pfx or len(pfx) < 16:
                    break
                pos_hash, stm, flags, z, q = struct.unpack("<QBBfh", pfx)

                nw_b = f.read(1)
                if not nw_b:
                    break
                nw = nw_b[0]
                w_feats = []
                if nw > 0:
                    w_feats = list(struct.unpack(f"<{nw}H", f.read(nw * 2)))

                nb_b = f.read(1)
                if not nb_b:
                    break
                nb = nb_b[0]
                b_feats = []
                if nb > 0:
                    b_feats = list(struct.unpack(f"<{nb}H", f.read(nb * 2)))

                # Perspective alignment:
                # If side_to_move == 0 (White): Us = White, Them = Black
                # If side_to_move == 1 (Black): Us = Black, Them = White
                if stm == 0:
                    us_f = w_feats
                    them_f = b_feats
                else:
                    us_f = b_feats
                    them_f = w_feats

                rec = BosonDatasetRecord(
                    position_hash=pos_hash,
                    side_to_move=stm,
                    metadata_flags=flags,
                    z_stm=float(z),
                    q_stm=int(q),
                    us_features=us_f,
                    them_features=them_f
                )
                self.records.append(rec)

    def _precompute_accumulators(self, weights_path: Optional[str]):
        """Precompute frozen accumulator values [1024] for all records."""
        from tools.training.model import xorshift64_deterministic_weights, HALFKP_FEATURES, ACCUMULATOR_SIZE

        if weights_path and os.path.exists(weights_path):
            with open(weights_path, "rb") as f:
                _ = f.read(28)
                b_bytes = f.read(ACCUMULATOR_SIZE * 2)
                w_bytes = f.read(HALFKP_FEATURES * ACCUMULATOR_SIZE * 2)
                b = torch.frombuffer(bytearray(b_bytes), dtype=torch.int16).float()
                w = torch.frombuffer(bytearray(w_bytes), dtype=torch.int16).float().reshape(HALFKP_FEATURES, ACCUMULATOR_SIZE)
        else:
            b, w = xorshift64_deterministic_weights(42)

        for rec in self.records:
            acc = torch.zeros(FC1_INPUT_SIZE, dtype=torch.float32)
            # Us half
            us_acc = b.clone()
            if rec.us_features:
                us_acc = us_acc + w[rec.us_features].sum(dim=0)
            # Them half
            them_acc = b.clone()
            if rec.them_features:
                them_acc = them_acc + w[rec.them_features].sum(dim=0)

            acc[:ACCUMULATOR_SIZE] = us_acc
            acc[ACCUMULATOR_SIZE:] = them_acc
            rec.accumulator = acc

    def __len__(self) -> int:
        return len(self.records)

    def __getitem__(self, idx: int) -> Dict[str, torch.Tensor]:
        rec = self.records[idx]
        item = {
            "z_stm": torch.tensor(rec.z_stm, dtype=torch.float32),
            "q_stm": torch.tensor(rec.q_stm, dtype=torch.float32),
            "side_to_move": torch.tensor(rec.side_to_move, dtype=torch.long),
            "metadata_flags": torch.tensor(rec.metadata_flags, dtype=torch.long)
        }
        if rec.accumulator is not None:
            item["accumulator"] = rec.accumulator
        return item


def collate_boson_batch(batch: List[Dict[str, torch.Tensor]]) -> Dict[str, torch.Tensor]:
    """Collates a list of dataset items into batched tensors."""
    collated = {
        "z_stm": torch.stack([item["z_stm"] for item in batch]),
        "q_stm": torch.stack([item["q_stm"] for item in batch]),
        "side_to_move": torch.stack([item["side_to_move"] for item in batch]),
        "metadata_flags": torch.stack([item["metadata_flags"] for item in batch]),
    }
    if "accumulator" in batch[0]:
        collated["accumulator"] = torch.stack([item["accumulator"] for item in batch])
    return collated


def get_dataloader(dataset: BosonDataset,
                   batch_size: int = 128,
                   shuffle: bool = True,
                   num_workers: int = 0) -> DataLoader:
    return DataLoader(
        dataset,
        batch_size=batch_size,
        shuffle=shuffle,
        num_workers=num_workers,
        collate_fn=collate_boson_batch
    )
