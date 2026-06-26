// Surgical mutators for direct high-velocity bit updates
    void clearPieceBit(Square sq, Piece piece) noexcept { m_pieces[static_cast<size_t>(piece)] &= ~Bitboards::getSquareBit(sq); }
    void setPieceBit(Square sq, Piece piece) noexcept { m_pieces[static_cast<size_t>(piece)] |= Bitboards::getSquareBit(sq); }