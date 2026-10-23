#include "Compression.h"

#define ZLIB_CONST
#include "zlib.h"

using namespace std;

namespace vfs {
    uint32_t Crc32(const string& bytes)
    {
        return static_cast<uint32_t>(crc32(0L, reinterpret_cast<const Bytef*>(bytes.data()), static_cast<uInt>(bytes.size())));
    }

    InflateResult Inflate(const string& deflated, size_t size)
    {
        InflateResult result = { false, string(size, '\0') };

        // negative window bits: raw deflate, with no header or trailer
        z_stream stream = {};
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
            result.bytes.clear();
            return result;
        }

        stream.next_in = reinterpret_cast<const Bytef*>(deflated.data());
        stream.avail_in = static_cast<uInt>(deflated.size());
        stream.next_out = reinterpret_cast<Bytef*>(result.bytes.data());
        stream.avail_out = static_cast<uInt>(size);

        int status = inflate(&stream, Z_FINISH);
        inflateEnd(&stream);

        result.valid = status == Z_STREAM_END && stream.avail_out == 0;
        if (!result.valid)
            result.bytes.clear();
        return result;
    }
}
