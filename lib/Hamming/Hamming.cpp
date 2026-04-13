#include "Hamming.h"

// ─── Construction / teardown ──────────────────────────────────────────────────

HammingCodec::HammingCodec(int n, int k)
    : _n(n), _k(k), _m(n - k) {}

HammingCodec::~HammingCodec() {
    _freeMatrices();
}

void HammingCodec::_freeMatrices() {
    if (_G) {
        for (int i = 0; i < _k; i++) delete[] _G[i];
        delete[] _G;
        _G = nullptr;
    }
    if (_H) {
        for (int i = 0; i < _m; i++) delete[] _H[i];
        delete[] _H;
        _H = nullptr;
    }
}

// ─── Initialisation ───────────────────────────────────────────────────────────

bool HammingCodec::begin() {
    // Validate: N must equal 2^M − 1
    if (_m <= 0 || _k <= 0 || _n != ((1 << _m) - 1)) {
        return false;
    }
    _buildMatrices();
    return true;
}

// ─── Matrix construction ──────────────────────────────────────────────────────
//
// H is an M×N parity-check matrix whose N columns are all 2^M−1 non-zero
// M-bit vectors, split into two groups:
//
//   Columns 0..K-1   data positions    — weight-≥2 patterns (non-powers-of-two)
//   Columns K..N-1   parity positions  — identity I_m (weight-1 patterns)
//
// G = [I_k | P]  where  P = H[:,0:K]^T
// (systematic form: the first K bits of every codeword are the original data)
//
void HammingCodec::_buildMatrices() {
    _freeMatrices();

    // Allocate
    _G = new int*[_k];
    for (int i = 0; i < _k; i++) { _G[i] = new int[_n]; }

    _H = new int*[_m];
    for (int i = 0; i < _m; i++) { _H[i] = new int[_n]; }

    // Collect the K data-column bit-patterns (weight >= 2 → not a power of two)
    int* dataCol = new int[_k];
    int count = 0;
    for (int v = 1; v < (1 << _m) && count < _k; v++)
        if (v & (v - 1))          // weight >= 2
            dataCol[count++] = v;

    // Build H
    for (int i = 0; i < _m; i++) {
        for (int j = 0; j < _k; j++)
            _H[i][j] = (dataCol[j] >> (_m - 1 - i)) & 1;  // MSB → row 0
        for (int j = 0; j < _m; j++)
            _H[i][_k + j] = (i == j) ? 1 : 0;             // I_m block
    }
    delete[] dataCol;

    // Build G = [I_k | P],  P = H[:,0:K]^T
    for (int i = 0; i < _k; i++) {
        for (int j = 0; j < _k; j++)
            _G[i][j] = (i == j) ? 1 : 0;   // I_k block
        for (int j = 0; j < _m; j++)
            _G[i][_k + j] = _H[j][i];      // parity block
    }
}

// ─── Buffer-size helpers ──────────────────────────────────────────────────────

int HammingCodec::encodedBits(int inputLen) const {
    const int numWords = (inputLen * 8 + _k - 1) / _k;
    return numWords * _n;
}

int HammingCodec::encodedBytes(int inputLen) const {
    return (encodedBits(inputLen) + 7) / 8;
}

// ─── Bit-level helpers ────────────────────────────────────────────────────────
// Bits are addressed MSB-first within each byte:
//   position 0  = MSB of byte 0
//   position 7  = LSB of byte 0
//   position 8  = MSB of byte 1 … etc.

int HammingCodec::_readBit(const uint8_t* buf, int pos) {
    return (buf[pos >> 3] >> (7 - (pos & 7))) & 1;
}

void HammingCodec::_writeBit(uint8_t* buf, int pos, int bit) {
    uint8_t mask = 1u << (7 - (pos & 7));
    if (bit) buf[pos >> 3] |=  mask;
    else     buf[pos >> 3] &= ~mask;
}

// ─── Word-level encode ────────────────────────────────────────────────────────
// c = d · G  over GF(2)

void HammingCodec::_encodeWord(const int* d, int* c) const {
    for (int j = 0; j < _n; j++) {
        int s = 0;
        for (int i = 0; i < _k; i++) s ^= (d[i] & _G[i][j]);
        c[j] = s;
    }
}

// ─── Word-level decode + single-error correction ──────────────────────────────
// Syndrome s = H · r^T (mod 2).
// Non-zero syndrome → column of H that matches → flip that bit.
// Returns corrected bit index (0..N-1), or -1 if no error.

int HammingCodec::_decodeWord(int* r, int* d) const {
    // Compute syndrome
    int* syn = new int[_m]();
    for (int i = 0; i < _m; i++)
        for (int j = 0; j < _n; j++)
            syn[i] ^= (r[j] & _H[i][j]);

    bool hasError = false;
    for (int i = 0; i < _m; i++) if (syn[i]) { hasError = true; break; }

    int errPos = -1;
    if (hasError) {
        for (int j = 0; j < _n && errPos < 0; j++) {
            bool match = true;
            for (int i = 0; i < _m; i++)
                if (_H[i][j] != syn[i]) { match = false; break; }
            if (match) errPos = j;
        }
        if (errPos >= 0) r[errPos] ^= 1;
    }
    delete[] syn;

    // Systematic: first K bits of codeword are the data bits
    for (int i = 0; i < _k; i++) d[i] = r[i];
    return errPos;
}

// ─── Buffer encode ────────────────────────────────────────────────────────────

int HammingCodec::encode(const uint8_t* input,  int inputLen,
                               uint8_t* output, int outputLen) const {
    memset(output, 0, outputLen);

    const int totalDataBits = inputLen * 8;
    const int numWords      = (totalDataBits + _k - 1) / _k;

    int* d = new int[_k];
    int* c = new int[_n];
    int inPos = 0, outPos = 0;

    for (int w = 0; w < numWords; w++) {
        for (int i = 0; i < _k; i++)
            d[i] = (inPos < totalDataBits) ? _readBit(input, inPos++) : 0;

        _encodeWord(d, c);

        for (int j = 0; j < _n; j++)
            _writeBit(output, outPos++, c[j]);
    }

    delete[] d;
    delete[] c;
    return outPos;   // total encoded bits written
}

// ─── Buffer decode ────────────────────────────────────────────────────────────

int HammingCodec::decode(const uint8_t* input,  int inputBits,
                               uint8_t* output, int outputLen,
                               int*     outDataBits) const {
    memset(output, 0, outputLen);

    const int numWords = inputBits / _n;
    int* r = new int[_n];
    int* d = new int[_k];
    int inPos = 0, outPos = 0, errs = 0;

    for (int w = 0; w < numWords; w++) {
        for (int j = 0; j < _n; j++)
            r[j] = _readBit(input, inPos++);

        if (_decodeWord(r, d) >= 0) errs++;

        for (int i = 0; i < _k && (outPos >> 3) < outputLen; i++)
            _writeBit(output, outPos++, d[i]);
    }

    delete[] r;
    delete[] d;
    if (outDataBits) *outDataBits = outPos;
    return errs;
}

// ─── Diagnostics ──────────────────────────────────────────────────────────────

void HammingCodec::printMatrices(Stream& stream) const {
    stream.printf("G (%dx%d):\n", _k, _n);
    for (int i = 0; i < _k; i++) {
        stream.print("  [ ");
        for (int j = 0; j < _n; j++) stream.printf("%d ", _G[i][j]);
        stream.println("]");
    }
    stream.println();
    stream.printf("H (%dx%d):\n", _m, _n);
    for (int i = 0; i < _m; i++) {
        stream.print("  [ ");
        for (int j = 0; j < _n; j++) stream.printf("%d ", _H[i][j]);
        stream.println("]");
    }
}

void HammingCodec::printInfo(int inputLen, Stream& stream) const {
    const int numCW  = (inputLen * 8 + _k - 1) / _k;
    const int encB   = encodedBytes(inputLen);
    stream.printf("Hamming(%d,%d)  M=%d\n", _n, _k, _m);
    stream.printf("  Input    : %d bytes = %d bits\n", inputLen, inputLen * 8);
    stream.printf("  Codewords: %d x %d bits = %d encoded bits\n",
                  numCW, _n, numCW * _n);
    stream.printf("  Encoded  : %d bytes (bit-tight, no padding)\n", encB);
    stream.printf("  Code rate: %d/%d = %.1f%%\n", _k, _n, 100.0f * _k / _n);
    stream.printf("  Overhead : +%.1f%%\n",
                  100.0f * ((float)encB / inputLen - 1.0f));
}
