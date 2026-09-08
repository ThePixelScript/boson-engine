"""
Boson Dataset Schema (BOSN_DS1).
Binary serialization and deserialization for HalfKP sparse dataset records.
"""

import struct
from typing import Dict, List, BinaryIO, Iterator, Tuple

MAGIC = b"BOSN_DS1"
FORMAT_VERSION = 1
FEATURE_SPEC_VERSION = 1
HEADER_SIZE = 64
HEADER_FORMAT_64 = "<8sIIIIQB31s"
HEADER_FORMAT_68 = "<8sIIIIQB35s"

# Split Identifiers
SPLIT_TRAIN = 0
SPLIT_VAL = 1
SPLIT_TEST = 2

SPLIT_NAMES = {
    SPLIT_TRAIN: "train",
    SPLIT_VAL: "val",
    SPLIT_TEST: "test"
}

# Metadata Flag Bitmasks
FLAG_IN_CHECK = 1 << 0      # bit 0: Active side to move is in check
FLAG_HAS_CAPTURE = 1 << 1   # bit 1: Position has at least one legal capture
FLAG_HAS_PROMO = 1 << 2     # bit 2: Position has at least one legal promotion
FLAG_HAS_EP = 1 << 3        # bit 3: En-passant square active / EP capture exists


class DatasetHeader:
    def __init__(self,
                 magic: bytes = MAGIC,
                 format_version: int = FORMAT_VERSION,
                 feature_version: int = FEATURE_SPEC_VERSION,
                 teacher_version: int = 950,
                 teacher_depth: int = 6,
                 record_count: int = 0,
                 split_id: int = SPLIT_TRAIN):
        self.magic = magic
        self.format_version = format_version
        self.feature_version = feature_version
        self.teacher_version = teacher_version
        self.teacher_depth = teacher_depth
        self.record_count = record_count
        self.split_id = split_id

    def pack(self) -> bytes:
        reserved = b"\x00" * 31
        return struct.pack(HEADER_FORMAT_64,
                           self.magic,
                           self.format_version,
                           self.feature_version,
                           self.teacher_version,
                           self.teacher_depth,
                           self.record_count,
                           self.split_id,
                           reserved)

    @classmethod
    def unpack(cls, data: bytes) -> "DatasetHeader":
        if len(data) < 64:
            raise ValueError(f"Header data too short: {len(data)} bytes (expected >= 64)")
        if len(data) >= 68 and data[:8] == MAGIC:
            # Try 68 bytes first if available
            try:
                magic, fmt_v, feat_v, teach_v, teach_d, rec_cnt, split, _ = struct.unpack(HEADER_FORMAT_68, data[:68])
                if magic == MAGIC:
                    return cls(magic, fmt_v, feat_v, teach_v, teach_d, rec_cnt, split)
            except struct.error:
                pass
        magic, fmt_v, feat_v, teach_v, teach_d, rec_cnt, split, _ = struct.unpack(HEADER_FORMAT_64, data[:64])
        if magic != MAGIC:
            raise ValueError(f"Invalid magic: {magic} (expected {MAGIC})")
        return cls(magic, fmt_v, feat_v, teach_v, teach_d, rec_cnt, split)


class DatasetRecord:
    def __init__(self,
                 position_hash: int = 0,
                 side_to_move: int = 0,
                 metadata_flags: int = 0,
                 z_stm: float = 0.5,
                 q_stm: int = 0,
                 white_features: List[int] = None,
                 black_features: List[int] = None):
        self.position_hash = position_hash
        self.side_to_move = side_to_move
        self.metadata_flags = metadata_flags
        self.z_stm = float(z_stm)
        self.q_stm = int(q_stm)
        self.white_features = white_features if white_features is not None else []
        self.black_features = black_features if black_features is not None else []

    def pack(self) -> bytes:
        nw = len(self.white_features)
        nb = len(self.black_features)
        if nw > 32 or nb > 32:
            raise ValueError(f"Excessive active features: White={nw}, Black={nb} (max 32)")
        prefix = struct.pack("<QBBfh",
                             self.position_hash,
                             self.side_to_move,
                             self.metadata_flags,
                             self.z_stm,
                             self.q_stm)
        w_part = struct.pack(f"<B{nw}H", nw, *self.white_features)
        b_part = struct.pack(f"<B{nb}H", nb, *self.black_features)
        return prefix + w_part + b_part

    @classmethod
    def unpack_from_stream(cls, stream: BinaryIO) -> "DatasetRecord":
        prefix_bytes = stream.read(16)
        if not prefix_bytes or len(prefix_bytes) < 16:
            return None
        pos_hash, stm, flags, z, q = struct.unpack("<QBBfh", prefix_bytes)

        nw_byte = stream.read(1)
        if not nw_byte:
            raise ValueError("Truncated record reading white feature count")
        nw = nw_byte[0]
        w_feats = []
        if nw > 0:
            w_bytes = stream.read(nw * 2)
            if len(w_bytes) < nw * 2:
                raise ValueError("Truncated record reading white features")
            w_feats = list(struct.unpack(f"<{nw}H", w_bytes))

        nb_byte = stream.read(1)
        if not nb_byte:
            raise ValueError("Truncated record reading black feature count")
        nb = nb_byte[0]
        b_feats = []
        if nb > 0:
            b_bytes = stream.read(nb * 2)
            if len(b_bytes) < nb * 2:
                raise ValueError("Truncated record reading black features")
            b_feats = list(struct.unpack(f"<{nb}H", b_bytes))

        return cls(pos_hash, stm, flags, z, q, w_feats, b_feats)


def write_dataset_file(filepath: str,
                       records: List[DatasetRecord],
                       split_id: int,
                       teacher_version: int = 950,
                       teacher_depth: int = 6) -> int:
    header = DatasetHeader(
        record_count=len(records),
        split_id=split_id,
        teacher_version=teacher_version,
        teacher_depth=teacher_depth
    )
    with open(filepath, "wb") as f:
        f.write(header.pack())
        for rec in records:
            f.write(rec.pack())
    return len(records)


def read_dataset_file(filepath: str) -> Tuple[DatasetHeader, List[DatasetRecord]]:
    with open(filepath, "rb") as f:
        header_bytes = f.read(HEADER_SIZE)
        header = DatasetHeader.unpack(header_bytes)
        records = []
        for _ in range(header.record_count):
            rec = DatasetRecord.unpack_from_stream(f)
            if rec is None:
                break
            records.append(rec)
    return header, records
