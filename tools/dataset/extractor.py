"""
Boson Dataset Extractor & Game Splitter.
Extracts positions from games with game-level partitioning, forward sampling,
canonical side-to-move labeling, and cross-split leakage elimination.
"""

import os
import sys
import hashlib
import re
from typing import List, Dict, Tuple, Set, Optional

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))

from tools.dataset.schema import (
    DatasetRecord, DatasetHeader, write_dataset_file,
    SPLIT_TRAIN, SPLIT_VAL, SPLIT_TEST,
    FLAG_IN_CHECK, FLAG_HAS_CAPTURE, FLAG_HAS_PROMO, FLAG_HAS_EP
)
from tools.dataset.halfkp import HalfKPEncoder, parse_fen_simple

MAX_POSITIONS_PER_GAME = 30
MIN_PLY_DELTA = 2
FORCED_MATE_THRESHOLD = 25000


def deterministic_game_hash(game_identifier: str) -> int:
    """Compute deterministic 64-bit integer hash for a game string/ID."""
    digest = hashlib.sha256(game_identifier.encode('utf-8')).hexdigest()
    return int(digest[:16], 16)


CANONICAL_OPENING_IDS = [f"open_{i:02d}" for i in range(1, 51)]


def deterministic_opening_hash(opening_id: str) -> int:
    """Compute deterministic 64-bit FNV-1a hash for an opening ID."""
    h = 14695981039346656037
    for c in opening_id:
        h ^= ord(c)
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def get_opening_split_map() -> Dict[str, int]:
    """
    Deterministically partition 50 canonical opening blocks by hash of opening ID:
    45 blocks Train (180 games, 90%), 3 blocks Validation (12 games, 6%), 2 blocks Test (8 games, 4%).
    """
    sorted_openings = sorted(CANONICAL_OPENING_IDS, key=deterministic_opening_hash)
    split_map = {}
    for i, op_id in enumerate(sorted_openings):
        if i < 45:
            split_map[op_id] = SPLIT_TRAIN
        elif i < 48:
            split_map[op_id] = SPLIT_VAL
        else:
            split_map[op_id] = SPLIT_TEST
    return split_map


OPENING_SPLIT_MAP = get_opening_split_map()


def assign_opening_split(opening_id: str) -> int:
    """Assign opening block to partition atomically."""
    if opening_id in OPENING_SPLIT_MAP:
        return OPENING_SPLIT_MAP[opening_id]
    h = deterministic_opening_hash(opening_id)
    bucket = h % 100
    if bucket < 90:
        return SPLIT_TRAIN
    elif bucket < 96:
        return SPLIT_VAL
    else:
        return SPLIT_TEST


def assign_game_split(game_hash: int) -> int:
    """Assign game to split: 90% Train, 5% Val, 5% Test."""
    bucket = game_hash % 100
    if bucket < 90:
        return SPLIT_TRAIN
    elif bucket < 95:
        return SPLIT_VAL
    else:
        return SPLIT_TEST


def compute_z_stm(result: str, side_to_move: str) -> float:
    """
    Compute game outcome label relative to side to move:
    1.0 = win, 0.5 = draw, 0.0 = loss for side to move.
    """
    result = result.strip()
    stm_white = (side_to_move.lower() == 'w')

    if result in ("1-0", "1"):
        return 1.0 if stm_white else 0.0
    elif result in ("0-1", "0"):
        return 0.0 if stm_white else 1.0
    elif result in ("1/2-1/2", "0.5", "1/2"):
        return 0.5
    else:
        raise ValueError(f"Unrecognized game result: {result}")


def compute_q_stm(q_eval: int, side_to_move: str, is_white_relative: bool = True) -> int:
    """
    Compute teacher score relative to side to move.
    If is_white_relative is True, q_eval is from White's perspective (+ = White advantage).
    """
    if is_white_relative:
        stm_white = (side_to_move.lower() == 'w')
        q = q_eval if stm_white else -q_eval
    else:
        q = q_eval
    # Clamp to int16 range [-30000, 30000]
    return max(-30000, min(30000, q))


def is_eligible_position(q_stm: int, termination_reason: str = "Ordinary") -> Tuple[bool, str]:
    """
    Check if a position is eligible.
    Returns (eligible: bool, reason: str).
    Exclusion reasons: Ordinary (eligible), Forced Mate, Tablebase, Timeout, Search Error.
    """
    if abs(q_stm) >= FORCED_MATE_THRESHOLD:
        return False, "Forced Mate"
    if termination_reason == "Tablebase":
        return False, "Tablebase"
    if termination_reason in ("Timeout", "TimeExpiry"):
        return False, "Timeout"
    if termination_reason in ("SearchError", "EngineCrash", "ProtocolError"):
        return False, "Search Error"
    return True, "Ordinary"


def canonical_position_key(fen: str) -> str:
    """Extract canonical position identity (pieces, active side, castling, en-passant)."""
    parts = fen.strip().split()
    if len(parts) >= 4:
        return f"{parts[0]} {parts[1]} {parts[2]} {parts[3]}"
    return parts[0]


class DatasetExtractor:
    """Manages multi-game position extraction and cross-split leakage elimination."""

    def __init__(self, teacher_version: int = 950, teacher_depth: int = 6):
        self.teacher_version = teacher_version
        self.teacher_depth = teacher_depth

        self.train_records: List[DatasetRecord] = []
        self.val_records: List[DatasetRecord] = []
        self.test_records: List[DatasetRecord] = []

        self.train_keys: Set[str] = set()
        self.val_keys: Set[str] = set()
        self.test_keys: Set[str] = set()

        self.total_games_processed = 0
        self.total_positions_examined = 0
        self.total_positions_extracted = 0
        self.sampling_delta_dropped = 0
        self.sampling_cap_dropped = 0
        self.cross_split_dropped = 0
        self.exclusion_counts = {
            "Ordinary": 0,
            "Forced Mate": 0,
            "Tablebase": 0,
            "Timeout": 0,
            "Search Error": 0
        }

    def process_game(self,
                     game_id: str,
                     positions: List[Dict],
                     result: str,
                     termination_reason: str = "Ordinary",
                     opening_id: Optional[str] = None) -> int:
        """
        Process a sequence of positions from a single game.
        Enforces opening-group atomic splitting when opening_id is provided or extracted from game_id.
        positions: list of dicts with keys:
          - 'ply': int
          - 'fen': str
          - 'q_eval': int (White-relative centipawns)
          - 'flags': int (metadata flags)
          - 'position_hash': int (optional uint64)
        """
        self.total_games_processed += 1

        # Enforce opening-group atomic splitting
        if opening_id:
            split = assign_opening_split(opening_id)
        else:
            m = re.search(r"open_\d+", game_id)
            if m:
                split = assign_opening_split(m.group(0))
            else:
                g_hash = deterministic_game_hash(game_id)
                split = assign_game_split(g_hash)

        sampled_in_game = 0
        last_sampled_ply = -999

        for p_info in positions:
            self.total_positions_examined += 1
            ply = p_info['ply']
            fen = p_info['fen']
            q_eval = p_info['q_eval']
            flags = p_info.get('flags', 0)
            pos_hash = p_info.get('position_hash', 0)

            # Check sampling delta policy (Δply < 2)
            if (ply - last_sampled_ply) < MIN_PLY_DELTA:
                self.sampling_delta_dropped += 1
                continue

            # Parse FEN
            try:
                piece_map, w_ksq, b_ksq, stm, castling, ep_str, _, _ = parse_fen_simple(fen)
            except Exception:
                self.exclusion_counts["Search Error"] += 1
                continue

            # Compute labels and verify exclusion taxonomy
            q_stm = compute_q_stm(q_eval, stm, is_white_relative=True)
            eligible, reason = is_eligible_position(q_stm, termination_reason)
            if not eligible:
                self.exclusion_counts[reason] = self.exclusion_counts.get(reason, 0) + 1
                continue

            # Check per-game position cap policy (Per-Game Cap > 30)
            if sampled_in_game >= MAX_POSITIONS_PER_GAME:
                self.sampling_cap_dropped += 1
                continue

            # Compute canonical key for cross-split collision check
            key = canonical_position_key(fen)

            # Cross-split leakage elimination (strict disjointness regardless of game ordering)
            if split == SPLIT_TRAIN:
                if key in self.val_keys or key in self.test_keys:
                    self.cross_split_dropped += 1
                    continue
            elif split == SPLIT_VAL:
                if key in self.train_keys or key in self.test_keys:
                    self.cross_split_dropped += 1
                    continue
            elif split == SPLIT_TEST:
                if key in self.train_keys or key in self.val_keys:
                    self.cross_split_dropped += 1
                    continue

            # Compute HalfKP features
            w_feats, b_feats = HalfKPEncoder.encode_from_board(piece_map, w_ksq, b_ksq)
            z_stm = compute_z_stm(result, stm)
            stm_code = 0 if stm.lower() == 'w' else 1

            if pos_hash == 0:
                pos_hash = int(hashlib.md5(key.encode('utf-8')).hexdigest()[:16], 16)

            # Detect en-passant in metadata if not already set
            if ep_str != '-' and not (flags & FLAG_HAS_EP):
                flags |= FLAG_HAS_EP

            record = DatasetRecord(
                position_hash=pos_hash,
                side_to_move=stm_code,
                metadata_flags=flags,
                z_stm=z_stm,
                q_stm=q_stm,
                white_features=w_feats,
                black_features=b_feats
            )

            # Add to respective split
            if split == SPLIT_TRAIN:
                self.train_records.append(record)
                self.train_keys.add(key)
            elif split == SPLIT_VAL:
                self.val_records.append(record)
                self.val_keys.add(key)
            else:
                self.test_records.append(record)
                self.test_keys.add(key)

            sampled_in_game += 1
            last_sampled_ply = ply
            self.total_positions_extracted += 1

        return sampled_in_game

    def get_additive_accounting(self) -> Dict:
        """Compute additive pipeline accounting tally."""
        mate = self.exclusion_counts.get("Forced Mate", 0)
        tb = self.exclusion_counts.get("Tablebase", 0)
        to = self.exclusion_counts.get("Timeout", 0)
        err = self.exclusion_counts.get("Search Error", 0)
        excl_total = mate + tb + to + err
        samp_total = self.sampling_delta_dropped + self.sampling_cap_dropped
        retained = len(self.train_records) + len(self.val_records) + len(self.test_records)
        residual = self.total_positions_examined - (excl_total + samp_total + self.cross_split_dropped + retained)
        return {
            "total_examined": self.total_positions_examined,
            "exclusion_taxonomy": {
                "total": excl_total,
                "forced_mate": mate,
                "tablebase": tb,
                "timeout": to,
                "search_error": err
            },
            "sampling_policy": {
                "total": samp_total,
                "delta_ply": self.sampling_delta_dropped,
                "per_game_cap": self.sampling_cap_dropped
            },
            "cross_split_collisions": self.cross_split_dropped,
            "retained_records": {
                "total": retained,
                "train": len(self.train_records),
                "val": len(self.val_records),
                "test": len(self.test_records)
            },
            "residual": residual,
            "exact_identity_verified": (residual == 0)
        }

    def print_additive_accounting(self) -> str:
        acc = self.get_additive_accounting()
        excl = acc["exclusion_taxonomy"]
        samp = acc["sampling_policy"]
        ret = acc["retained_records"]
        lines = [
            "=" * 80,
            "                     ADDITIVE PIPELINE ACCOUNTING REPORT",
            "=" * 80,
            f"  Total Examined Plies:                     {acc['total_examined']}",
            f"  - Exclusion Taxonomy:                     {excl['total']} (Mate: {excl['forced_mate']}, Tablebase: {excl['tablebase']}, Timeout: {excl['timeout']}, Error: {excl['search_error']})",
            f"  - Sampling Policy:                        {samp['total']} (Delta-ply < 2: {samp['delta_ply']}, Per-Game Cap > 30: {samp['per_game_cap']})",
            f"  - Cross-Split Duplicate FEN Collisions:   {acc['cross_split_collisions']}",
            f"  = Total Final Retained Records:           {ret['total']} (Train: {ret['train']}, Val: {ret['val']}, Test: {ret['test']})",
            "-" * 80,
            f"  Accounting Identity Invariant:            {'VERIFIED (Residual = 0)' if acc['exact_identity_verified'] else 'FAILED'}",
            "=" * 80
        ]
        text = "\n".join(lines)
        print(text)
        return text

    def assert_zero_cross_split_leakage(self) -> bool:
        """Verify strict disjointness of position sets across splits."""
        tv = self.train_keys & self.val_keys
        tt = self.train_keys & self.test_keys
        vt = self.val_keys & self.test_keys
        assert len(tv) == 0, f"Train-Val leakage detected: {len(tv)} positions!"
        assert len(tt) == 0, f"Train-Test leakage detected: {len(tt)} positions!"
        assert len(vt) == 0, f"Val-Test leakage detected: {len(vt)} positions!"
        return True

    def export_all(self, out_dir: str) -> Dict[str, str]:
        """Export train.bin, val.bin, test.bin."""
        self.assert_zero_cross_split_leakage()
        import os
        os.makedirs(out_dir, exist_ok=True)

        paths = {}
        for split_id, name, recs in [
            (SPLIT_TRAIN, "train.bin", self.train_records),
            (SPLIT_VAL, "val.bin", self.val_records),
            (SPLIT_TEST, "test.bin", self.test_records)
        ]:
            out_path = os.path.join(out_dir, name)
            write_dataset_file(out_path, recs, split_id, self.teacher_version, self.teacher_depth)
            paths[name] = out_path

        return paths
