#include "search/MovePicker.hpp"
#include "board/MoveGenerator.hpp"
#include "board/MoveExecutor.hpp"
#include "board/UndoState.hpp"
#include "board/Bitboard.hpp"
#include "search/MoveOrderer.hpp"
#include "search/see/SEE.hpp"
#include "search/Search.hpp"
#include "search/SearchController.hpp"
#include <algorithm>
#include <cmath>

namespace Boson {

MovePicker::MovePicker(const Position& position, Move tt, const SearchContext& context, PickerMode pickerMode) noexcept
    : pos(position),
      mode(pickerMode),
      currentStage(Stage::TTMove),
      ttMove(tt),
      killers{Move::none(), Move::none()},
      counterMove(Move::none()),
      captureIndex(0),
      quietIndex(0),
      badCaptureIndex(0),
      yieldedCount(0),
      killerIndex(0),
      m_context(context) {
    if (context.ply < 64 && context.killerMoves != nullptr) {
        killers[0] = (*context.killerMoves)[context.ply][0];
        killers[1] = (*context.killerMoves)[context.ply][1];
    }
    const auto& params = SearchController::getInstance().getParams();
    if (params.debug.enableCMH && context.prevMove.getRawData() != 0) {
        counterMove = Search::getCMH().getCounterMove(context.prevMove.getFromSquare(), context.prevMove.getToSquare());
        if (counterMove.getRawData() == 0) {
            Piece prevPiece = findPieceAtSquare(pos, context.prevMove.getToSquare());
            if (prevPiece != Piece::None) {
                counterMove = Search::getCMH().getCounterMove(prevPiece, context.prevMove.getToSquare());
            }
        }
    }
}

Piece MovePicker::findPieceAtSquare(const Position& pos, Square sq) noexcept {
    if (sq == Square::None) return Piece::None;
    const Bitboard squareMask = Bitboards::getSquareBit(sq);
    for (int p = 0; p < 12; ++p) {
        if (pos.getPieceBitboard(static_cast<Piece>(p)) & squareMask) {
            return static_cast<Piece>(p);
        }
    }
    return Piece::None;
}

int MovePicker::getPieceIndex(Piece p) noexcept {
    if (p == Piece::None) return 0;
    return static_cast<int>(p) % 6;
}

bool MovePicker::isCapture(const Position& pos, Move m) noexcept {
    if (m.getRawData() == 0) return false;
    const Bitboard targetBit = Bitboards::getSquareBit(m.getToSquare());
    return (pos.getTotalOccupancy() & targetBit) != 0 ||
           m.isEnPassant() ||
           (m.getToSquare() == pos.getEnPassantSquare() && pos.getEnPassantSquare() != Square::None);
}

bool MovePicker::hasYielded(Move m) const noexcept {
    for (uint8_t i = 0; i < yieldedCount; ++i) {
        if (yieldedMoves[i].getFromSquare() == m.getFromSquare() &&
            yieldedMoves[i].getToSquare() == m.getToSquare() &&
            yieldedMoves[i].getPromotionPiece() == m.getPromotionPiece()) {
            return true;
        }
    }
    return false;
}

void MovePicker::recordYielded(Move m) noexcept {
    if (yieldedCount < yieldedMoves.size()) {
        yieldedMoves[yieldedCount++] = m;
    }
}

bool MovePicker::isPseudoLegal(const Position& pos, Move m) noexcept {
    if (m.getRawData() == 0) return false;
    Square from = m.getFromSquare();
    Square to = m.getToSquare();
    if (from == to || static_cast<int>(from) < 0 || static_cast<int>(from) > 63 ||
        static_cast<int>(to) < 0 || static_cast<int>(to) > 63) return false;

    Color us = pos.getSideToMove();
    Bitboard friendlyOcc = pos.getColorOccupancy(us);
    Bitboard fromBit = Bitboards::getSquareBit(from);
    if (!(friendlyOcc & fromBit)) return false;

    Bitboard toBit = Bitboards::getSquareBit(to);
    if (friendlyOcc & toBit) return false;

    Bitboard totalOcc = pos.getTotalOccupancy();
    Bitboard enemyOcc = pos.getColorOccupancy(!us);

    Piece piece = Piece::None;
    size_t startP = (us == Color::White) ? 0 : 6;
    for (size_t p = startP; p < startP + 6; ++p) {
        if (pos.getPieceBitboard(static_cast<Piece>(p)) & fromBit) {
            piece = static_cast<Piece>(p);
            break;
        }
    }
    if (piece == Piece::None) return false;

    int pType = static_cast<int>(piece) % 6; // 0=P, 1=N, 2=B, 3=R, 4=Q, 5=K

    switch (pType) {
        case 1: // Knight
            return (MoveGenerator::getKnightAttacks(from) & toBit) != 0;
        case 2: // Bishop
            return (MoveGenerator::getBishopAttacks(from, totalOcc) & toBit) != 0;
        case 3: // Rook
            return (MoveGenerator::getRookAttacks(from, totalOcc) & toBit) != 0;
        case 4: // Queen
            return (MoveGenerator::getQueenAttacks(from, totalOcc) & toBit) != 0;
        case 5: { // King
            if (m.isCastling()) {
                CastlingRights rights = pos.getCastlingRights();
                if (us == Color::White) {
                    if (to == Square::G1 && from == Square::E1) {
                        return static_cast<bool>(rights & CastlingRights::WhiteOO) &&
                               !(totalOcc & (Bitboards::getSquareBit(Square::F1) | Bitboards::getSquareBit(Square::G1)));
                    }
                    if (to == Square::C1 && from == Square::E1) {
                        return static_cast<bool>(rights & CastlingRights::WhiteOOO) &&
                               !(totalOcc & (Bitboards::getSquareBit(Square::D1) | Bitboards::getSquareBit(Square::C1) | Bitboards::getSquareBit(Square::B1)));
                    }
                } else {
                    if (to == Square::G8 && from == Square::E8) {
                        return static_cast<bool>(rights & CastlingRights::BlackOO) &&
                               !(totalOcc & (Bitboards::getSquareBit(Square::F8) | Bitboards::getSquareBit(Square::G8)));
                    }
                    if (to == Square::C8 && from == Square::E8) {
                        return static_cast<bool>(rights & CastlingRights::BlackOOO) &&
                               !(totalOcc & (Bitboards::getSquareBit(Square::D8) | Bitboards::getSquareBit(Square::C8) | Bitboards::getSquareBit(Square::B8)));
                    }
                }
                return false;
            }
            return (MoveGenerator::getKingAttacks(from) & toBit) != 0;
        }
        case 0: { // Pawn
            int dir = (us == Color::White) ? 8 : -8;
            int fromInt = static_cast<int>(from);
            int toInt = static_cast<int>(to);
            bool isPromoRank = (us == Color::White) ? (toInt >= 56) : (toInt <= 7);

            if (m.isPromotion() != isPromoRank) return false;
            if (m.isPromotion() && m.getPromotionPiece() == Move::PromotionPiece::None) return false;

            if (toInt == fromInt + dir) {
                return !(totalOcc & toBit);
            }
            if (toInt == fromInt + 2 * dir) {
                Bitboard startRank = (us == Color::White) ? 0xFF00ULL : 0xFF000000000000ULL;
                if (!(fromBit & startRank)) return false;
                Square middleSq = static_cast<Square>(fromInt + dir);
                return !(totalOcc & toBit) && !(totalOcc & Bitboards::getSquareBit(middleSq));
            }
            // Pawn Capture
            int diff = toInt - fromInt;
            if (us == Color::White) {
                if (diff != 7 && diff != 9) return false;
                if (diff == 7 && (fromInt % 8 == 0)) return false;
                if (diff == 9 && (fromInt % 8 == 7)) return false;
            } else {
                if (diff != -7 && diff != -9) return false;
                if (diff == -9 && (fromInt % 8 == 0)) return false;
                if (diff == -7 && (fromInt % 8 == 7)) return false;
            }
            if (enemyOcc & toBit) return true;
            if (to == pos.getEnPassantSquare() && pos.getEnPassantSquare() != Square::None) return true;
            return false;
        }
        default:
            return false;
    }
}

bool MovePicker::isLegal(const Position& pos, Move m) noexcept {
    if (!isPseudoLegal(pos, m)) return false;
    Color us = pos.getSideToMove();
    Color them = (us == Color::White) ? Color::Black : Color::White;

    if (m.isCastling()) {
        if (MoveGenerator::inCheck(pos, us)) return false;
        Square to = m.getToSquare();
        if (to == Square::G1 && (MoveGenerator::isSquareAttacked(pos, Square::E1, them) || 
                                MoveGenerator::isSquareAttacked(pos, Square::F1, them) || 
                                MoveGenerator::isSquareAttacked(pos, Square::G1, them))) return false;
        if (to == Square::C1 && (MoveGenerator::isSquareAttacked(pos, Square::E1, them) || 
                                MoveGenerator::isSquareAttacked(pos, Square::D1, them) || 
                                MoveGenerator::isSquareAttacked(pos, Square::C1, them))) return false;
        if (to == Square::G8 && (MoveGenerator::isSquareAttacked(pos, Square::E8, them) || 
                                MoveGenerator::isSquareAttacked(pos, Square::F8, them) || 
                                MoveGenerator::isSquareAttacked(pos, Square::G8, them))) return false;
        if (to == Square::C8 && (MoveGenerator::isSquareAttacked(pos, Square::E8, them) || 
                                MoveGenerator::isSquareAttacked(pos, Square::D8, them) || 
                                MoveGenerator::isSquareAttacked(pos, Square::C8, them))) return false;
    }

    Position tempPos = pos;
    UndoState undo;
    MoveExecutor::makeMove(tempPos, m, undo);
    bool legal = !MoveGenerator::inCheck(tempPos, us);
    MoveExecutor::undoMove(tempPos, m, undo);
    return legal;
}

void MovePicker::scoreCaptures() noexcept {
    captureScores.resize(captures.size());
    for (size_t i = 0; i < captures.size(); ++i) {
        const Move& m = captures[i];
        Piece attacker = findPieceAtSquare(pos, m.getFromSquare());
        Piece victim = findPieceAtSquare(pos, m.getToSquare());

        if (m.isEnPassant() ||
            (m.getToSquare() == pos.getEnPassantSquare() && pos.getEnPassantSquare() != Square::None)) {
            victim = (pos.getSideToMove() == Color::White) ? Piece::BlackPawn : Piece::WhitePawn;
        }

        if (victim != Piece::None) {
            int aIdx = getPieceIndex(attacker);
            int vIdx = getPieceIndex(victim);
            int seeValue = SEE::evaluate(pos, m.getFromSquare(), m.getToSquare());
            if (seeValue >= 0) {
                captureScores[i] = MoveOrderer::SCORE_CAPTURES + MoveOrderer::MVV_LVA[vIdx][aIdx];
                if (m.isPromotion()) {
                    captureScores[i] += static_cast<int>(m.getPromotionPiece()) * 100;
                }
            } else {
                captureScores[i] = MoveOrderer::SCORE_LOSING_CAPTURES + seeValue;
            }
        } else if (m.isPromotion()) {
            captureScores[i] = MoveOrderer::SCORE_PROMOTIONS + static_cast<int>(m.getPromotionPiece()) * 100;
        } else {
            captureScores[i] = 0;
        }
    }
}

void MovePicker::scoreQuiets() noexcept {
    quietScores.resize(quiets.size());
    const auto& params = SearchController::getInstance().getParams();
    auto& stats = SearchController::getInstance().getStats();

    for (size_t i = 0; i < quiets.size(); ++i) {
        const Move& m = quiets[i];
        if (m.isPromotion()) {
            quietScores[i] = MoveOrderer::SCORE_PROMOTIONS + static_cast<int>(m.getPromotionPiece()) * 100;
            continue;
        }

        Piece attacker = findPieceAtSquare(pos, m.getFromSquare());
        int conthistScore = 0;
        if (params.debug.enableContHist && m_context.prevMove.getRawData() != 0 && attacker != Piece::None) {
            conthistScore = Search::getContHist().getScore(attacker, m_context.prevMove.getToSquare(), m.getToSquare());
            if (conthistScore > 0) {
                stats.conthistHits++;
            }
        }

        if (conthistScore > 0) {
            quietScores[i] = MoveOrderer::SCORE_CONTHIST_BASE + std::clamp(conthistScore / 8, 0, 3500);
        } else if (attacker != Piece::None && m_context.historyTable != nullptr) {
            size_t pIdx = static_cast<size_t>(attacker);
            size_t toIdx = static_cast<size_t>(m.getToSquare());
            int hist = static_cast<int>((*m_context.historyTable)[pIdx][toIdx]);
            quietScores[i] = MoveOrderer::SCORE_QUIET + std::clamp(hist + (conthistScore / 16), 0, 20000);
        } else {
            quietScores[i] = MoveOrderer::SCORE_QUIET;
        }
    }
}

Move MovePicker::nextMove() noexcept {
    while (true) {
        switch (currentStage) {
            case Stage::TTMove: {
                currentStage = Stage::GenCaptures;
                if (ttMove.getRawData() != 0) {
                    bool matchesMode = true;
                    if (mode == PickerMode::Quiescence) {
                        matchesMode = isCapture(pos, ttMove);
                    }
                    if (matchesMode && isLegal(pos, ttMove)) {
                        recordYielded(ttMove);
                        return ttMove;
                    }
                }
                break;
            }
            case Stage::GenCaptures: {
                MoveGenerator::generateLegalCaptures(pos, captures);
                scoreCaptures();
                captureIndex = 0;
                currentStage = Stage::GoodCaptures;
                break;
            }
            case Stage::GoodCaptures: {
                while (captureIndex < captures.size()) {
                    size_t bestIdx = captureIndex;
                    for (size_t i = captureIndex + 1; i < captures.size(); ++i) {
                        if (captureScores[i] > captureScores[bestIdx]) {
                            bestIdx = i;
                        }
                    }
                    std::swap(captures[captureIndex], captures[bestIdx]);
                    std::swap(captureScores[captureIndex], captureScores[bestIdx]);

                    if (captureScores[captureIndex] < 0) {
                        for (size_t i = captureIndex; i < captures.size(); ++i) {
                            badCaptures.push_back(captures[i]);
                        }
                        break;
                    }

                    Move m = captures[captureIndex++];
                    if (!hasYielded(m)) {
                        return m;
                    }
                }
                if (mode == PickerMode::Quiescence) {
                    currentStage = Stage::Done;
                } else {
                    currentStage = Stage::Killers;
                }
                break;
            }
            case Stage::Killers: {
                if (killerIndex == 0) {
                    killerIndex = 1;
                    if (killers[0].getRawData() != 0 && !isCapture(pos, killers[0]) &&
                        !hasYielded(killers[0]) && isLegal(pos, killers[0])) {
                        recordYielded(killers[0]);
                        return killers[0];
                    }
                }
                if (killerIndex == 1) {
                    killerIndex = 2;
                    if (killers[1].getRawData() != 0 && !isCapture(pos, killers[1]) &&
                        !hasYielded(killers[1]) && isLegal(pos, killers[1])) {
                        recordYielded(killers[1]);
                        return killers[1];
                    }
                }
                currentStage = Stage::CounterMove;
                break;
            }
            case Stage::CounterMove: {
                currentStage = Stage::GenQuiets;
                if (counterMove.getRawData() != 0 && !isCapture(pos, counterMove) &&
                    !hasYielded(counterMove) && isLegal(pos, counterMove)) {
                    SearchController::getInstance().getStats().cmhHits++;
                    recordYielded(counterMove);
                    return counterMove;
                }
                break;
            }
            case Stage::GenQuiets: {
                MoveGenerator::generateLegalQuiets(pos, quiets);
                scoreQuiets();
                quietIndex = 0;
                currentStage = Stage::Quiets;
                break;
            }
            case Stage::Quiets: {
                while (quietIndex < quiets.size()) {
                    size_t bestIdx = quietIndex;
                    for (size_t i = quietIndex + 1; i < quiets.size(); ++i) {
                        if (quietScores[i] > quietScores[bestIdx]) {
                            bestIdx = i;
                        }
                    }
                    std::swap(quiets[quietIndex], quiets[bestIdx]);
                    std::swap(quietScores[quietIndex], quietScores[bestIdx]);

                    Move m = quiets[quietIndex++];
                    if (!hasYielded(m)) {
                        return m;
                    }
                }
                currentStage = Stage::BadCaptures;
                break;
            }
            case Stage::BadCaptures: {
                while (badCaptureIndex < badCaptures.size()) {
                    Move m = badCaptures[badCaptureIndex++];
                    if (!hasYielded(m)) {
                        return m;
                    }
                }
                currentStage = Stage::Done;
                break;
            }
            case Stage::Done: {
                return Move::none();
            }
        }
    }
}

} // namespace Boson
