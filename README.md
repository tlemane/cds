# cds - bit-packed containers & compact data structures

`cds` is a header-only **C++23** library of **bit-packed containers** and
**compact `rank` / `select`** data structures: packed integer vectors, bit
vectors, `rank`/`select` indexes, and compressed sequences.

[![CI](https://github.com/tlemane/cds/actions/workflows/ci.yml/badge.svg)](https://github.com/tlemane/cds/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/tlemane/cds)](https://github.com/tlemane/cds/releases/latest)
![platforms](https://img.shields.io/badge/platforms-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)
![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)
![header-only](https://img.shields.io/badge/header--only-yes-brightgreen.svg)
[![License: MIT](https://img.shields.io/github/license/tlemane/cds)](LICENSE)

> [!NOTE]
> **Early stage.** The API and the serialization format may
> change, and there may be rough edges. Bug reports, feedback, and suggestions
> are very welcome.

## Design

A few choices shape the library.

- **Static by default, polymorphic on demand.** Structures are compile-time
  templates with no virtual dispatch or dependencies. An optional adapter layer
  wraps any of them behind a common interface when runtime dispatch is needed.

- **Configurable layout.** The word type, bit order (`lsb` / `msb`), packing mode
  (`dense` / `sparse` / `none`), and element width (1–64 bits) are all parameters,
  fixed at compile time or left to runtime at some optimization cost.

- **Container-like API.** `packed_vector` / `bit_vector` offer the usual
  `operator[]`, assignment, and iterators over packed storage. The proxy
  references and iterators compile down to plain shifts and masks.

- **Indexes over any bit source.** Rank and select operate over any type modelling
  the `bit_source` concept: `bit_vector`, `bit_array`, a view over mapped words,
  or your own.

- **Owning and viewable.** Each structure has an owning type and a zero-copy
  `_view`, and serializes to a versioned format (in-memory buffer, file, or
  `mmap`) that the view can bind to without copying.


---

## Table of contents

- [Content](#content)
- [Requirements](#requirements)
- [Quick start](#quick-start)
- [Examples](#examples)
- [Core design](#core-design)
- [Serialization](#serialization)
- [Tests & benchmarks](#tests--benchmarks)
- [Continuous integration](#continuous-integration)
- [Performance](#performance)

---

## Content

### Packed & bit containers

- **`packed_vector`**: a growable, `std::vector`-like sequence of unsigned integers
  at a chosen bit width (1 to 64). The width is fixed at compile time or set at
  runtime, and the layout (endianness, dense vs. sparse) is selectable.
- **`packed_array`**: same as `packed_vector` but with compile-time capacity, no heap
  allocation.
- **`bit_vector` / `bit_array`**: the 1-bit case, with `operator[]`, `push_back`,
  and `popcount`.
- **`const_packed_view`, `bit_view` / `const_bit_view`**: non-owning views

### Rank indexes

`rank1(i)` counts the set bits in `[0, i)`, and `rank0(i) = i − rank1(i)`.

- **`rank9`**: constant-time queries at about 25 % space overhead. The
  fast default. (*Vigna, “Broadword Implementation of Rank/Select Queries”, WEA 2008.*)
- **`rank_poppy`**: constant-time queries at about 3/4 % overhead. (*Zhou, Andersen & Kaminsky, “Space-Efficient, High-Performance Rank & Select
  Structures on Uncompressed Bit Sequences”, SEA 2013.*)
- **`rank_scan`**: a linear scan.

### Select indexes

`select1(r)` / `select0(r)` return the position of the `r`-th one / zero
(0-indexed). A `select_target` (`ones`, `zeros`, or `both`) sets which sides are
built, `both` roughly doubles a one-sided index.

- **`darray`**: the fastest select in `cds`, and `ef` default (*Okanohara & Sadakane, “Practical Entropy-Compressed Rank/Select Dictionary”, ALENEX 2007.*)
- **`select9`**: built on a `rank9`, fast.
- **`select_poppy`**: built on a `rank_poppy`. Small, but slower than `darray` / `select9`. (*Zhou, Andersen & Kaminsky, “Space-Efficient, High-Performance Rank & Select Structures on Uncompressed Bit Sequences”, SEA 2013.*)
- **`select_scan`**: a linear scan.

### Compressed sequences

- **`ef`**: EF representation of a non-decreasing integer sequence. Fully indexable: random access, rank, successor / predecessor, locate, and a fast sequential decoder. The select index is a template
  parameter, default is `darray`. (*Elias, “Efficient Storage and Retrieval by Content and Address of Static Files”, JACM 1974; Fano, “On the Number of Bits Required to Implement an Associative Memory”, MIT Project MAC 1971; Vigna, “Quasi-Succinct Indices”, WSDM 2013.*)
- **`rrr`**: RRR compressed bit vector. (*Raman, Raman & Rao, “Succinct Indexable Dictionaries with Applications to
  Encoding k-ary Trees and Multisets”, SODA 2002.*)

### Sequences over an alphabet

- **`wavelet_matrix`**: `access` / `rank` / `select` over a sequence of arbitrary
  integer symbols, as `log σ` bit-dictionary levels. The level type is a template
  parameter (default `bit_dict`; use `rrr` for compressed levels). Alphabet width
  is compile-time or inferred at build. (*Claude & Navarro, “The Wavelet Matrix”, SPIRE 2012.*)
- **`bit_dict`**: a bit vector bundled with rank + select over its own bits (a
  `bit_dictionary`). The default `wavelet_matrix` level type, useful standalone.

---

## Requirements

- A **C++23** compiler
- **CMake ≥ 3.28** to consume it as a package (optional).
- No third-party dependencies. (The benchmarks optionally fetch `xxsds/sdsl-lite` for comparison.)

CI builds and tests on **Linux** (x86-64 and ARM64) with **GCC 14** and **Clang 19**
(libc++), on **macOS** (Apple Silicon) with **Apple Clang**, and on **Windows** with
**MSVC (Visual Studio 17 2022)**.


### CMake

`cds` exposes the interface target **`cds::cds`**.

```cmake
# Vendored / submodule:
add_subdirectory(external/cds)
target_link_libraries(myapp PRIVATE cds::cds)
```

```cmake
# FetchContent:
include(FetchContent)
FetchContent_Declare(cds
    GIT_REPOSITORY https://github.com/tlemane/cds.git
    GIT_TAG        main)
FetchContent_MakeAvailable(cds)
target_link_libraries(myapp PRIVATE cds::cds)
```

Linking `cds::cds` sets the C++23 requirement and the include path.

## Quick start

```cpp
#include <cds/bit/vector.hpp>
#include <cds/rank/rank9.hpp>
#include <cds/select/darray.hpp>
#include <cds/ef.hpp>
#include <cds/rrr.hpp>

#include <cstdint>
#include <vector>

using u64 = std::uint64_t;
using bv_t = cds::bit_vector<u64, cds::pack_endian::lsb, cds::pack_mode::dense>;

int main() {
    // ---- bit vector ----
    bv_t bv;
    for (int i = 0; i < 1000; ++i) {
        bv.push_back(i % 3 == 0);
    }

    bool b    = bv[42];         // random access
    auto ones = bv.popcount();  // whole-vector popcount

    // ---- rank / select ----
    cds::rank9<bv_t> rank(bv);
    std::size_t r = rank.rank1(300);

    cds::darray<bv_t, cds::select_target::both> sel(bv);
    std::size_t p1 = sel.select1(10);   // position of the 10th one  (0-indexed)
    std::size_t p0 = sel.select0(10);   // position of the 10th zero

    // ---- Elias–Fano ----
    std::vector<u64> values = {1, 3, 3, 7, 12, 30, 31};
    cds::ef<> ef(values);
    u64  v    = ef[4];             // -> 12
    auto succ = ef.nge(8);         // next value >= 8  -> {pos=4, val=12}
    auto pred = ef.ple(8);         // prev value <= 8  -> {pos=3, val=7}

    for (u64 x : ef) { /* fast sequential decode */
        (void)x;
    }

    // ---- RRR ----
    cds::rrr<> comp(bv);
    std::size_t cr = comp.rank1(300);
    std::size_t cp = comp.select1(10);
}
```

---

## Examples

The [`examples/`](examples/) directory holds standalone, runnable programs, each
a single `main()` with commented feature walkthroughs. They build with
`-DCDS_BUILD_EXAMPLES=ON` (as `example_<name>`), or directly:

```bash
c++ -std=c++23 -Iinclude examples/00_bit_vector.cpp -o 00 && ./00
```

- [`00_bit_vector.cpp`](examples/00_bit_vector.cpp): `bit_vector` push_back, `operator[]`, `popcount`, mutation.
- [`01_packed_vector.cpp`](examples/01_packed_vector.cpp): `packed_vector` / `packed_array`, compile-time & runtime width, the `unsafe` write path, literals.
- [`02_rank.cpp`](examples/02_rank.cpp): `rank9` / `rank_poppy` / `rank_scan` over a bit source.
- [`03_select.cpp`](examples/03_select.cpp): `darray` / `select9` / `select_poppy` / `select_scan`, and the rank/select composition.
- [`04_ef.cpp`](examples/04_ef.cpp): Elias–Fano access, `rank`, `nge` / `ple`, `locate`, `diff`, iteration, `from_deltas`.
- [`05_rrr.cpp`](examples/05_rrr.cpp): RRR rank + select in one, compression vs. raw, `BlockSize` trade-off.
- [`06_serialization.cpp`](examples/06_serialization.cpp): save / load over a buffer (owning and zero-copy view), a file, and an `mmap`.
- [`07_wavelet_matrix.cpp`](examples/07_wavelet_matrix.cpp): `wavelet_matrix` access / rank / select over an integer alphabet, plain and `rrr`-compressed levels.

Advanced:

- [`adv_interface.cpp`](examples/adv_interface.cpp): runtime polymorphism via `rank_adapter` behind `rank_interface`.
- [`adv_as_view.cpp`](examples/adv_as_view.cpp): `as_view()` sharing one owning backend across several zero-copy views / interfaces.
- [`adv_rank_over_select.cpp`](examples/adv_rank_over_select.cpp): `rank_backed_select` composing rank + select, plugged into `ef` as a smaller select index.

---

## Core design

### Layout: endianness & packing mode

Packed and bit containers are parameterized by two enums (in
`cds/core/packed/type.hpp`):

```cpp
enum class pack_endian : std::uint8_t { msb, lsb, rt }; // bit order within a word
enum class pack_mode   : std::uint8_t { none, sparse, dense, rt };
```

- **`pack_endian`**:
  - `lsb`
  - `msb`
  - `rt`: runtime selection.

- **`pack_mode`**:
  - `none`: one element per machine word (a plain native array, no bit math).
  - `dense`: elements packed back-to-back, straddling word boundaries (no wasted
    bits).
  - `sparse`: `floor(word_bits / width)` elements per word, never straddles
    (wastes the remainder, but each access touches a single word).
  - `rt`: runtime selection.

### The `bit_source` concept

Rank and select structures are **not** tied to one container. They accept any
type modelling `cds::bit_source` (declared in `cds/bit/interface.hpp`): a type
exposing its backing words via `bit_source_traits`:

```cpp
template <typename T>
concept bit_source = /* T provides word_type, data(), size(), offset(), endian */;
```

`bit_vector`, `bit_array`, `bit_view`, and `const_bit_view` all model it, so the
same `rank9<Source>` works over an owning vector, a fixed array, or a
non-owning view. **A rank/select index binds a pointer into its source's words:
the source must outlive the index.**

### Owning vs. view

Every indexed structure comes in two flavours:

| owning | zero-copy view |
| --- | --- |
| `packed_vector` | `packed_view` / `const_packed_view` |
| `bit_vector` | `bit_view` / `const_bit_view` |
| `rank9` | `rank9_view` |
| `rank_poppy` | `rank_poppy_view` |
| `select9` | `select9_view` |
| `select_poppy` | `select_poppy_view` |
| `darray` | `darray_view` |
| `ef` | `ef_view` |
| `rrr` | `rrr_view` |
| `bit_dict` | `bit_dict_view` |
| `wavelet_matrix` | `wavelet_matrix_view` |

The owning type builds and holds its arrays. The `_view` binds, without copying,
into a buffer you supply (e.g. an `mmap`'d file). Both answer the same
queries. See [Serialization](#serialization--persistence).

Every owning structure also has an **`as_view()`** method returning its `_view`
bound to that object's own in-memory storage, with no serialization and no copy.
The view is a lightweight handle (spans and pointers), so one backend can cheaply
feed many consumers at once, for example a `rank_interface` and a
`select_interface` sharing a single compressed `rrr`. The backend must outlive
any view taken from it. See [`adv_as_view.cpp`](examples/adv_as_view.cpp).

### The `unsafe` fast write path

Packed writes normally clear the target bits. When you
are writing into memory that is **known to be zero** (e.g. freshly reserved
storage, which is zero-filled), you can skip the clear with `cds::unsafe`:

```cpp
cds::packed_vector<u64, u64, 12, cds::pack_endian::lsb, cds::pack_mode::dense> v;
v.reserve(n);
for (u64 x : data)
    v.push_back(cds::unsafe(x));
```

User-defined literals are also available:
```cpp
using namespace cds::literals;
v[index] = 1_c1; // std::uint8_t
v[index] = 1_c2; // std::uint16_t
v[index] = 1_c4; // std::uint32_t
v[index] = 1_c8; // std::uint64_t
```

### Concepts

The library ships concepts you can constrain against (and which the structures
satisfy):

- `cds::rank1_structure`, `rank0_structure`, `rank_structure` (`cds/rank/concepts.hpp`)
- `cds::select1_structure`, `select0_structure` (`cds/select/concepts.hpp`)
- `cds::bit_source` (`cds/bit/interface.hpp`)
- `cds::io::byte_sink`, `byte_source`, `span_source`, `mutable_span_source`
  (`cds/io/byte.hpp`)

For example `rrr`, `darray`, `rank9`, … all model the rank/select concepts, so
they are interchangeable wherever a generic `rank1_structure` /
`select1_structure` is expected.

### Runtime polymorphism

Every structure is a static, zero-overhead template with no virtual calls. When
you need to choose an implementation at runtime, hold heterogeneous structures
together, or hide the concrete type behind an ABI boundary, each family also ships
an abstract interface and a generic adapter that wraps any conforming structure
behind it (`cds/*/interface.hpp`):

| family | interface(s) | adapter(s) |
| --- | --- | --- |
| rank | `rank_interface` (`rank1`/`rank0`/`size`) | `rank_adapter<T>` |
| select | `select1_interface`, `select0_interface` | `select1_adapter<T>`, `select0_adapter<T>` |
| packed | `packed_const_interface<V>`, `packed_interface<V>`, `packed_dynamic_interface<V>` | `const_packed_adapter<T>`, `packed_adapter<T>`, `packed_dynamic_adapter<T>` |
| bit | `bit_const_interface`, `bit_interface`, `bit_dynamic_interface` | `const_bit_adapter<T>`, `bit_adapter<T>`, `bit_dynamic_adapter<T>` |

The adapter forwards constructor arguments to the wrapped type and answers the
interface's virtual methods by delegating to it, so any concrete structure is
usable behind one pointer type:

```cpp
#include <cds/rank/interface.hpp>
#include <memory>

// choose the rank implementation at runtime, behind a single pointer type:
std::unique_ptr<cds::rank_interface> idx =
    use_compressed
        ? std::unique_ptr<cds::rank_interface>{std::make_unique<cds::rank_adapter<cds::rrr<>>>(bv)}
        : std::unique_ptr<cds::rank_interface>{std::make_unique<cds::rank_adapter<cds::rank9<bv_t>>>(bv)};

std::size_t r = idx->rank1(300);   // one virtual call, whichever impl was chosen
```

Adapters are `final` and non-copyable/non-movable. The usual lifetime rule still applies
to source-backed indexes: a `rank_adapter<rank9<…>>` keeps a pointer into its `bit_source`, while a
self-contained `rank_adapter<rrr<…>>` copies the data and has no such coupling.

---

## Serialization

Every indexed structure (`packed_vector`, `bit_vector`, the rank/select indexes,
`ef`, `rrr`, …) serializes through a minimal byte abstraction in
`cds/io/byte.hpp`:

```cpp
concept byte_sink   = /* s.write(ptr, n) -> bool */;
concept byte_source = /* s.read(ptr, n)  -> bool */;
concept span_source = byte_source && /* s.view(n) -> span<const std::byte> */;
```

Each blob starts with a small **version header** so loads are checked for compatibility.

The layout is **alignment-aware**. Every header is sized to a multiple of 8 bytes, and each
variable-length section (the packed word arrays, and `darray`'s `int64` / `uint64`
inventories) is padded so the next one starts on an 8-byte boundary. A `_view` therefore
binds directly onto the bytes `save()` wrote, whether that is a memory buffer or an `mmap`,
and reads every word in place through a correctly aligned pointer: no copy, no misaligned
access. The only requirement is that the buffer or mapping you pass to `load()` starts
8-byte aligned, which `mmap` pages and standard allocators already satisfy. CI runs the
suite under UBSan, which enforces this.

### Back-ends (`cds/io/`)

- **`buffer_sink` / `buffer_source`**: in-memory `std::vector<std::byte>` /
  `std::span<const std::byte>`. `buffer_source` is a `span_source` (zero-copy).
- **`file_sink` / `file_source`**: stream to/from a file.
- **`mmap_file`** (`cds/io/mmap.hpp`): memory-map a file. Its `view()` gives a
  `span` you wrap in a `buffer_source` to load a `_view` in place. Portable across
  Linux / macOS / Windows.

## Tests & benchmarks

`cds` itself is header-only. The CMake project can additionally build the
tooling when it is the top-level project. Options (defaults shown):


| option | default | effect |
| --- | --- | --- |
| `CDS_BUILD_TESTS` | ON (top-level) | doctest unit tests under `tests/` |
| `CDS_BUILD_BENCHMARKS` | ON (top-level) | nanobench benchmarks under `benchmarks/` |
| `CDS_INSTALL` | ON (top-level) | install rules + `cds` package config |
| `CDS_ENABLE_SANITIZERS` | OFF | ASan + UBSan on the tests |
| `CDS_WARNINGS_AS_ERRORS` | OFF | `-Werror` in dev builds |


```bash
cmake -S . -B build
cmake --build build
```

### Tests

```bash
ctest --test-dir build
```

### Benchmarks

#### `cds-perf`

The `cds-perf` binary tries to give you a performance summary of each
`cds` datastructure on **your machine**: `throughput`, `ns/op`, `instructions/op`, `memory`, etc.

```bash
./build/benchmarks/cds-perf
```

#### Microbenchmarks

- `bench_packed`: `packed_vector` operations.
- `bench_bv`: `bit_vector` operations.
- `bench_rank`: rank structures (`rank9`, `rank_poppy`, `rank_scan`).
- `bench_select`: select structures (`darray`, `select9`, `select_poppy`).
- `bench_ef`: Elias–Fano (`ef`).
- `bench_sdsl_packed`: `packed_vector` vs `sdsl::int_vector`.
- `bench_sdsl_bv`: `bit_vector` vs `sdsl::bit_vector`.
- `bench_sdsl_ef`: `ef` vs `sdsl::sd_vector`.
- `bench_sdsl_rrr`: `rrr` vs `sdsl::rrr_vector`.

## Continuous integration

Every push to `main` and every pull request runs the
[CI workflow](.github/workflows/ci.yml). Tests
run through CTest (doctest) and every example is compiled.

| Job | OS | Toolchain | Build types |
| --- | --- | --- | --- |
| Linux | Ubuntu 24.04, x86-64 **and** ARM64 | GCC 14, Clang 19 (libc++) | Debug + Release |
| macOS | macOS 15 (Apple Silicon) | Apple Clang (libc++) | Release |
| Windows | Windows Server 2022 | MSVC (Visual Studio 17 2022) | Release |
| Sanitizers (ASan/UBSan) | Ubuntu 24.04 | GCC 14 | RelWithDebInfo |

- **Debug and Release both build and test.**
- **Sanitizers** run the whole suite under AddressSanitizer and UndefinedBehaviorSanitizer,
  including strict alignment checks. UBSan is what a zero-copy `_view` has to satisfy: its
  words are `reinterpret_cast` in place, so every serialized section is padded to an 8-byte
  boundary and the sanitizer job guards that invariant.

## Performance

Numbers below are from one `cds-perf` run (see [Tests & benchmarks](#tests--benchmarks)),
each structure against its closest `sdsl-lite` equivalent. This is a snapshot from a single
machine, not a spec: run `cds-perf` yourself for numbers that apply to your hardware. Every
`x` figure is `cds / sdsl` throughput, so `> 1.00x` means cds is faster.

**Setup.** Intel Core Ultra 7 165H (L1d 48 KiB, L2 2 MiB, L3 24 MiB), GCC 15.3 at
`-O3 -march=native` (Release), C++23. Cells report throughput
(`M/s` = million ops/second, `G/s` = billion), `ns/op`, and the `cds/sdsl` ratio.
Rank and select also show `inst/op` (retired instructions per operation).
`packed_vector`, `bit_vector`, `ef`, `rrr` and `wavelet_matrix` use `n = 1,000,000`. `rank` and
`select` are shown at two working-set sizes, **cached** (index over 200,000 bits, ~24 KiB,
L1-resident) and **at-scale** (1,000,000 bits, ~122 KiB, L2), because throughput depends on
where the structure lives.

### packed_vector vs `sdsl::int_vector<W>`

`ns/op` and `x` for width `W = 20`, `n = 1,000,000`. `dense` packs `W` bits exactly (same
footprint as `int_vector`). `sparse` packs `floor(64/W)` values per word (faster, some waste
when `W` does not divide 64). `_u` is the `unsafe` write path into zeroed memory.

| op | sdsl `int_vector<20>` | lsb/dense | lsb/sparse | msb/sparse |
| --- | --- | --- | --- | --- |
| build | 318.8 M/s (3.14 ns) | 302.5 M/s (3.31 ns, 0.95x) | 450.7 M/s (2.22 ns, 1.41x) | 383.3 M/s (2.61 ns, 1.20x) |
| random read | 156.8 M/s (6.38 ns) | 176.6 M/s (5.66 ns, 1.13x) | 410.2 M/s (2.44 ns, 2.62x) | 300.2 M/s (3.33 ns, 1.91x) |
| random write | 86.3 M/s (11.6 ns) | 123.5 M/s (8.10 ns, 1.43x) | 274.0 M/s (3.65 ns, 3.18x) | 221.7 M/s (4.51 ns, 2.57x) |
| random write `_u` | 86.3 M/s (11.6 ns) | 175.8 M/s (5.69 ns, 2.04x) | 256.5 M/s (3.90 ns, 2.97x) | 213.4 M/s (4.69 ns, 2.47x) |
| sequential scan | 388.5 M/s (2.57 ns) | 500.5 M/s (2.00 ns, 1.29x) | 949.8 M/s (1.05 ns, 2.45x) | 747.0 M/s (1.34 ns, 1.92x) |

**Memory** (bits per element, lower is better):

| width | sdsl `int_vector` | lsb/dense | lsb/sparse |
| --- | --- | --- | --- |
| W4 | 4.0 | 4.0 (1.00x) | 4.0 (1.00x) |
| W12 | 12.0 | 12.0 (1.00x) | 12.8 (+6.7%) |
| W17 | 17.0 | 17.0 (1.00x) | 21.33 (+25%) |
| W20 | 20.0 | 20.0 (1.00x) | 21.33 (+6.7%) |

`dense` matches `int_vector` exactly. `sparse` packs `floor(64/W)` values per 64-bit word and
leaves the remainder unused, so its footprint depends on how many values fit rather than on
`W` directly.

### bit_vector vs `sdsl::bit_vector`

`ns/op` and `x`, `n = 1,000,000`, lsb (msb is within a few percent).

| op | sdsl | cds | x |
| --- | --- | --- | --- |
| build | 125.7 M/s (7.96 ns) | 330.3 M/s (3.03 ns) | **2.63x** |
| random read | 892.2 M/s (1.12 ns) | 867.2 M/s (1.15 ns) | 0.97x |
| random write | 122.8 M/s (8.14 ns) | 625.2 M/s (1.60 ns) | **5.09x** |
| sequential scan | 435.8 M/s (2.29 ns) | 887.0 M/s (1.13 ns) | **2.04x** |
| popcount | 86.9 G/s | 84.6 G/s | 0.97x |

**Memory**: 1.00 bit/element (122.07 KiB for `n = 1,000,000`), identical to `sdsl::bit_vector`.

### rank vs `sdsl::rank_support_v`

`rank1` query. Instruction counts are density independent, as expected.

Each cell is throughput `(ns/op, inst/op[, x])`.

| config | sdsl `rank_support_v` | `rank9` | `rank_poppy` |
| --- | --- | --- | --- |
| lsb, cached (200k) | 261.9 M/s (3.82 ns, 37.9) | 402.6 M/s (2.48 ns, 24.0, **1.54x**) | 172.0 M/s (5.82 ns, 65.0, 0.66x) |
| lsb, at-scale (1M) | 251.2 M/s (3.98 ns, 37.9) | 359.8 M/s (2.78 ns, 24.0, **1.43x**) | 43.6 M/s (23.0 ns, 65.2, 0.17x) |
| msb, at-scale (1M) | 239.0 M/s (4.18 ns, 37.9) | 300.8 M/s (3.32 ns, 28.9, **1.26x**) | 43.8 M/s (22.9 ns, 70.2, 0.18x) |

**Memory** (index size as a fraction of the source bitmap, at-scale, lower is better):

| structure | overhead | index bytes |
| --- | --- | --- |
| sdsl `rank_support_v` | 25.0% | 30.54 KiB |
| `rank9` | 25.1% | 30.59 KiB |
| `rank_poppy` | 3.2% | 3.91 KiB |


### select vs `sdsl::select_support_mcl`

Single query, `target=both` (each structure answers select1 **and** select0). `darray` is a
standalone select structure. `select9`/`select_poppy` reuse a rank index and search it.

`sdsl` and `darray` cells are throughput `(ns/op, inst/op[, x])`. The last column is
`select9 / select_poppy` throughput and ratio.

| target / density | sdsl `select_support_mcl` | `darray` | `select9` / `select_poppy` |
| --- | --- | --- | --- |
| select1, 50% | 29.8 M/s (33.5 ns, 103) | 72.0 M/s (13.9 ns, 33.9, **2.41x**) | 12.1 M/s (0.41x) / 15.5 M/s (0.52x) |
| select0, 50% | 30.7 M/s (32.6 ns, 104) | 71.6 M/s (14.0 ns, 35.0, **2.33x**) | 12.4 M/s (0.40x) / 12.9 M/s (0.42x) |
| select1, 5% ones | 17.8 M/s (56.2 ns, 165) | 37.0 M/s (27.0 ns, 61.4, **2.08x**) | 7.7 M/s (0.43x) / 9.1 M/s (0.51x) |
| select0, 5% zeros | 30.7 M/s (32.5 ns, 99) | 107.2 M/s (9.33 ns, 32.8, **3.49x**) | 14.6 M/s (0.48x) / 13.7 M/s (0.44x) |

**Memory** (index size as a fraction of the source, both-sided, including any rank dependency):

| structure | 50% | 5% |
| --- | --- | --- |
| sdsl `select_support_mcl` | 39.6% (48.34 KiB) | 39.0% (47.66 KiB) |
| `darray` | 56.4% (68.85 KiB) | 56.4% (68.84 KiB) |
| `select9` | 25.9% (31.63 KiB) | 25.9% (31.62 KiB) |
| `select_poppy` | 3.7% (4.56 KiB) | 3.7% (4.56 KiB) |


### ef (Elias-Fano) vs `sdsl::sd_vector`

`n = 1,000,000`, sparse case (universe 11x n). `ef` here is parameterized by its internal
select index (`darray` / `poppy` / `select9`).

`sdsl` and `ef`+darray cells are throughput `(ns/op[, x])`. The compact-index columns show
throughput and ratio.

| op | sdsl `sd_vector` | `ef`+darray | `ef`+poppy | `ef`+select9 |
| --- | --- | --- | --- | --- |
| random read | 25.7 M/s (38.9 ns) | 55.4 M/s (18.0 ns, **2.16x**) | 14.9 M/s (0.58x) | 12.1 M/s (0.47x) |
| `nge` (successor) | 8.4 M/s (119.3 ns) | 26.9 M/s (37.1 ns, **3.21x**) | 10.5 M/s (**1.25x**) | 10.4 M/s (**1.24x**) |
| `ple` (predecessor) | 8.2 M/s (122.1 ns) | 25.4 M/s (39.4 ns, **3.10x**) | 10.1 M/s (**1.24x**) | 10.0 M/s (**1.22x**) |
| sequential decode | 73.6 M/s (13.6 ns) | 251.0 M/s (3.98 ns, **3.41x**) | 290.5 M/s (**3.95x**) | 230.7 M/s (**3.14x**) |

**Memory** (bits per element, lower is better):

| index | sparse (universe 11x n) | dense (universe 3x n) |
| --- | --- | --- |
| sdsl `sd_vector` | 6.60 | 4.60 |
| `ef`+darray | 6.62 (1.00x) | 4.52 (1.02x) |
| `ef`+poppy | 5.40 (1.22x smaller) | 3.33 (1.38x smaller) |
| `ef`+select9 | 5.91 (1.12x smaller) | 3.83 (1.20x smaller) |


### rrr (compressed bitmap) vs `sdsl::rrr_vector`

`n = 1,000,000`, `ns/op` and x, block size `BS`.

| config | op | sdsl `rrr_vector` | cds `rrr` | x |
| --- | --- | --- | --- | --- |
| BS63, 50% | access | 2.5 M/s (403.3 ns) | 3.0 M/s (334.1 ns) | **1.21x** |
| BS63, 50% | rank1 | 2.5 M/s (403.6 ns) | 3.0 M/s (333.5 ns) | **1.21x** |
| BS63, 50% | select1 | 1.9 M/s (521.3 ns) | 2.4 M/s (417.6 ns) | **1.25x** |
| BS15, 50% | rank1 | 5.9 M/s (170.7 ns) | 6.8 M/s (146.0 ns) | **1.17x** |
| BS15, 5% | rank1 | 11.7 M/s (85.6 ns) | 12.9 M/s (77.5 ns) | **1.10x** |
| BS15, 5% | access | 18.8 M/s (53.2 ns) | 17.1 M/s (58.6 ns) | 0.91x |
| BS15, 5% | select1 | 5.0 M/s (201.9 ns) | 3.7 M/s (271.3 ns) | 0.74x |

**Memory** (compressed size in bits per stored bit, lower is better). cds and sdsl are identical in memory usage.


| block size | 5% ones | 50% ones |
| --- | --- | --- |
| BS15 | 0.52 | 1.18 |
| BS31 | 0.42 | 1.11 |
| BS63 | 0.36 | 1.06 |


### wavelet_matrix vs `sdsl::wm_int`

`n = 1,000,000` symbols. The wavelet's levels are a `bit_dictionary`, so the rank/select
backend is swappable: `r9+darray` (the default `bit_dict`), `r9+select9` (shares the rank9
index), and `poppy+selpop` (shares a poppy index, most compact). Table is the 8-bit alphabet.

| op | sdsl `wm_int` | `r9+darray` | `r9+select9` | `poppy+selpop` |
| --- | --- | --- | --- | --- |
| build | 5.3 M/s (189.7 ns) | 9.2 M/s (108.8 ns, **1.74x**) | 11.4 M/s (88.1 ns, **2.15x**) | 9.4 M/s (106.2 ns, **1.79x**) |
| access | 6.3 M/s (158.4 ns) | 6.2 M/s (161.6 ns, 0.98x) | 6.1 M/s (0.96x) | 3.4 M/s (0.54x) |
| rank | 8.6 M/s (115.7 ns) | 8.8 M/s (113.0 ns, **1.02x**) | 8.9 M/s (**1.03x**) | 2.4 M/s (0.28x) |
| select | 1.5 M/s (684.8 ns) | 2.5 M/s (404.0 ns, **1.69x**) | 1.1 M/s (0.79x) | 1.2 M/s (0.85x) |

**Memory** (bits per symbol, lower is better, `x` is `sdsl / cds`, so `> 1` means cds smaller):

| alphabet | sdsl `wm_int` | `r9+darray` | `r9+select9` | `poppy+selpop` |
| --- | --- | --- | --- | --- |
| DNA 2-bit | 3.14 | 3.63 (0.86x) | 2.52 (**1.25x**) | 2.07 (**1.51x**) |
| 8-bit | 12.05 | 14.50 (0.83x) | 10.07 (**1.20x**) | 8.29 (**1.45x**) |
| 16-bit | 23.92 | 29.01 (0.82x) | 20.13 (**1.19x**) | 16.57 (**1.44x**) |


