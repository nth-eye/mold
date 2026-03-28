#include "test_common.h"

// ---------------------------------------------------------------
// vector_t basic operations
// ---------------------------------------------------------------

TEST_CASE("vector_t default constructor")
{
    mold::vector_t<int, 8> v;
    CHECK(v.size() == 0);
    CHECK(v.empty());
    CHECK(!v.full());
    CHECK(v.capacity() == 8);
    CHECK(v.max_size() == 8);
    CHECK(v.begin() == v.end());
}

TEST_CASE("vector_t sized constructor")
{
    mold::vector_t<int, 8> v(4);
    CHECK(v.size() == 4);
    CHECK(!v.empty());
    CHECK(!v.full());
    for (size_t i = 0; i < 4; ++i)
        CHECK(v[i] == 0);
}

TEST_CASE("vector_t sized constructor with value")
{
    mold::vector_t<int, 8> v(3, 42);
    CHECK(v.size() == 3);
    for (size_t i = 0; i < 3; ++i)
        CHECK(v[i] == 42);
}

TEST_CASE("vector_t iterator constructor")
{
    int arr[] = {10, 20, 30};
    mold::vector_t<int, 8> v(std::begin(arr), std::end(arr));
    CHECK(v.size() == 3);
    CHECK(v[0] == 10);
    CHECK(v[1] == 20);
    CHECK(v[2] == 30);
}

TEST_CASE("vector_t span constructor")
{
    int arr[] = {1, 2, 3, 4};
    std::span<const int> s(arr);
    mold::vector_t<int, 8> v(s);
    CHECK(v.size() == 4);
    CHECK(v[0] == 1);
    CHECK(v[3] == 4);
}

TEST_CASE("vector_t initializer_list constructor")
{
    mold::vector_t<int, 8> v = {5, 10, 15, 20};
    CHECK(v.size() == 4);
    CHECK(v[0] == 5);
    CHECK(v[1] == 10);
    CHECK(v[2] == 15);
    CHECK(v[3] == 20);
}

TEST_CASE("vector_t copy constructor")
{
    mold::vector_t<int, 8> a = {1, 2, 3};
    mold::vector_t<int, 8> b = a;
    CHECK(b.size() == 3);
    CHECK(b[0] == 1);
    CHECK(b[1] == 2);
    CHECK(b[2] == 3);
    // Verify independent
    a[0] = 99;
    CHECK(b[0] == 1);
}

TEST_CASE("vector_t move constructor")
{
    mold::vector_t<int, 8> a = {1, 2, 3};
    mold::vector_t<int, 8> b = std::move(a);
    CHECK(b.size() == 3);
    CHECK(b[0] == 1);
    CHECK(b[1] == 2);
    CHECK(b[2] == 3);
}

TEST_CASE("vector_t copy assignment")
{
    mold::vector_t<int, 8> a = {1, 2, 3};
    mold::vector_t<int, 8> b = {9, 8, 7, 6, 5};
    b = a;
    CHECK(b.size() == 3);
    CHECK(b[0] == 1);
    CHECK(b[2] == 3);
}

TEST_CASE("vector_t move assignment")
{
    mold::vector_t<int, 8> b = {9, 8};
    b = mold::vector_t<int, 8>{1, 2, 3};
    CHECK(b.size() == 3);
    CHECK(b[0] == 1);
    CHECK(b[2] == 3);
}

TEST_CASE("vector_t initializer_list assignment")
{
    mold::vector_t<int, 8> v;
    v = {10, 20, 30};
    CHECK(v.size() == 3);
    CHECK(v[0] == 10);
    CHECK(v[2] == 30);
}

TEST_CASE("vector_t self assignment")
{
    mold::vector_t<int, 8> v = {1, 2, 3};
    auto& ref = v;
    v = ref;
    CHECK(v.size() == 3);
    CHECK(v[0] == 1);
    CHECK(v[2] == 3);
}

// ---------------------------------------------------------------
// Assign methods
// ---------------------------------------------------------------

TEST_CASE("vector_t assign(n, val)")
{
    mold::vector_t<int, 8> v = {1, 2};
    v.assign(4, 77);
    CHECK(v.size() == 4);
    for (size_t i = 0; i < 4; ++i)
        CHECK(v[i] == 77);
}

TEST_CASE("vector_t assign(iterators)")
{
    int arr[] = {100, 200, 300};
    mold::vector_t<int, 8> v = {1};
    v.assign(std::begin(arr), std::end(arr));
    CHECK(v.size() == 3);
    CHECK(v[0] == 100);
    CHECK(v[2] == 300);
}

TEST_CASE("vector_t assign(span)")
{
    int arr[] = {5, 6};
    mold::vector_t<int, 8> v;
    v.assign(std::span<const int>(arr));
    CHECK(v.size() == 2);
    CHECK(v[0] == 5);
    CHECK(v[1] == 6);
}

TEST_CASE("vector_t assign(initializer_list)")
{
    mold::vector_t<int, 8> v;
    v.assign({10, 20, 30, 40, 50});
    CHECK(v.size() == 5);
    CHECK(v[4] == 50);
}

// ---------------------------------------------------------------
// Capacity
// ---------------------------------------------------------------

TEST_CASE("vector_t full()")
{
    mold::vector_t<int, 3> v = {1, 2, 3};
    CHECK(v.full());
    CHECK(v.size() == v.capacity());
}

// ---------------------------------------------------------------
// Iterators
// ---------------------------------------------------------------

TEST_CASE("vector_t iterators")
{
    mold::vector_t<int, 8> v = {10, 20, 30};

    int sum = 0;
    for (auto it = v.begin(); it != v.end(); ++it)
        sum += *it;
    CHECK(sum == 60);

    std::vector<int> reversed(v.rbegin(), v.rend());
    CHECK(reversed.size() == 3);
    CHECK(reversed[0] == 30);
    CHECK(reversed[1] == 20);
    CHECK(reversed[2] == 10);

    CHECK(v.cbegin() == v.begin());
    CHECK(v.cend() == v.end());
}

TEST_CASE("vector_t range-for")
{
    mold::vector_t<int, 8> v = {1, 2, 3, 4};
    int sum = 0;
    for (int x : v)
        sum += x;
    CHECK(sum == 10);
}

// ---------------------------------------------------------------
// Access
// ---------------------------------------------------------------

TEST_CASE("vector_t front/back")
{
    mold::vector_t<int, 8> v = {10, 20, 30};
    CHECK(v.front() == 10);
    CHECK(v.back() == 30);

    v.front() = 99;
    v.back() = 88;
    CHECK(v[0] == 99);
    CHECK(v[2] == 88);
}

TEST_CASE("vector_t data()")
{
    mold::vector_t<int, 8> v = {1, 2, 3};
    CHECK(v.data() == v.begin());
    CHECK(v.data()[1] == 2);
}

// ---------------------------------------------------------------
// Modifiers
// ---------------------------------------------------------------

TEST_CASE("vector_t push_back")
{
    mold::vector_t<int, 8> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    CHECK(v.size() == 3);
    CHECK(v[0] == 1);
    CHECK(v[1] == 2);
    CHECK(v[2] == 3);
}

TEST_CASE("vector_t emplace_back")
{
    mold::vector_t<int, 8> v;
    auto& ref = v.emplace_back(42);
    CHECK(ref == 42);
    CHECK(v.size() == 1);
    CHECK(v[0] == 42);
}

TEST_CASE("vector_t pop_back")
{
    mold::vector_t<int, 8> v = {1, 2, 3};
    v.pop_back();
    CHECK(v.size() == 2);
    CHECK(v.back() == 2);
    v.pop_back();
    v.pop_back();
    CHECK(v.empty());
}

TEST_CASE("vector_t clear")
{
    mold::vector_t<int, 8> v = {1, 2, 3, 4, 5};
    v.clear();
    CHECK(v.empty());
    CHECK(v.size() == 0);
}

TEST_CASE("vector_t resize grow")
{
    mold::vector_t<int, 8> v;
    v.resize(4);
    CHECK(v.size() == 4);
    for (size_t i = 0; i < 4; ++i)
        CHECK(v[i] == 0);
}

TEST_CASE("vector_t resize grow with value")
{
    mold::vector_t<int, 8> v = {1, 2};
    v.resize(5, 99);
    CHECK(v.size() == 5);
    CHECK(v[0] == 1);
    CHECK(v[1] == 2);
    CHECK(v[2] == 99);
    CHECK(v[3] == 99);
    CHECK(v[4] == 99);
}

TEST_CASE("vector_t resize shrink")
{
    mold::vector_t<int, 8> v = {1, 2, 3, 4, 5};
    v.resize(2);
    CHECK(v.size() == 2);
    CHECK(v[0] == 1);
    CHECK(v[1] == 2);
}

TEST_CASE("vector_t insert single at beginning")
{
    mold::vector_t<int, 8> v = {2, 3, 4};
    auto it = v.insert(v.begin(), 1);
    CHECK(*it == 1);
    CHECK(v.size() == 4);
    CHECK(v[0] == 1);
    CHECK(v[1] == 2);
    CHECK(v[2] == 3);
    CHECK(v[3] == 4);
}

TEST_CASE("vector_t insert single at end")
{
    mold::vector_t<int, 8> v = {1, 2, 3};
    auto it = v.insert(v.end(), 4);
    CHECK(*it == 4);
    CHECK(v.size() == 4);
    CHECK(v[3] == 4);
}

TEST_CASE("vector_t insert single in middle")
{
    mold::vector_t<int, 8> v = {1, 3, 4};
    auto it = v.insert(v.begin() + 1, 2);
    CHECK(*it == 2);
    CHECK(v.size() == 4);
    CHECK(v[0] == 1);
    CHECK(v[1] == 2);
    CHECK(v[2] == 3);
    CHECK(v[3] == 4);
}

TEST_CASE("vector_t insert N copies")
{
    mold::vector_t<int, 8> v = {1, 4};
    v.insert(v.begin() + 1, 2, 99);
    CHECK(v.size() == 4);
    CHECK(v[0] == 1);
    CHECK(v[1] == 99);
    CHECK(v[2] == 99);
    CHECK(v[3] == 4);
}

TEST_CASE("vector_t insert range")
{
    int arr[] = {20, 30};
    mold::vector_t<int, 8> v = {10, 40};
    v.insert(v.begin() + 1, std::begin(arr), std::end(arr));
    CHECK(v.size() == 4);
    CHECK(v[0] == 10);
    CHECK(v[1] == 20);
    CHECK(v[2] == 30);
    CHECK(v[3] == 40);
}

TEST_CASE("vector_t insert initializer_list")
{
    mold::vector_t<int, 8> v = {1, 5};
    v.insert(v.begin() + 1, {2, 3, 4});
    CHECK(v.size() == 5);
    CHECK(v[0] == 1);
    CHECK(v[1] == 2);
    CHECK(v[2] == 3);
    CHECK(v[3] == 4);
    CHECK(v[4] == 5);
}

TEST_CASE("vector_t insert zero elements")
{
    mold::vector_t<int, 8> v = {1, 2, 3};
    auto it = v.insert(v.begin() + 1, 0, 99);
    CHECK(it == v.begin() + 1);
    CHECK(v.size() == 3);
}

TEST_CASE("vector_t erase single")
{
    mold::vector_t<int, 8> v = {1, 2, 3, 4, 5};

    SUBCASE("erase beginning") {
        auto it = v.erase(v.begin());
        CHECK(*it == 2);
        CHECK(v.size() == 4);
        CHECK(v[0] == 2);
    }
    SUBCASE("erase end") {
        auto it = v.erase(v.end() - 1);
        CHECK(it == v.end());
        CHECK(v.size() == 4);
        CHECK(v.back() == 4);
    }
    SUBCASE("erase middle") {
        auto it = v.erase(v.begin() + 2);
        CHECK(*it == 4);
        CHECK(v.size() == 4);
        CHECK(v[0] == 1);
        CHECK(v[1] == 2);
        CHECK(v[2] == 4);
        CHECK(v[3] == 5);
    }
}

TEST_CASE("vector_t erase range")
{
    mold::vector_t<int, 8> v = {1, 2, 3, 4, 5};

    SUBCASE("erase beginning range") {
        v.erase(v.begin(), v.begin() + 2);
        CHECK(v.size() == 3);
        CHECK(v[0] == 3);
    }
    SUBCASE("erase end range") {
        v.erase(v.end() - 2, v.end());
        CHECK(v.size() == 3);
        CHECK(v.back() == 3);
    }
    SUBCASE("erase middle range") {
        v.erase(v.begin() + 1, v.begin() + 3);
        CHECK(v.size() == 3);
        CHECK(v[0] == 1);
        CHECK(v[1] == 4);
        CHECK(v[2] == 5);
    }
    SUBCASE("erase all") {
        v.erase(v.begin(), v.end());
        CHECK(v.empty());
    }
}

TEST_CASE("vector_t pop_idx")
{
    mold::vector_t<int, 8> v = {10, 20, 30, 40, 50};

    SUBCASE("pop middle - swaps with last") {
        v.pop_idx(1);
        CHECK(v.size() == 4);
        CHECK(v[0] == 10);
        CHECK(v[1] == 50);
        CHECK(v[2] == 30);
        CHECK(v[3] == 40);
    }
    SUBCASE("pop first - swaps with last") {
        v.pop_idx(0);
        CHECK(v.size() == 4);
        CHECK(v[0] == 50);
    }
    SUBCASE("pop last - no swap needed") {
        v.pop_idx(4);
        CHECK(v.size() == 4);
        CHECK(v[3] == 40);
    }
    SUBCASE("pop single element") {
        mold::vector_t<int, 4> s = {99};
        s.pop_idx(0);
        CHECK(s.empty());
    }
}

TEST_CASE("vector_t pop iterator")
{
    mold::vector_t<int, 8> v = {10, 20, 30, 40, 50};
    v.pop(v.begin() + 2);
    CHECK(v.size() == 4);
    CHECK(v[0] == 10);
    CHECK(v[1] == 20);
    CHECK(v[2] == 50);
    CHECK(v[3] == 40);
}

TEST_CASE("vector_t pop range")
{
    mold::vector_t<int, 8> v = {1, 2, 3, 4, 5, 6, 7, 8};

    SUBCASE("pop middle range - tail fills gap") {
        v.pop(v.begin() + 1, v.begin() + 4);
        CHECK(v.size() == 5);
        CHECK(v[0] == 1);
        CHECK(v[1] == 6);
        CHECK(v[2] == 7);
        CHECK(v[3] == 8);
        CHECK(v[4] == 5);
    }
    SUBCASE("pop from end") {
        v.pop(v.end() - 3, v.end());
        CHECK(v.size() == 5);
        CHECK(v[4] == 5);
    }
    SUBCASE("pop from beginning") {
        v.pop(v.begin(), v.begin() + 2);
        CHECK(v.size() == 6);
        CHECK(v[0] == 7);
        CHECK(v[1] == 8);
    }
    SUBCASE("pop all") {
        v.pop(v.begin(), v.end());
        CHECK(v.empty());
    }
    SUBCASE("pop more than remaining tail") {
        mold::vector_t<int, 8> w = {1, 2, 3, 4, 5};
        w.pop(w.begin() + 1, w.begin() + 4);
        CHECK(w.size() == 2);
        CHECK(w[0] == 1);
        CHECK(w[1] == 5);
    }
}

TEST_CASE("vector_t pop with non-trivial type")
{
    mold::vector_t<std::string, 8> v;
    v.push_back("alpha");
    v.push_back("beta");
    v.push_back("gamma");
    v.push_back("delta");

    SUBCASE("pop_idx") {
        v.pop_idx(1);
        CHECK(v.size() == 3);
        CHECK(v[0] == "alpha");
        CHECK(v[1] == "delta");
        CHECK(v[2] == "gamma");
    }
    SUBCASE("pop range") {
        v.pop(v.begin(), v.begin() + 2);
        CHECK(v.size() == 2);
        CHECK(v[0] == "gamma");
        CHECK(v[1] == "delta");
    }
}

TEST_CASE("vector_t swap")
{
    mold::vector_t<int, 8> a = {1, 2};
    mold::vector_t<int, 8> b = {3, 4, 5};
    a.swap(b);
    CHECK(a.size() == 3);
    CHECK(a[0] == 3);
    CHECK(b.size() == 2);
    CHECK(b[0] == 1);
}

TEST_CASE("vector_t equality")
{
    mold::vector_t<int, 8> a = {1, 2, 3};
    mold::vector_t<int, 8> b = {1, 2, 3};
    mold::vector_t<int, 8> c = {1, 2, 4};
    mold::vector_t<int, 8> d = {1, 2};
    CHECK(a == b);
    CHECK(!(a == c));
    CHECK(!(a == d));
}

TEST_CASE("vector_t spaceship operator")
{
    mold::vector_t<int, 8> a = {1, 2, 3};
    mold::vector_t<int, 8> b = {1, 2, 3};
    mold::vector_t<int, 8> c = {1, 2, 4};
    mold::vector_t<int, 8> d = {1, 2};
    mold::vector_t<int, 8> e = {1, 2, 3, 4};

    CHECK((a <=> b) == std::strong_ordering::equal);
    CHECK(a < c);
    CHECK(c > a);
    CHECK(a > d);
    CHECK(d < a);
    CHECK(a < e);
    CHECK(a <= b);
    CHECK(a >= b);
    CHECK(!(a < b));
    CHECK(!(a > b));
}

TEST_CASE("vector_t ADL swap")
{
    mold::vector_t<int, 8> a = {1, 2};
    mold::vector_t<int, 8> b = {3, 4, 5};
    using std::swap;
    swap(a, b);
    CHECK(a.size() == 3);
    CHECK(a[0] == 3);
    CHECK(b.size() == 2);
    CHECK(b[0] == 1);
}

// ---------------------------------------------------------------
// Non-trivial element type
// ---------------------------------------------------------------

TEST_CASE("vector_t with std::string elements")
{
    mold::vector_t<std::string, 4> v;
    v.push_back("hello");
    v.push_back("world");
    v.emplace_back("foo");
    CHECK(v.size() == 3);
    CHECK(v[0] == "hello");
    CHECK(v[1] == "world");
    CHECK(v[2] == "foo");

    v.erase(v.begin() + 1);
    CHECK(v.size() == 2);
    CHECK(v[0] == "hello");
    CHECK(v[1] == "foo");

    v.clear();
    CHECK(v.empty());
}

TEST_CASE("vector_t with std::string copy/move")
{
    mold::vector_t<std::string, 4> a;
    a.push_back("alpha");
    a.push_back("beta");

    mold::vector_t<std::string, 4> b = a;
    CHECK(b[0] == "alpha");
    CHECK(b[1] == "beta");

    mold::vector_t<std::string, 4> c = std::move(a);
    CHECK(c[0] == "alpha");
    CHECK(c[1] == "beta");
}

// ---------------------------------------------------------------
// Serialization round-trips
// ---------------------------------------------------------------

struct vec_holder_t {
    mold::vector_t<int, 16> ints;
    mold::vector_t<std::string, 8> strings;
};

TEST_CASE("vector_t JSON round-trip")
{
    vec_holder_t original = {
        .ints = {1, 2, 3, 4, 5},
        .strings = {"hello", "world", "foo"},
    };

    vec_holder_t decoded = {};
    REQUIRE(json_roundtrip(original, decoded) == mold::error_t::ok);
    CHECK(decoded.ints.size() == 5);
    CHECK(decoded.ints[0] == 1);
    CHECK(decoded.ints[4] == 5);
    CHECK(decoded.strings.size() == 3);
    CHECK(decoded.strings[0] == "hello");
    CHECK(decoded.strings[2] == "foo");
}

TEST_CASE("vector_t CBOR round-trip")
{
    vec_holder_t original = {
        .ints = {10, 20, 30},
        .strings = {"cbor", "test"},
    };

    vec_holder_t decoded = {};
    REQUIRE(cbor_roundtrip(original, decoded) == mold::error_t::ok);
    CHECK(decoded.ints.size() == 3);
    CHECK(decoded.ints[0] == 10);
    CHECK(decoded.ints[2] == 30);
    CHECK(decoded.strings.size() == 2);
    CHECK(decoded.strings[0] == "cbor");
    CHECK(decoded.strings[1] == "test");
}

TEST_CASE("vector_t MessagePack round-trip")
{
    vec_holder_t original = {
        .ints = {100, 200, 300, 400},
        .strings = {"msgpack"},
    };

    vec_holder_t decoded = {};
    REQUIRE(msgpack_roundtrip(original, decoded) == mold::error_t::ok);
    CHECK(decoded.ints.size() == 4);
    CHECK(decoded.ints[0] == 100);
    CHECK(decoded.ints[3] == 400);
    CHECK(decoded.strings.size() == 1);
    CHECK(decoded.strings[0] == "msgpack");
}

TEST_CASE("vector_t empty round-trip")
{
    vec_holder_t original = {};

    vec_holder_t json_decoded = {};
    REQUIRE(json_roundtrip(original, json_decoded) == mold::error_t::ok);
    CHECK(json_decoded.ints.empty());
    CHECK(json_decoded.strings.empty());

    vec_holder_t cbor_decoded = {};
    REQUIRE(cbor_roundtrip(original, cbor_decoded) == mold::error_t::ok);
    CHECK(cbor_decoded.ints.empty());
    CHECK(cbor_decoded.strings.empty());

    vec_holder_t msgpack_decoded = {};
    REQUIRE(msgpack_roundtrip(original, msgpack_decoded) == mold::error_t::ok);
    CHECK(msgpack_decoded.ints.empty());
    CHECK(msgpack_decoded.strings.empty());
}

struct cap_holder_t { mold::vector_t<int, 4> v; };

TEST_CASE("vector_t full capacity round-trip")
{
    mold::vector_t<int, 4> original = {1, 2, 3, 4};
    CHECK(original.full());

    cap_holder_t in = { .v = original };

    cap_holder_t json_out = {};
    REQUIRE(json_roundtrip(in, json_out) == mold::error_t::ok);
    CHECK(json_out.v == original);

    cap_holder_t cbor_out = {};
    REQUIRE(cbor_roundtrip(in, cbor_out) == mold::error_t::ok);
    CHECK(cbor_out.v == original);

    cap_holder_t msgpack_out = {};
    REQUIRE(msgpack_roundtrip(in, msgpack_out) == mold::error_t::ok);
    CHECK(msgpack_out.v == original);
}

// ---------------------------------------------------------------
// Overflow protection in deserialization
// ---------------------------------------------------------------

struct small_cap_holder_t { mold::vector_t<int, 3> v; };

TEST_CASE("vector_t JSON overflow silently stops at capacity")
{
    // JSON array with 5 elements decoded into vector_t<int, 3>
    static const char json[] = R"({"v": [1, 2, 3, 4, 5]})";
    std::string_view sv = json;

    small_cap_holder_t decoded = {};
    (void)mold::json_to(decoded, sv);
    // prepare() returns nullptr when full, parser should stop
    CHECK(decoded.v.size() <= 3);
}

// ---------------------------------------------------------------
// Nested vector_t
// ---------------------------------------------------------------

struct nested_vec_t {
    mold::vector_t<mold::vector_t<int, 4>, 3> matrix;
};

TEST_CASE("vector_t nested JSON round-trip")
{
    nested_vec_t original;
    original.matrix.push_back({1, 2, 3});
    original.matrix.push_back({4, 5});
    original.matrix.push_back({6});

    nested_vec_t decoded = {};
    REQUIRE(json_roundtrip(original, decoded) == mold::error_t::ok);
    CHECK(decoded.matrix.size() == 3);
    CHECK(decoded.matrix[0].size() == 3);
    CHECK(decoded.matrix[0][0] == 1);
    CHECK(decoded.matrix[0][2] == 3);
    CHECK(decoded.matrix[1].size() == 2);
    CHECK(decoded.matrix[1][0] == 4);
    CHECK(decoded.matrix[2].size() == 1);
    CHECK(decoded.matrix[2][0] == 6);
}

TEST_CASE("vector_t nested CBOR round-trip")
{
    nested_vec_t original;
    original.matrix.push_back({10, 20});
    original.matrix.push_back({30});

    nested_vec_t decoded = {};
    REQUIRE(cbor_roundtrip(original, decoded) == mold::error_t::ok);
    CHECK(decoded.matrix.size() == 2);
    CHECK(decoded.matrix[0] == mold::vector_t<int, 4>{10, 20});
    CHECK(decoded.matrix[1] == mold::vector_t<int, 4>{30});
}

// ---------------------------------------------------------------
// vector_t with struct elements
// ---------------------------------------------------------------

struct point_t {
    int x;
    int y;
};

struct polyline_t {
    mold::vector_t<point_t, 8> points;
};

TEST_CASE("vector_t of structs JSON round-trip")
{
    polyline_t original = {
        .points = {{.x = 0, .y = 0}, {.x = 10, .y = 20}, {.x = 30, .y = 40}},
    };

    polyline_t decoded = {};
    REQUIRE(json_roundtrip(original, decoded) == mold::error_t::ok);
    CHECK(decoded.points.size() == 3);
    CHECK(decoded.points[0].x == 0);
    CHECK(decoded.points[0].y == 0);
    CHECK(decoded.points[1].x == 10);
    CHECK(decoded.points[1].y == 20);
    CHECK(decoded.points[2].x == 30);
    CHECK(decoded.points[2].y == 40);
}

TEST_CASE("vector_t of structs CBOR round-trip")
{
    polyline_t original = {
        .points = {{.x = 1, .y = 2}, {.x = 3, .y = 4}},
    };

    polyline_t decoded = {};
    REQUIRE(cbor_roundtrip(original, decoded) == mold::error_t::ok);
    CHECK(decoded.points.size() == 2);
    CHECK(decoded.points[0].x == 1);
    CHECK(decoded.points[1].y == 4);
}

TEST_CASE("vector_t of structs MessagePack round-trip")
{
    polyline_t original = {
        .points = {{.x = 5, .y = 6}},
    };

    polyline_t decoded = {};
    REQUIRE(msgpack_roundtrip(original, decoded) == mold::error_t::ok);
    CHECK(decoded.points.size() == 1);
    CHECK(decoded.points[0].x == 5);
    CHECK(decoded.points[0].y == 6);
}

// ---------------------------------------------------------------
// vector_t with optional elements
// ---------------------------------------------------------------

struct opt_vec_t {
    std::optional<mold::vector_t<int, 8>> values;
};

TEST_CASE("optional<vector_t> JSON round-trip")
{
    SUBCASE("present") {
        opt_vec_t original = { .values = mold::vector_t<int, 8>{1, 2, 3} };
        opt_vec_t decoded = {};
        REQUIRE(json_roundtrip(original, decoded) == mold::error_t::ok);
        REQUIRE(decoded.values.has_value());
        CHECK(decoded.values->size() == 3);
        CHECK((*decoded.values)[0] == 1);
    }
    SUBCASE("missing") {
        // When optional container field is missing from JSON, it stays nullopt
        static const char json[] = R"({})";
        std::string_view sv = json;
        opt_vec_t decoded = {};
        mold::json_to(decoded, sv);
        CHECK(!decoded.values.has_value());
    }
}

// ---------------------------------------------------------------
// Emplace in middle
// ---------------------------------------------------------------

TEST_CASE("vector_t emplace in middle")
{
    mold::vector_t<int, 8> v = {1, 3, 4};
    auto it = v.emplace(v.begin() + 1, 2);
    CHECK(*it == 2);
    CHECK(v.size() == 4);
    CHECK(v[0] == 1);
    CHECK(v[1] == 2);
    CHECK(v[2] == 3);
    CHECK(v[3] == 4);
}

TEST_CASE("vector_t emplace at end")
{
    mold::vector_t<int, 8> v = {1, 2};
    auto it = v.emplace(v.end(), 3);
    CHECK(*it == 3);
    CHECK(v.size() == 3);
    CHECK(v[2] == 3);
}

TEST_CASE("vector_t emplace at beginning")
{
    mold::vector_t<int, 8> v = {2, 3};
    v.emplace(v.begin(), 1);
    CHECK(v.size() == 3);
    CHECK(v[0] == 1);
    CHECK(v[1] == 2);
    CHECK(v[2] == 3);
}
