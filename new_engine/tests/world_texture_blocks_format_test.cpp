// CPU-only container tests. Synthetic blocks exercise the container contract;
// actual BC7 decoding/quality is tested separately by the encoder/GL fixtures.
#include "engine/world_texture_blocks_format.h"
#include <atomic>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace bc = world_texture_blocks;
namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;
static unsigned checks = 0;
static void require(bool value, const std::string &message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
static bc::Image image(std::uint32_t width = 9, std::uint32_t height = 7, unsigned seed = 3) {
    bc::Image result; result.width = width; result.height = height;
    for (;;) {
        bc::Level level; level.width = width; level.height = height;
        level.blocks.resize(std::size_t((width + 3) / 4) * ((height + 3) / 4) * 16);
        for (std::size_t i = 0; i < level.blocks.size(); ++i)
            level.blocks[i] = std::uint8_t(seed * 19 + i * 7 + result.levels.size());
        for (std::size_t i = 0; i < level.blocks.size(); i += 16) {
            if ((i / 16) & 1) level.blocks[i] = 0x02; // RGB-only mode1.
            else {
                level.blocks[i] = 0x40; // Mode6 with all opaque alpha endpoint/P bits.
                level.blocks[i + 6] |= 0xfe; level.blocks[i + 7] = 0xff; level.blocks[i + 8] |= 1;
            }
        }
        level.rmse = 1.25f; level.bias[0] = 0.125f; level.bias[1] = -0.25f; level.bias[2] = 0.5f;
        result.levels.push_back(std::move(level));
        if (width == 1 && height == 1) break;
        width = std::max(1u, width / 2); height = std::max(1u, height / 2);
    }
    return result;
}
static bool same(const bc::Image &a, const bc::Image &b) {
    if (a.width != b.width || a.height != b.height || a.levels.size() != b.levels.size() || a.format!=b.format || a.normal_max_degrees!=b.normal_max_degrees || a.normal_rms_degrees!=b.normal_rms_degrees) return false;
    for (std::size_t i = 0; i < a.levels.size(); ++i) {
        const auto &x = a.levels[i], &y = b.levels[i];
        if (x.width != y.width || x.height != y.height || x.blocks != y.blocks ||
            std::memcmp(&x.rmse, &y.rmse, sizeof(float)) || std::memcmp(x.bias, y.bias, sizeof(x.bias)) ||
            x.alpha_max != y.alpha_max || x.max_error!=y.max_error) return false;
    }
    return true;
}
// Deliberately independent bit-at-a-time CRC, avoiding a writer/parser test
// that could agree on the same incorrect implementation or field coverage.
static std::uint64_t reference_crc(const Bytes &bytes) {
    std::uint64_t crc = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        crc ^= std::uint64_t(i >= 56 && i < 64 ? 0 : bytes[i]) << 56;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc << 1) ^ ((crc >> 63) ? 0x42f0e1eba9ea3693ull : 0);
    }
    return crc;
}
static void put32(Bytes &bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes.at(offset + i) = std::uint8_t(value >> (i * 8));
}
static void put64(Bytes &bytes, std::size_t offset, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) bytes.at(offset + i) = std::uint8_t(value >> (i * 8));
}
static void put_float(Bytes &bytes, std::size_t offset, float value) {
    std::uint32_t bits; std::memcpy(&bits, &value, 4); put32(bytes, offset, bits);
}
static void repair_crc(Bytes &bytes) { put64(bytes, 56, reference_crc(bytes)); }
static Bytes serialize(const Bytes &source, const bc::Image &value) {
    Bytes bytes; std::string error;
    require(bc::serialize(source.data(), source.size(), value, bytes, error), "serialize: " + error);
    return bytes;
}
static void reject(const Bytes &bytes, const Bytes &source, const std::string &label) {
    const auto sentinel = image(1, 1, 91); auto output = sentinel; std::string error;
    require(!bc::parse(bytes.data(), bytes.size(), source.data(), source.size(), output, error), label + " accepted");
    require(!error.empty(), label + " has no diagnostic");
    require(same(output, sentinel), label + " changed the previous output");
}
static Bytes file_bytes(const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}
static void require_no_temps(const fs::path &directory) {
    for (const auto &entry : fs::directory_iterator(directory))
        require(entry.path().filename().string().find(".tmp.") == std::string::npos, "temporary file leaked");
}

int main() {
    fs::path directory;
    try {
        const Bytes source = {0, 0xff, 'P', 'N', 'G', 0, 0x80};
        const auto original = image();
        require(bc::fingerprint("hello", 5) == 0xa430d84680aabd0bull, "FNV-1a known vector");
        require(bc::checksum("123456789", 9) == 0x6c40df5f0b497347ull, "CRC64-ECMA known vector");
        require(bc::fingerprint(nullptr, 0) == 14695981039346656037ull, "empty fingerprint");
        require(bc::fingerprint(nullptr, 1) == 0, "invalid fingerprint pointer");
        const auto good = serialize(source, original);
        require(good == serialize(source, original), "serialization must be reproducible");
        require(good.size() == 64 + 4 * 40 + source.size() + 144, "independent layout byte count");
        require(good[8] == 1 && good[9] == 0 && good[12] == 1 && good[16] == 1 && good[20] == 64,
                "header is explicitly little-endian");
        require(std::equal(source.begin(), source.end(), good.begin() + 224), "full encoded source retained");
        require(bc::file_checksum(good.data(), good.size()) == reference_crc(good), "independent checksum coverage");
        bc::Image decoded; std::string error;
        require(bc::parse(good.data(), good.size(), source.data(), source.size(), decoded, error), "positive parse: " + error);
        require(error.empty() && same(decoded, original), "roundtrip all mip bytes and metric float bits");
        {
            auto linear=original;linear.format=bc::format_linear_bc7;
            for(auto &level:linear.levels)level.max_error=bc::linear_error_max;
            const auto bytes=serialize(source,linear);bc::Image roundtrip;
            require(!bc::parse(bytes.data(),bytes.size(),source.data(),source.size(),roundtrip,error),"Linear cache must not alias default sRGB");
            require(bc::parse(bytes.data(),bytes.size(),source.data(),source.size(),roundtrip,error,bc::format_linear_bc7)&&same(linear,roundtrip),"Strict linear roundtrip");
            require(!bc::parse(good.data(),good.size(),source.data(),source.size(),roundtrip,error,bc::format_linear_bc7)&&same(linear,roundtrip),"sRGB cache must not alias linear; output atomic");
            for(std::size_t mip=0;mip<linear.levels.size();mip++)for(unsigned kind=0;kind<3;kind++){
                auto bad=bytes;const auto offset=64+mip*40;
                if(kind==0)put32(bad,offset+36,bc::linear_error_max+1);
                if(kind==1)put_float(bad,offset+16,std::nextafter(bc::linear_rmse_max,2.f));
                if(kind==2)put_float(bad,offset+20,std::nextafter(1.5f,2.f));
                repair_crc(bad);
                require(!bc::parse(bad.data(),bad.size(),source.data(),source.size(),roundtrip,error,bc::format_linear_bc7)&&same(linear,roundtrip),"Every linear mip enforces max/RMS/bias certificates atomically");
            }
            auto bad=linear;bad.levels.back().max_error++;
            Bytes unchanged={1,2,3};require(!bc::serialize(source.data(),source.size(),bad,unchanged,error)&&unchanged==Bytes({1,2,3}),"Writer rejects weak data certificate atomically");
        }
        {
            auto normal=original;normal.format=bc::format_normal_bc7;
            normal.normal_max_degrees=2.f;normal.normal_rms_degrees=.75f;
            for(auto &level:normal.levels)level.max_error=bc::linear_error_max;
            const auto bytes=serialize(source,normal);bc::Image roundtrip;
            require(bc::parse(bytes.data(),bytes.size(),source.data(),source.size(),roundtrip,error,bc::format_normal_bc7)&&same(normal,roundtrip),"Angular-certified normal format roundtrip at exact limits");
            require(!bc::parse(bytes.data(),bytes.size(),source.data(),source.size(),roundtrip,error,bc::format_linear_bc7)&&same(normal,roundtrip),"Normal and generic linear cache roles remain distinct");
            auto generic=normal;generic.format=bc::format_linear_bc7;generic.normal_max_degrees=generic.normal_rms_degrees=0;
            const auto generic_bytes=serialize(source,generic);
            require(!bc::parse(generic_bytes.data(),generic_bytes.size(),source.data(),source.size(),roundtrip,error,bc::format_normal_bc7)&&same(normal,roundtrip),"Old generic linear caches cannot certify normals");
            for(unsigned kind=0;kind<6;kind++){
                auto bad=bytes;auto flags=bc::detail::get32(bad.data()+36);
                if(kind==0)flags&=~2u;
                if(kind==1)flags|=1u<<26;
                if(kind==2)flags=(flags&~(4095u<<2))|(2049u<<2);
                if(kind==3)flags=(flags&~(4095u<<14))|(769u<<14);
                if(kind<4)put32(bad,36,flags);
                if(kind==4)put32(bad,64+36,bc::linear_error_max+1);
                if(kind==5)put_float(bad,64+(normal.levels.size()-1)*40+16,std::nextafter(bc::linear_rmse_max,2.f));
                repair_crc(bad);
                require(!bc::parse(bad.data(),bad.size(),source.data(),source.size(),roundtrip,error,bc::format_normal_bc7)&&same(normal,roundtrip),"Normal certificate, reserved bits and every mip fail atomically");
            }
            for(const float metric:{std::nextafter(2.f,3.f),-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
                auto bad=normal;bad.normal_max_degrees=metric;Bytes unchanged={7};
                require(!bc::serialize(source.data(),source.size(),bad,unchanged,error)&&unchanged==Bytes({7}),"Writer rejects invalid normal max angle");
                bad=normal;bad.normal_rms_degrees=metric;
                require(!bc::serialize(source.data(),source.size(),bad,unchanged,error)&&unchanged==Bytes({7}),"Writer rejects invalid normal RMS");
            }
            normal.normal_max_degrees=.10001f;normal.normal_rms_degrees=.00101f;
            const auto rounded=serialize(source,normal);
            require(bc::parse(rounded.data(),rounded.size(),source.data(),source.size(),roundtrip,error,bc::format_normal_bc7),"Fractional normal certificate roundtrip");
            require(roundtrip.normal_max_degrees>=normal.normal_max_degrees&&roundtrip.normal_max_degrees<normal.normal_max_degrees+1.f/1024&&roundtrip.normal_rms_degrees>=normal.normal_rms_degrees&&roundtrip.normal_rms_degrees<normal.normal_rms_degrees+1.f/1024,"Quantized angular certificates round upward conservatively");
        }
        for (const auto &dimensions : {std::pair<std::uint32_t, std::uint32_t>{1, 1}, {1, 16384}, {16384, 1}, {3, 5}}) {
            const auto candidate = image(dimensions.first, dimensions.second);
            const auto bytes = serialize(source, candidate);
            require(bc::parse(bytes.data(), bytes.size(), source.data(), source.size(), decoded, error) && same(decoded, candidate),
                    "non-square/full15-level boundary roundtrip");
        }
        auto boundary_quality = original; boundary_quality.levels[0].rmse = bc::base_rmse_max;
        for (std::size_t i = 1; i < boundary_quality.levels.size(); ++i) boundary_quality.levels[i].rmse = 9.5f;
        const auto quality_bytes = serialize(source, boundary_quality);
        require(bc::parse(quality_bytes.data(), quality_bytes.size(), source.data(), source.size(), decoded, error) &&
                same(decoded, boundary_quality), "base PSNR float boundary and independent looser tail policy");
        for (const float rmse : {std::nextafter(bc::base_rmse_max, std::numeric_limits<float>::infinity()), 9.0f}) {
            auto bytes = good; put_float(bytes, 80, rmse); repair_crc(bytes); reject(bytes, source, "base PSNR38 requirement");
        }
        auto changed_source = source; changed_source[3] ^= 0x55;
        reject(good, changed_source, "same-length original source identity mismatch");

        // Repair the checksum for structural mutations, proving those guards
        // reject independently of the checksum safety net.
        const auto mutate32 = [&](std::size_t offset, std::uint32_t value, const char *label) {
            auto bytes = good; put32(bytes, offset, value); repair_crc(bytes); reject(bytes, source, label);
        };
        const auto mutate64 = [&](std::size_t offset, std::uint64_t value, const char *label) {
            auto bytes = good; put64(bytes, offset, value); repair_crc(bytes); reject(bytes, source, label);
        };
        auto bad_magic = good; bad_magic[0] ^= 1; repair_crc(bad_magic); reject(bad_magic, source, "magic");
        for (auto offset : {8u, 12u, 16u, 20u, 36u}) mutate32(offset, 99, "unsupported version/recipe/format/header/flags");
        mutate32(24, 0, "zero width"); mutate32(28, 0, "zero height");
        mutate32(24, 16385, "dimension budget");
        auto large_area = good; put32(large_area, 24, 16384); put32(large_area, 28, 8192); repair_crc(large_area);
        reject(large_area, source, "pixel area budget");
        for (auto count : {0u, 3u, 5u, 16u, 0xffffffffu}) mutate32(32, count, "exact bounded mip count");
        mutate64(40, source.size() - 1, "encoded source length");
        mutate64(40, 0xffffffffffffffffull, "source length overflow");
        mutate64(48, good.size() + 1, "declared file length");
        mutate64(48, 0xffffffffffffffffull, "file length overflow");
        for (unsigned level = 0; level < original.levels.size(); ++level) {
            const std::size_t base = 64 + level * 40;
            mutate32(base, 0, "mip width"); mutate32(base + 4, 0, "mip height");
            mutate64(base + 8, 0, "empty block payload");
            mutate64(base + 8, 0xffffffffffffffffull, "block length overflow");
            mutate32(base + 32, 1, "nonopaque alpha certificate");
            mutate32(base + 36, 1, "reserved descriptor field");
            for (const float value : {-1.0f, 10.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
                auto bytes = good; put_float(bytes, base + 16, value); repair_crc(bytes);
                reject(bytes, source, "invalid per-mip RMSE");
            }
            for (unsigned channel = 0; channel < 3; ++channel)
                for (const float value : {-3.0f, 3.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
                    auto bytes = good; put_float(bytes, base + 20 + channel * 4, value); repair_crc(bytes);
                    reject(bytes, source, "invalid per-channel mean bias");
                }
        }
        for (const std::size_t length : {std::size_t(0), std::size_t(7), std::size_t(63), std::size_t(64), std::size_t(223),
                                        std::size_t(224), std::size_t(230), good.size() - 1}) {
            reject(Bytes(good.begin(), good.begin() + std::ptrdiff_t(length)), source, "truncation");
            if (length >= 64) {
                Bytes bytes(good.begin(), good.begin() + std::ptrdiff_t(length));
                put64(bytes, 48, length); repair_crc(bytes); reject(bytes, source, "truncated but self-consistent declared length");
            }
        }
        auto trailing = good; trailing.push_back(0); put64(trailing, 48, trailing.size()); repair_crc(trailing);
        reject(trailing, source, "trailing bytes with repaired header and checksum");
        for (const std::size_t offset : {std::size_t(56), std::size_t(80), std::size_t(227), good.size() - 1}) {
            auto bytes = good; bytes[offset] ^= 1; reject(bytes, source, "metadata/source/payload/checksum bit corruption");
        }
        std::size_t block_at = 224 + source.size();
        for (const auto &level : original.levels) {
            for (std::size_t i = 0; i < level.blocks.size(); i += 16) {
                auto bytes = good; bytes[block_at + i] = 0; repair_crc(bytes); reject(bytes, source, "reserved BC7 mode prefix");
            }
            block_at += level.blocks.size();
        }
        const std::size_t first_block = 224 + source.size();
        for (const unsigned mode : {0u, 2u, 3u, 4u, 5u, 7u}) {
            auto bytes = good; bytes[first_block] = std::uint8_t(1u << mode); repair_crc(bytes);
            reject(bytes, source, "BC7 mode outside fixed encoder recipe");
        }
        for (unsigned bit = 49; bit <= 64; ++bit) {
            auto bytes = good; bytes[first_block + bit / 8] &= std::uint8_t(~(1u << (bit % 8))); repair_crc(bytes);
            reject(bytes, source, "nonopaque mode6 endpoint or P bit");
        }
        auto sentinel = image(1, 1, 91); decoded = sentinel;
        require(!bc::parse(good.data(), bc::max_file_bytes + 1, source.data(), source.size(), decoded, error) && same(decoded, sentinel),
                "oversized file rejected before dereferencing declared extent");
        require(!bc::parse(good.data(), good.size(), source.data(), bc::max_encoded_bytes + 1, decoded, error) && same(decoded, sentinel),
                "oversized original rejected before dereferencing declared extent");
        require(!bc::parse(nullptr, good.size(), source.data(), source.size(), decoded, error), "null file pointer");
        require(!bc::parse(good.data(), good.size(), nullptr, source.size(), decoded, error), "null source pointer");
        require(!bc::parse(good.data(), good.size(), source.data(), 0, decoded, error), "empty original source");
        const auto reject_image = [&](const bc::Image &candidate, const char *label) {
            Bytes previous = {1, 2, 3}; const auto snapshot = previous;
            require(!bc::serialize(source.data(), source.size(), candidate, previous, error), std::string(label) + " serialized");
            require(previous == snapshot && !error.empty(), std::string(label) + " failed transactionality");
        };
        auto invalid = original; invalid.width = 0; reject_image(invalid, "invalid image dimensions");
        invalid = original; invalid.levels.pop_back(); reject_image(invalid, "missing 1x1 tail");
        invalid = original; invalid.levels.push_back(original.levels.back()); reject_image(invalid, "extra 1x1 tail");
        invalid = original; invalid.levels[1].width++; reject_image(invalid, "wrong mip dimensions");
        invalid = original; invalid.levels[0].blocks.pop_back(); reject_image(invalid, "partial BC7 block");
        invalid = original; invalid.levels[3].blocks[0] = 0; reject_image(invalid, "reserved block prefix");
        invalid = original; invalid.levels[2].rmse = 10; reject_image(invalid, "failed quality certificate");
        invalid = original; invalid.levels[0].rmse = std::nextafter(bc::base_rmse_max, std::numeric_limits<float>::infinity());
        reject_image(invalid, "failed base PSNR38 certificate");
        invalid = original; invalid.levels[1].alpha_max = 255; reject_image(invalid, "failed opaque certificate");
        Bytes previous = {7, 8}; const auto snapshot = previous;
        require(!bc::serialize(source.data(), bc::max_encoded_bytes + 1, original, previous, error) && previous == snapshot,
                "serialize source budget before read");
        require(!bc::serialize(nullptr, source.size(), original, previous, error) && previous == snapshot, "serialize null source");

        directory = fs::temp_directory_path() / ("valwood-block-format-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        require(fs::create_directory(directory), "create isolated fixture directory");
        const auto path = directory / "collision-key.vbc7"; const auto name = path.string();
        require(bc::write(name.c_str(), source.data(), source.size(), original, error), "atomic write: " + error);
        require(file_bytes(path) == good, "disk representation equals deterministic memory representation");
        require(bc::read(name.c_str(), source.data(), source.size(), decoded, error) && same(decoded, original), "read roundtrip");
        require(!bc::read(name.c_str(), changed_source.data(), changed_source.size(), decoded, error) && same(decoded, original),
                "lookup path/hash collision does not authorize wrong source");
        const auto alternative = image(9, 7, 39);
        require(bc::write(name.c_str(), changed_source.data(), changed_source.size(), alternative, error), "replace colliding lookup file");
        decoded = sentinel;
        require(!bc::read(name.c_str(), source.data(), source.size(), decoded, error) && same(decoded, sentinel), "full source collision rejection");
        require(bc::read(name.c_str(), changed_source.data(), changed_source.size(), decoded, error) && same(decoded, alternative),
                "colliding filename may still serve its actual original source");
        require(bc::write(name.c_str(), source.data(), source.size(), original, error), "restore original file");
        require(!bc::write(name.c_str(), source.data(), source.size(), invalid, error) && file_bytes(path) == good,
                "invalid replacement preserves prior complete file");
        require(!bc::write(directory.string().c_str(), source.data(), source.size(), original, error), "failed rename to directory");
        require_no_temps(directory);
        const auto absent = (directory / "absent" / "file.vbc7").string();
        require(!bc::write(absent.c_str(), source.data(), source.size(), original, error), "nonexistent parent write failure");
        decoded = sentinel;
        require(!bc::read(absent.c_str(), source.data(), source.size(), decoded, error) && same(decoded, sentinel), "missing file preserves output");
        require(!bc::read(nullptr, source.data(), source.size(), decoded, error), "null read path");
        require(!bc::write("", source.data(), source.size(), original, error), "empty write path");
        const auto oversized = directory / "oversized.vbc7";
        { std::ofstream sparse(oversized, std::ios::binary); sparse.put(0); }
        fs::resize_file(oversized, bc::max_file_bytes + 1);
        require(!bc::read(oversized.string().c_str(), source.data(), source.size(), decoded, error) && same(decoded, sentinel),
                "oversized sparse file rejected before allocation");

        // One writer and one reader only: rename must expose either whole
        // image, never a prefix or a mixed generation during concurrent cook.
        std::atomic<bool> writer_failed{false};
        std::thread writer([&] {
            std::string local_error;
            for (unsigned i = 0; i < 100; ++i)
                if (!bc::write(name.c_str(), source.data(), source.size(), i & 1 ? original : alternative, local_error))
                    writer_failed.store(true);
        });
        bool reads_valid = true;
        for (unsigned i = 0; i < 200; ++i) {
            bc::Image current;
            if (!bc::read(name.c_str(), source.data(), source.size(), current, error) ||
                (!same(current, original) && !same(current, alternative))) reads_valid = false;
        }
        writer.join();
        require(!writer_failed.load() && reads_valid, "atomic concurrent replacement/read");
        require_no_temps(directory);
        fs::remove_all(directory); directory.clear();
        std::cout << "BC7 container CPU PASS: " << checks
                  << " checks; source identity, CRC64, full-chain budgets, quality certificates, corruption, transactional output and atomic replacement\n";
        return 0;
    } catch (const std::exception &failure) {
        if (!directory.empty()) { std::error_code ignored; fs::remove_all(directory, ignored); }
        std::cerr << "BC7 container CPU FAIL: " << failure.what() << '\n'; return 1;
    }
}
