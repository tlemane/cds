#include <cds/bit/vector.hpp>
#include <cds/ef.hpp>
#include <cds/rank/rank9.hpp>

#include <cds/io/buffer.hpp>
#include <cds/io/file.hpp>
#include <cds/io/mmap.hpp>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <vector>

using u64 = std::uint64_t;
using bv_t = cds::bit_vector<u64, cds::pack_endian::lsb>;

int main() {
    const std::vector<u64> values = {1, 3, 3, 7, 12, 30, 31, 100, 101, 250};
    cds::ef<> ef(values);

    // (A) In-memory buffer: save to bytes, then load two ways.
    cds::io::buffer_sink sink;
    if (!ef.save(sink)) {
        std::cerr << "save failed\n";
        return 1;
    }
    const std::vector<std::byte> bytes = sink.release();
    std::cout << "(A) serialized ef into " << bytes.size() << " bytes\n";

    // (A.1) Load an OWNING ef. It copies the bytes out of the source and owns
    // its arrays. load() returns std::expected<ef, io::load_error>.
    {
        cds::io::buffer_source src(bytes);
        auto loaded = cds::ef<>::load(src);
        if (!loaded) {
            std::cerr << "owning load failed\n";
            return 1;
        }
        std::cout << "    owning ef: size=" << loaded->size() << " ef[4]=" << (*loaded)[4] << '\n';
    }

    // (A.2) Load a zero-copy ef_view. It BINDS into `bytes` without copying,
    // so `bytes` must outlive the view.
    {
        cds::io::buffer_source src(bytes);
        auto view = cds::ef_view<>::load(src);
        if (!view) {
            std::cerr << "view load failed\n";
            return 1;
        }
        auto nge = view->nge(8);
        std::cout << "    ef_view : size=" << view->size() << " nge(8)={" << nge.pos << ","
                  << nge.val << "}\n";
    }

    // (B) File: stream the same blob to disk and read an owning ef back.
    // file_sink / file_source model byte_sink / byte_source directly, so
    // save()/load() work over them with no intermediate buffer.
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "cds_example_ef.bin";

    {
        auto fsink = cds::io::file_sink::open(path.string());
        if (!fsink || !ef.save(*fsink)) {
            std::cerr << "file save failed\n";
            return 1;
        }
    } // file_sink closes the fd on scope exit

    {
        auto fsrc = cds::io::file_source::open(path.string());
        if (!fsrc) {
            std::cerr << "file open failed\n";
            return 1;
        }
        auto loaded = cds::ef<>::load(*fsrc);
        if (!loaded) {
            std::cerr << "file load failed\n";
            return 1;
        }
        std::cout << "(B) loaded ef from file: size=" << loaded->size()
                  << " back=" << loaded->back() << '\n';
    }

    // (C) mmap: map the file and bind an ef_view on the mapped
    // bytes
    {
        auto mapped = cds::io::immap<std::byte>::open_whole_file(path.string());
        if (!mapped) {
            std::cerr << "mmap failed\n";
            return 1;
        }
        // The mapped region already looks like a std::span<const std::byte>,
        // which is exactly what buffer_source wraps.
        cds::io::buffer_source src(mapped->view());
        auto view = cds::ef_view<>::load(src);
        if (!view) {
            std::cerr << "mmap view load failed\n";
            return 1;
        }
        std::cout << "(C) ef_view over mmap: size=" << view->size() << " ef[8]=" << (*view)[8]
                  << " ple(200)={" << view->ple(200).pos << "," << view->ple(200).val << "}\n";
    } // unmapped on scope exit

    std::filesystem::remove(path);

    // A note on non-owning indexes (rank9, darray, ...): they do not store the
    // bits, only an index over them. Their load() therefore takes the SAME bit
    // source the index was built over. You reload/keep the bits yourself, then
    // reattach the index. (ef and rrr own their data)
    {
        bv_t bv;
        bv.reserve(256);
        for (int i = 0; i < 256; ++i)
            bv.push_back(i % 5 == 0);
        cds::rank9<bv_t> r9(bv);

        cds::io::buffer_sink rs;
        (void)r9.save(rs);
        auto rbytes = rs.release();

        cds::io::buffer_source rsrc(rbytes);
        auto r9_loaded = cds::rank9<bv_t>::load(rsrc, bv); // source passed in
        if (r9_loaded)
            std::cout << "(D) reloaded rank9 over its source: rank1(100)=" << r9_loaded->rank1(100)
                      << '\n';
    }

    return 0;
}
