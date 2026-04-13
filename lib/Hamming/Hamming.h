#pragma once
#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
// HammingCodec
//
// Encodes and decodes byte buffers using a systematic Hamming(N,K) code.
// N and K are supplied at construction time; the generator matrix G and
// parity-check matrix H are computed automatically.
//
// Valid configurations (N = 2^M − 1, M = N − K):
//   HammingCodec codec(7,  4);   // M=3  — default
//   HammingCodec codec(15, 11);  // M=4
//   HammingCodec codec(31, 26);  // M=5
//
// Typical usage:
//   HammingCodec codec(7, 4);
//   codec.begin();                        // build matrices — call once in setup()
//
//   int encBits = codec.encode(src, srcLen, dst, codec.encodedBytes(srcLen));
//   int errors  = codec.decode(dst, encBits, out, srcLen);
//
// Bit-tight packing:
//   Codewords are written as N consecutive bits into a flat bitstream with no
//   padding.  Byte boundaries land wherever they fall — a byte routinely holds
//   the tail of one codeword and the head of the next.  The decoder uses the
//   exact bit count returned by encode() to stay aligned.
// ─────────────────────────────────────────────────────────────────────────────

class HammingCodec {
public:
    // ── Construction / teardown ───────────────────────────────────────────────
    HammingCodec(int n, int k);
    ~HammingCodec();

    // Disallow copy — owns heap memory for the matrices
    HammingCodec(const HammingCodec&)            = delete;
    HammingCodec& operator=(const HammingCodec&) = delete;

    // ── Initialisation ────────────────────────────────────────────────────────
    // Build G and H from N and K.  Must be called before encode/decode.
    // Returns false if the configuration is not a valid Hamming code
    // (N must equal 2^M − 1 where M = N − K).
    bool begin();

    // ── Encoding ──────────────────────────────────────────────────────────────
    // Encode `inputLen` bytes from `input` into `output`.
    // `output` must be at least encodedBytes(inputLen) bytes long.
    // Returns the number of encoded bits written (pass this to decode).
    int encode(const uint8_t* input,  int inputLen,
                     uint8_t* output, int outputLen) const;

    // ── Decoding ──────────────────────────────────────────────────────────────
    // Decode `inputBits` bits (the value returned by encode) from `input`
    // into `output`.  Corrects single-bit errors per codeword automatically.
    // `output` must be at least the original inputLen bytes long.
    // Returns the number of bit errors corrected.
    // Optional `outDataBits` receives the number of data bits written.
    int decode(const uint8_t* input,  int inputBits,
                     uint8_t* output, int outputLen,
                     int*     outDataBits = nullptr) const;

    // ── Buffer-size helpers ───────────────────────────────────────────────────
    // Number of encoded bytes needed for a plaintext buffer of `inputLen` bytes.
    int encodedBytes(int inputLen) const;

    // Number of encoded bits (useful for logging / framing headers).
    int encodedBits(int inputLen) const;

    // ── Diagnostics ───────────────────────────────────────────────────────────
    // Print G and H to any Stream (defaults to Serial).
    void printMatrices(Stream& stream = Serial) const;

    // Print a one-line codec summary (rate, buffer sizes, etc.).
    void printInfo(int inputLen, Stream& stream = Serial) const;

    // Accessors for the raw matrices (read-only)
    int n() const { return _n; }
    int k() const { return _k; }
    int m() const { return _m; }

private:
    // ── Configuration ─────────────────────────────────────────────────────────
    const int _n;   // codeword length
    const int _k;   // data word length
    const int _m;   // parity bits  (= n − k)

    // ── Matrix storage (heap-allocated in begin()) ────────────────────────────
    int** _G = nullptr;   // [_k][_n]
    int** _H = nullptr;   // [_m][_n]

    void _freeMatrices();
    void _buildMatrices();

    // ── Word-level codec (operate on int arrays of length _n / _k) ───────────
    void _encodeWord(const int* d, int* c) const;
    int  _decodeWord(int* r, int* d)       const;   // returns errPos or -1

    // ── Bit-level I/O into byte buffers ───────────────────────────────────────
    static int  _readBit (const uint8_t* buf, int pos);
    static void _writeBit(      uint8_t* buf, int pos, int bit);
};
