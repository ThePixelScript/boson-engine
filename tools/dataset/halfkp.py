"""
Bit-Exact HalfKP Sparse Feature Encoder.
Matches Boson's C++ FeatureTransformer.cpp and NNUETypes.hpp bit-for-bit.
"""

from typing import List, Tuple, Dict, Optional
import json
import sys

HALFKP_FEATURES = 40960

PIECE_TO_CODE = {
    'P': 0, 'N': 1, 'B': 2, 'R': 3, 'Q': 4,
    'p': 5, 'n': 6, 'b': 7, 'r': 8, 'q': 9
}

COLOR_WHITE = 0
COLOR_BLACK = 1


def square_from_coords(file: int, rank: int) -> int:
    """Return little-endian rank-file index 0..63."""
    return rank * 8 + file


def parse_fen_simple(fen: str) -> Tuple[Dict[int, str], int, int, str, str, int, int]:
    """
    Parse standard FEN string into:
    (piece_map, white_king_sq, black_king_sq, side_to_move, castling, ep_square, halfmove, fullmove)
    """
    parts = fen.strip().split()
    board_part = parts[0]
    side = parts[1] if len(parts) > 1 else 'w'
    castling = parts[2] if len(parts) > 2 else '-'
    ep_str = parts[3] if len(parts) > 3 else '-'
    halfmove = int(parts[4]) if len(parts) > 4 else 0
    fullmove = int(parts[5]) if len(parts) > 5 else 1

    piece_map = {}
    white_king_sq = -1
    black_king_sq = -1

    ranks = board_part.split('/')
    if len(ranks) != 8:
        raise ValueError(f"Invalid FEN board part: {board_part}")

    for r_idx, rank_str in enumerate(ranks):
        rank = 7 - r_idx
        file = 0
        for ch in rank_str:
            if ch.isdigit():
                file += int(ch)
            else:
                sq = rank * 8 + file
                if ch == 'K':
                    white_king_sq = sq
                elif ch == 'k':
                    black_king_sq = sq
                else:
                    piece_map[sq] = ch
                file += 1

    if white_king_sq == -1 or black_king_sq == -1:
        raise ValueError(f"FEN missing kings: {fen}")

    return piece_map, white_king_sq, black_king_sq, side, castling, ep_str, halfmove, fullmove


class HalfKPEncoder:
    """Canonical HalfKP feature encoder matching C++ FeatureTransformer."""

    @staticmethod
    def encode_from_board(piece_map: Dict[int, str],
                          white_king_sq: int,
                          black_king_sq: int) -> Tuple[List[int], List[int]]:
        """
        Encode active features for both perspectives.
        Returns: (sorted white_features, sorted black_features)
        """
        white_features = []
        black_features = []

        for sq, piece_char in piece_map.items():
            if piece_char not in PIECE_TO_CODE:
                continue
            base_code = PIECE_TO_CODE[piece_char]

            # 1. White Perspective
            w_idx = (white_king_sq * 640) + (base_code * 64) + sq
            white_features.append(w_idx)

            # 2. Black Perspective (rank-flipped coordinates and inverted piece colors)
            b_ksq = black_king_sq ^ 56
            b_psq = sq ^ 56
            b_code = (base_code + 5) if base_code < 5 else (base_code - 5)
            b_idx = (b_ksq * 640) + (b_code * 64) + b_psq
            black_features.append(b_idx)

        white_features.sort()
        black_features.sort()
        return white_features, black_features

    @classmethod
    def encode_fen(cls, fen: str) -> Tuple[List[int], List[int]]:
        piece_map, w_ksq, b_ksq, _, _, _, _, _ = parse_fen_simple(fen)
        return cls.encode_from_board(piece_map, w_ksq, b_ksq)


CANONICAL_TEST_POSITIONS = {
    "startpos": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "kiwipete": "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "tactical_wac001": "2rr3k/pp3pp1/1nnqbN1p/3pN3/2pP4/2P3Q1/PPB4P/R4RK1 w - - 0 1",
    "positional_closed": "r1bqk2r/pp2bppp/2n1pn2/2pp4/2PP4/2N1PN2/PP2BPPP/R1BQK2R w KQkq - 4 7",
    "endgame_kpk": "8/8/8/4k3/8/4P3/8/4K3 w - - 0 1",
    "search_stress_evasions": "r1b1k2r/ppppnppp/2n5/4P3/1b1P4/2N2N2/PPP2PPP/R1BQKB1R w KQkq - 1 6"
}


def main():
    if "--test" in sys.argv:
        results = {}
        for name, fen in CANONICAL_TEST_POSITIONS.items():
            w_feats, b_feats = HalfKPEncoder.encode_fen(fen)
            results[name] = {
                "fen": fen,
                "white_count": len(w_feats),
                "white_features": w_feats,
                "black_count": len(b_feats),
                "black_features": b_feats
            }
        print(json.dumps(results, indent=2))
    elif "--fen" in sys.argv:
        idx = sys.argv.index("--fen")
        if idx + 1 < len(sys.argv):
            fen = sys.argv[idx + 1]
            w_feats, b_feats = HalfKPEncoder.encode_fen(fen)
            print(json.dumps({"white": w_feats, "black": b_feats}))
    else:
        print("Usage: python halfkp.py [--test | --fen <FEN>]")


if __name__ == "__main__":
    main()
