#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>

#include "../src/qrcode.h"
#include "QrCode.hpp"

static uint32_t check(const qrcodegen::QrCode &nayuki, QRCode *ricmoo) {
    uint32_t wrong = 0;

    if (nayuki.size != ricmoo->size) { wrong += (1 << 20); }

    int border = 4;
    for (int y = -border; y < nayuki.size + border; y++) {
        for (int x = -border; x < nayuki.size + border; x++) {
            if (!!nayuki.getModule(x, y) != qrcode_getModule(ricmoo, x, y)) {
                wrong++;
            }
        }
    }

    return wrong;
}

static const qrcodegen::QrCode::Ecc *eccLevel(int ecc) {
    switch (ecc) {
        case 0: return &qrcodegen::QrCode::Ecc::LOW;
        case 1: return &qrcodegen::QrCode::Ecc::MEDIUM;
        case 2: return &qrcodegen::QrCode::Ecc::QUARTILE;
        default: return &qrcodegen::QrCode::Ecc::HIGH;
    }
}

int main() {
    std::clock_t t0, totalNayuki = 0, totalRicMoo = 0;

    int total = 0, passed = 0;
    for (char version = 1; version <= 40; version++) {
        if (LOCK_VERSION != 0 && LOCK_VERSION != version) { continue; }

        for (char ecc = 0; ecc < 4; ecc++) {
            const qrcodegen::QrCode::Ecc *errCorLvl = eccLevel(ecc);

            for (char tc = 0; tc < 6; tc++) {
                char *data;
                switch(tc) {
                    case 0:
                        data = (char*)"HELLO";
                        break;
                    case 1:
                        data = (char*)"Hello";
                        break;
                    case 2:
                        data = (char*)"1234";
                        break;
                    case 3:
                        data = (char*)"12345";
                        break;
                    case 4:
                        data = (char*)"\xc3\xa9t\xc3\xa9";
                        break;
                    case 5:
                        data = (char*)"\xff\x80";
                        break;
                }
                t0 = std::clock();
                const qrcodegen::QrCode nayuki = qrcodegen::QrCode::encodeText(data, version, *errCorLvl);
                totalNayuki += std::clock() - t0;

                t0 = std::clock();
                QRCode ricmoo;
                uint8_t ricmooBytes[qrcode_getBufferSize(version)];
                qrcode_initText(&ricmoo, ricmooBytes, version, ecc, data);
                totalRicMoo += std::clock() - t0;

                uint32_t badModules = check(nayuki, &ricmoo);
                if (badModules) {
                    printf("Failed test case: version=%d, ecc=%d, data=\"%s\", faliured=%d\n", version, ecc, data, badModules);
                } else {
                    passed++;
                }

                total++;
            }
        }
    }

    // Capacity: the longest string Nayuki accepts must match exactly, one more
    // character must be rejected (and must not touch the output buffer)
    for (int version = 1; version <= 40; version++) {
        if (LOCK_VERSION != 0 && LOCK_VERSION != version) { continue; }

        for (int ecc = 0; ecc < 4; ecc++) {
            for (char fill : {'1', 'A', 'a'}) {
                // Binary search for the longest string that fits, using Nayuki's bit counting
                int capacityBits = qrcodegen::QrCode::getNumDataCodewords(version, *eccLevel(ecc)) * 8;
                size_t lo = 0, hi = 8000;
                while (lo + 1 < hi) {
                    size_t mid = (lo + hi) / 2;
                    std::string text(mid, fill);
                    int bits = qrcodegen::QrSegment::getTotalBits(qrcodegen::QrSegment::makeSegments(text.c_str()), version);
                    if (bits != -1 && bits <= capacityBits) {
                        lo = mid;
                    } else {
                        hi = mid;
                    }
                }
                std::string data(lo, fill);

                QRCode ricmoo;
                uint8_t ricmooBytes[qrcode_getBufferSize(version)];
                int8_t result = qrcode_initText(&ricmoo, ricmooBytes, version, ecc, data.c_str());
                const qrcodegen::QrCode nayuki = qrcodegen::QrCode::encodeText(data.c_str(), version, *eccLevel(ecc));
                if (result != 0 || check(nayuki, &ricmoo)) {
                    printf("Failed capacity test case: version=%d, ecc=%d, fill=%c, length=%zu\n", version, ecc, fill, data.size());
                } else {
                    passed++;
                }
                total++;

                data += fill;
                memset(ricmooBytes, 0x5A, sizeof(ricmooBytes));
                result = qrcode_initText(&ricmoo, ricmooBytes, version, ecc, data.c_str());
                bool untouched = true;
                for (size_t i = 0; i < sizeof(ricmooBytes); i++) {
                    if (ricmooBytes[i] != 0x5A) { untouched = false; }
                }
                if (result != -1 || !untouched) {
                    printf("Failed overflow test case: version=%d, ecc=%d, fill=%c, length=%zu\n", version, ecc, fill, data.size());
                } else {
                    passed++;
                }
                total++;
            }
        }
    }

    // Invalid arguments must be rejected
    {
        QRCode ricmoo;
        uint8_t ricmooBytes[qrcode_getBufferSize(40)];
        const int8_t invalid[][2] = {
            { 0, 0 }, { 41, 0 }, { (int8_t)(LOCK_VERSION == 0 ? 1 : LOCK_VERSION), 4 },
            { (int8_t)(LOCK_VERSION == 0 ? 0 : (LOCK_VERSION % 40) + 1), 0 },
        };
        for (const auto &args : invalid) {
            if (qrcode_initText(&ricmoo, ricmooBytes, args[0], args[1], "HELLO") != -1) {
                printf("Failed invalid argument test case: version=%d, ecc=%d\n", args[0], args[1]);
            } else {
                passed++;
            }
            total++;
        }
    }

    printf("Tests complete: %d passed (out of %d)\n", passed, total);
    printf("Timing: Nayuki=%lu, RicMoo=%lu\n", (unsigned long)totalNayuki, (unsigned long)totalRicMoo);

    return (passed == total) ? 0 : 1;
}
